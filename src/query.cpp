#include "query.hpp"
#include <iostream>
#include <algorithm>
#include <queue>
#include <unordered_set>
#include <sstream>

namespace {

std::string json_esc(const std::string& s) {
    std::ostringstream o;
    for (char c : s) {
        if (c == '"') o << "\\\"";
        else if (c == '\\') o << "\\\\";
        else if (c == '\n') o << "\\n";
        else if (c == '\r') o << "\\r";
        else if (c == '\t') o << "\\t";
        else o << c;
    }
    return o.str();
}

std::vector<const Node*> find_matching_nodes(const Graph& graph, const std::string& sym) {
    std::vector<const Node*> exact, partial;
    for (const auto& pair : graph.nodes) {
        const auto& n = pair.second;
        if (n.id == sym || n.name == sym) exact.push_back(&n);
        else if (n.name.find(sym) != std::string::npos || n.file_path.find(sym) != std::string::npos) partial.push_back(&n);
    }
    return !exact.empty() ? exact : partial;
}

} // namespace

int QueryEngine::execute(const std::string& command,
                       const std::string& arg,
                       const Graph& graph,
                       const std::vector<CodeIssue>& issues,
                       double health_score) {
    if (command == "summary") {
        std::unordered_map<std::string, int> by_type;
        for (const auto& iss : issues) by_type[iss.type]++;

        std::vector<const Node*> mods;
        for (const auto& pair : graph.nodes) {
            if (pair.second.type == "module") mods.push_back(&pair.second);
        }
        std::sort(mods.begin(), mods.end(), [](const Node* a, const Node* b) { return a->pagerank > b->pagerank; });

        std::cout << "{\n"
                  << "  \"health_score\": " << health_score << ",\n"
                  << "  \"total_nodes\": " << graph.nodes.size() << ",\n"
                  << "  \"total_edges\": " << graph.edges.size() << ",\n"
                  << "  \"total_issues\": " << issues.size() << ",\n"
                  << "  \"issues_by_type\": {";
        size_t ti = 0;
        for (const auto& kv : by_type) {
            std::cout << "\"" << json_esc(kv.first) << "\": " << kv.second << (++ti < by_type.size() ? ", " : "");
        }
        std::cout << "},\n  \"top_modules\": [\n";
        for (size_t i = 0; i < std::min<size_t>(mods.size(), 8); ++i) {
            const Node* m = mods[i];
            std::cout << "    {\"name\": \"" << json_esc(m->name) << "\", \"pagerank\": " << m->pagerank
                      << ", \"in_degree\": " << m->in_degree << ", \"out_degree\": " << m->out_degree
                      << ", \"instability\": " << m->instability << "}"
                      << (i + 1 < std::min<size_t>(mods.size(), 8) ? "," : "") << "\n";
        }
        std::cout << "  ]\n}\n";
        return 0;
    }

    if (command == "blast-radius") {
        if (arg.empty()) {
            std::cout << "{\"error\": \"missing symbol argument\"}\n";
            return 1;
        }
        auto matches = find_matching_nodes(graph, arg);
        std::cout << "{\"query\": \"" << json_esc(arg) << "\", \"matches\": [\n";
        for (size_t m = 0; m < std::min<size_t>(matches.size(), 5); ++m) {
            const Node* n = matches[m];
            std::unordered_set<std::string> visited{n->id};
            std::queue<std::string> q;
            q.push(n->id);
            std::vector<std::string> impacted;

            while (!q.empty()) {
                std::string curr = q.front();
                q.pop();
                auto it = graph.dep_rev_adj.find(curr);
                if (it != graph.dep_rev_adj.end()) {
                    for (const auto& nxt : it->second) {
                        if (visited.insert(nxt).second) {
                            q.push(nxt);
                            if (impacted.size() < 40) impacted.push_back(nxt);
                        }
                    }
                }
            }

            std::cout << "  {\"id\": \"" << json_esc(n->id) << "\", \"type\": \"" << json_esc(n->type)
                      << "\", \"scope\": \"" << json_esc(n->scope) << "\", \"in_degree\": " << n->in_degree
                      << ", \"out_degree\": " << n->out_degree << ", \"blast_radius\": " << n->blast_radius
                      << ", \"impacted_sample\": [";
            for (size_t i = 0; i < impacted.size(); ++i) {
                std::cout << "\"" << json_esc(impacted[i]) << "\"" << (i + 1 < impacted.size() ? ", " : "");
            }
            std::cout << "]}" << (m + 1 < std::min<size_t>(matches.size(), 5) ? "," : "") << "\n";
        }
        std::cout << "]}\n";
        return 0;
    }

    if (command == "clones-of") {
        if (arg.empty()) {
            std::cout << "{\"error\": \"missing symbol argument\"}\n";
            return 1;
        }
        std::vector<const CodeIssue*> matched;
        for (const auto& iss : issues) {
            if (iss.type != "CODE_DUPLICATION" && iss.type != "ABSTRACTION_SPRAWL") continue;
            bool hit = (iss.title.find(arg) != std::string::npos || iss.description.find(arg) != std::string::npos);
            if (!hit) {
                for (const auto& nid : iss.involved_nodes) {
                    if (nid.find(arg) != std::string::npos) { hit = true; break; }
                }
            }
            if (hit) matched.push_back(&iss);
        }
        std::cout << "{\"query\": \"" << json_esc(arg) << "\", \"clones\": [\n";
        for (size_t i = 0; i < matched.size(); ++i) {
            const auto* iss = matched[i];
            std::cout << "  {\"id\": \"" << json_esc(iss->id) << "\", \"type\": \"" << json_esc(iss->type)
                      << "\", \"title\": \"" << json_esc(iss->title) << "\", \"description\": \"" << json_esc(iss->description) << "\"}"
                      << (i + 1 < matched.size() ? "," : "") << "\n";
        }
        std::cout << "]}\n";
        return 0;
    }

    if (command == "cycles") {
        std::vector<const CodeIssue*> matched;
        for (const auto& iss : issues) {
            if (iss.type != "CIRCULAR_DEPENDENCY") continue;
            if (arg.empty() || iss.description.find(arg) != std::string::npos) {
                matched.push_back(&iss);
            }
        }
        std::cout << "{\"cycles\": [\n";
        for (size_t i = 0; i < matched.size(); ++i) {
            const auto* iss = matched[i];
            std::cout << "  {\"id\": \"" << json_esc(iss->id) << "\", \"title\": \"" << json_esc(iss->title)
                      << "\", \"chain\": \"" << json_esc(iss->description) << "\"}"
                      << (i + 1 < matched.size() ? "," : "") << "\n";
        }
        std::cout << "]}\n";
        return 0;
    }

    if (command == "hotspots") {
        size_t limit = 15;
        if (!arg.empty()) {
            try { limit = static_cast<size_t>(std::max(1, std::stoi(arg))); } catch (...) {}
        }
        std::vector<const Node*> nodes;
        for (const auto& pair : graph.nodes) {
            if (pair.second.type == "file" || pair.second.type == "function") nodes.push_back(&pair.second);
        }
        std::sort(nodes.begin(), nodes.end(), [](const Node* a, const Node* b) {
            return a->hotspot_score > b->hotspot_score;
        });
        std::cout << "{\"hotspots\": [\n";
        for (size_t i = 0; i < std::min(nodes.size(), limit); ++i) {
            const Node* n = nodes[i];
            std::cout << "  {\"id\": \"" << json_esc(n->id) << "\", \"type\": \"" << json_esc(n->type)
                      << "\", \"hotspot_score\": " << n->hotspot_score
                      << ", \"runtime_heat\": " << n->runtime_heat
                      << ", \"pagerank\": " << n->pagerank
                      << ", \"complexity\": " << n->cyclomatic_complexity
                      << ", \"blast_radius\": " << n->blast_radius << "}"
                      << (i + 1 < std::min(nodes.size(), limit) ? "," : "") << "\n";
        }
        std::cout << "]}\n";
        return 0;
    }

    if (command == "module") {
        if (arg.empty()) {
            std::cout << "{\"error\": \"missing module name\"}\n";
            return 1;
        }
        std::string mod_id = (arg.rfind("module:", 0) == 0) ? arg : ("module:" + arg);
        if (!graph.has_node(mod_id)) {
            std::cout << "{\"error\": \"module not found\", \"module\": \"" << json_esc(arg) << "\"}\n";
            return 1;
        }
        const Node& m = graph.nodes.at(mod_id);
        std::vector<std::string> files;
        for (const auto& pair : graph.nodes) {
            if (pair.second.type == "file" && pair.second.module == m.name) files.push_back(pair.second.file_path);
        }
        std::cout << "{\n  \"module\": \"" << json_esc(m.name) << "\",\n"
                  << "  \"in_degree\": " << m.in_degree << ", \"out_degree\": " << m.out_degree
                  << ", \"instability\": " << m.instability << ", \"pagerank\": " << m.pagerank << ",\n"
                  << "  \"files_count\": " << files.size() << ",\n  \"files\": [";
        for (size_t i = 0; i < std::min<size_t>(files.size(), 50); ++i) {
            std::cout << "\"" << json_esc(files[i]) << "\"" << (i + 1 < std::min<size_t>(files.size(), 50) ? ", " : "");
        }
        std::cout << "]\n}\n";
        return 0;
    }

    std::cout << "{\"error\": \"unknown query command\", \"supported\": [\"summary\", \"blast-radius\", \"clones-of\", \"cycles\", \"hotspots\", \"module\"]}\n";
    return 1;
}
