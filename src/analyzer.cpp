#include "analyzer.hpp"
#include <cmath>
#include <algorithm>
#include <stack>
#include <queue>
#include <sstream>
#include <iostream>
#include <iomanip>

namespace {

double cosine_sparse_double(const std::unordered_map<std::string, double>& v1, double norm1,
                            const std::unordered_map<std::string, double>& v2, double norm2) {
    if (norm1 <= 0.0 || norm2 <= 0.0) return 0.0;
    const auto& small_v = (v1.size() < v2.size()) ? v1 : v2;
    const auto& large_v = (v1.size() < v2.size()) ? v2 : v1;
    double dot = 0.0;
    for (const auto& p : small_v) {
        auto it = large_v.find(p.first);
        if (it != large_v.end()) {
            dot += p.second * it->second;
        }
    }
    return dot / (norm1 * norm2);
}

double cosine_sparse_int(const std::unordered_map<std::string, int>& v1, double norm1,
                         const std::unordered_map<std::string, int>& v2, double norm2) {
    if (norm1 <= 0.0 || norm2 <= 0.0) return 0.0;
    const auto& small_v = (v1.size() < v2.size()) ? v1 : v2;
    const auto& large_v = (v1.size() < v2.size()) ? v2 : v1;
    double dot = 0.0;
    for (const auto& p : small_v) {
        auto it = large_v.find(p.first);
        if (it != large_v.end()) {
            dot += static_cast<double>(p.second) * it->second;
        }
    }
    return dot / (norm1 * norm2);
}

double cosine_behavior(const std::array<double, 8>& a, const std::array<double, 8>& b) {
    double dot = 0.0, ma = 0.0, mb = 0.0;
    for (size_t i = 0; i < 8; ++i) {
        dot += a[i] * b[i];
        ma += a[i] * a[i];
        mb += b[i] * b[i];
    }
    if (ma <= 0.0 || mb <= 0.0) return 0.0;
    return dot / (std::sqrt(ma) * std::sqrt(mb));
}

} // namespace

void ArchitectureAnalyzer::calculate_coupling_and_instability(Graph& graph) {
    for (auto& pair : graph.nodes) {
        pair.second.in_degree = 0;
        pair.second.out_degree = 0;
    }
    for (const auto& edge : graph.edges) {
        if (edge.type == "contains") continue; // Architectural coupling shouldn't count parent containment
        if (graph.has_node(edge.source)) {
            graph.nodes[edge.source].out_degree++;
        }
        if (graph.has_node(edge.target)) {
            graph.nodes[edge.target].in_degree++;
        }
    }
    for (auto& pair : graph.nodes) {
        int ca = pair.second.in_degree;
        int ce = pair.second.out_degree;
        if (ca + ce == 0) {
            pair.second.instability = 0.0;
        } else {
            pair.second.instability = static_cast<double>(ce) / (ca + ce);
        }
    }
}

void ArchitectureAnalyzer::calculate_blast_radius(Graph& graph) {
    // Build reverse dependency adjacency excluding "contains"
    std::unordered_map<std::string, std::vector<std::string>> dep_rev;
    for (const auto& e : graph.edges) {
        if (e.type == "imports" || e.type == "calls") {
            dep_rev[e.target].push_back(e.source);
        }
    }

    for (auto& pair : graph.nodes) {
        const std::string& start = pair.first;
        if (pair.second.type == "module") {
            pair.second.blast_radius = 0;
            continue;
        }
        std::unordered_set<std::string> visited;
        std::queue<std::string> q;
        visited.insert(start);
        q.push(start);

        while (!q.empty()) {
            std::string curr = q.front();
            q.pop();
            auto it = dep_rev.find(curr);
            if (it != dep_rev.end()) {
                for (const auto& nxt : it->second) {
                    if (visited.insert(nxt).second) {
                        q.push(nxt);
                    }
                }
            }
        }
        pair.second.blast_radius = static_cast<int>(visited.size()) - 1;
    }
}

void ArchitectureAnalyzer::calculate_pagerank(Graph& graph, double damping, int iterations) {
    size_t n = graph.nodes.size();
    if (n == 0) return;

    double initial_pr = 1.0 / n;
    for (auto& pair : graph.nodes) {
        pair.second.pagerank = initial_pr;
    }

    for (int it = 0; it < iterations; ++it) {
        std::unordered_map<std::string, double> new_pr;
        double dangling_sum = 0.0;

        for (const auto& pair : graph.nodes) {
            if (pair.second.out_degree == 0) {
                dangling_sum += pair.second.pagerank;
            }
        }

        for (const auto& pair : graph.nodes) {
            const std::string& u = pair.first;
            double incoming = 0.0;
            auto rev_it = graph.rev_adj.find(u);
            if (rev_it != graph.rev_adj.end()) {
                for (const auto& v : rev_it->second) {
                    if (graph.has_node(v) && graph.nodes[v].out_degree > 0) {
                        incoming += graph.nodes[v].pagerank / graph.nodes[v].out_degree;
                    }
                }
            }
            new_pr[u] = (1.0 - damping) / n + damping * (incoming + dangling_sum / n);
        }

        for (auto& pair : graph.nodes) {
            pair.second.pagerank = new_pr[pair.first];
        }
    }
}

std::vector<std::string> ArchitectureAnalyzer::find_shortest_cycle(const std::string& start_node, const std::vector<std::string>& scc, const Graph& graph) {
    std::unordered_set<std::string> scc_set(scc.begin(), scc.end());
    std::queue<std::vector<std::string>> q;

    auto it = graph.adj.find(start_node);
    if (it != graph.adj.end()) {
        for (const auto& nxt : it->second) {
            if (scc_set.find(nxt) != scc_set.end()) {
                if (nxt == start_node) {
                    return {start_node, start_node};
                }
                q.push({start_node, nxt});
            }
        }
    }

    std::unordered_set<std::string> visited;
    visited.insert(start_node);

    while (!q.empty()) {
        auto path = q.front();
        q.pop();

        if (path.size() > 6) continue;
        const std::string& curr = path.back();

        auto curr_it = graph.adj.find(curr);
        if (curr_it != graph.adj.end()) {
            for (const auto& nxt : curr_it->second) {
                if (nxt == start_node) {
                    path.push_back(start_node);
                    return path;
                }
                if (scc_set.find(nxt) != scc_set.end() && visited.find(nxt) == visited.end()) {
                    visited.insert(nxt);
                    auto new_path = path;
                    new_path.push_back(nxt);
                    q.push(new_path);
                }
            }
        }
    }

    std::vector<std::string> fallback;
    size_t limit = std::min(scc.size(), static_cast<size_t>(4));
    for (size_t i = 0; i < limit; ++i) fallback.push_back(scc[i]);
    fallback.push_back(scc[0]);
    return fallback;
}

void ArchitectureAnalyzer::detect_circular_dependencies(Graph& graph, std::vector<CodeIssue>& issues) {
    std::unordered_map<std::string, int> indices;
    std::unordered_map<std::string, int> lowlinks;
    std::unordered_set<std::string> on_stack;
    std::stack<std::string> st;
    int index = 0;
    std::vector<std::vector<std::string>> sccs;

    auto strongconnect = [&](auto& self, const std::string& v) -> void {
        indices[v] = index;
        lowlinks[v] = index;
        index++;
        st.push(v);
        on_stack.insert(v);

        auto it = graph.adj.find(v);
        if (it != graph.adj.end()) {
            for (const auto& w : it->second) {
                if (indices.find(w) == indices.end()) {
                    self(self, w);
                    lowlinks[v] = std::min(lowlinks[v], lowlinks[w]);
                } else if (on_stack.find(w) != on_stack.end()) {
                    lowlinks[v] = std::min(lowlinks[v], indices[w]);
                }
            }
        }

        if (lowlinks[v] == indices[v]) {
            std::vector<std::string> scc;
            while (true) {
                std::string w = st.top();
                st.pop();
                on_stack.erase(w);
                scc.push_back(w);
                if (w == v) break;
            }
            if (scc.size() > 1) {
                sccs.push_back(scc);
            }
        }
    };

    for (const auto& pair : graph.nodes) {
        if (pair.second.type == "module" || pair.second.type == "file") {
            if (indices.find(pair.first) == indices.end()) {
                strongconnect(strongconnect, pair.first);
            }
        }
    }

    // Sort SCCs so module-level and largest cycles come first
    std::sort(sccs.begin(), sccs.end(), [](const auto& a, const auto& b) {
        return a.size() > b.size();
    });

    int cycle_count = 0;
    for (const auto& scc : sccs) {
        cycle_count++;
        if (cycle_count > 25) break;

        std::vector<std::string> cycle_path = find_shortest_cycle(scc[0], scc, graph);

        CodeIssue issue;
        issue.id = "CIRCULAR_DEP_" + std::to_string(cycle_count);
        issue.type = "CIRCULAR_DEPENDENCY";
        issue.severity = IssueSeverity::CRITICAL;
        issue.title = "Aylanma Bog'liqlik Halqasi (" + std::to_string(cycle_path.size() - 1) + " qadam)";

        std::stringstream ss;
        for (size_t i = 0; i < cycle_path.size(); ++i) {
            ss << graph.nodes[cycle_path[i]].name;
            if (i + 1 < cycle_path.size()) ss << " ➔ ";
            issue.involved_nodes.push_back(cycle_path[i]);
        }
        issue.description = ss.str();
        issue.recommendation = "DIP (Dependency Inversion): Umumiy interfeys yarating yoki bog'liqlikni teskarisiga o'giring.";
        issue.score = 20.0;
        issues.push_back(issue);
    }
}

void ArchitectureAnalyzer::detect_layer_violations(Graph& graph, std::vector<CodeIssue>& issues) {
    auto get_layer_rank = [](const std::string& mod) -> int {
        if (mod.find("core") != std::string::npos) return 0;
        if (mod.find("api") != std::string::npos) return 1;
        if (mod.find("apps") != std::string::npos || mod.find("tools") != std::string::npos || mod.find("website") != std::string::npos) return 2;
        return 1;
    };

    int viol_count = 0;
    for (const auto& edge : graph.edges) {
        if (edge.type == "imports" || edge.type == "calls") {
            if (graph.has_node(edge.source) && graph.has_node(edge.target)) {
                const auto& src = graph.nodes[edge.source];
                const auto& tgt = graph.nodes[edge.target];

                int rank_src = get_layer_rank(src.module);
                int rank_tgt = get_layer_rank(tgt.module);

                if (rank_src == 0 && rank_tgt > 0) {
                    viol_count++;
                    if (viol_count > 15) break;

                    CodeIssue issue;
                    issue.id = "LAYER_VIOLATION_" + std::to_string(viol_count);
                    issue.type = "LAYER_VIOLATION";
                    issue.severity = IssueSeverity::CRITICAL;
                    issue.title = "Qatlam Qoidasi Buzilgan: Core ➔ " + tgt.module;
                    issue.description = "'" + src.name + "' poydevor qatlami yuqori qatlamdagi '" + tgt.name + "' ga noqonuniy bog'langan.";
                    issue.recommendation = "Clean Architecture: Core qatlami yuqori qatlamlarni bilmasligi kerak. Callback yoki Event-bus qo'llang.";
                    issue.involved_nodes.push_back(src.id);
                    issue.involved_nodes.push_back(tgt.id);
                    issue.score = 15.0;
                    issues.push_back(issue);
                }
            }
        }
    }
}

void ArchitectureAnalyzer::detect_god_objects(Graph& graph, std::vector<CodeIssue>& issues) {
    double total_pr = 0.0;
    int count = 0;
    for (const auto& pair : graph.nodes) {
        if (pair.second.type == "module" || pair.second.type == "file") {
            total_pr += pair.second.pagerank;
            count++;
        }
    }
    double avg_pr = count > 0 ? (total_pr / count) : 0.0;

    std::vector<const Node*> candidates;
    for (const auto& pair : graph.nodes) {
        const Node& node = pair.second;
        if (node.type == "module" || node.type == "file") {
            if (node.in_degree >= 5 && (node.pagerank > avg_pr * 2.0 || node.cyclomatic_complexity >= 25)) {
                candidates.push_back(&node);
            }
        }
    }

    std::sort(candidates.begin(), candidates.end(), [](const Node* a, const Node* b) {
        return (a->in_degree * 2 + a->cyclomatic_complexity) > (b->in_degree * 2 + b->cyclomatic_complexity);
    });

    int god_count = 0;
    for (const Node* node : candidates) {
        god_count++;
        if (god_count > 20) break;

        CodeIssue issue;
        issue.id = "GOD_OBJECT_" + std::to_string(god_count);
        issue.type = "GOD_OBJECT";
        issue.severity = IssueSeverity::WARNING;
        issue.title = "God Object / Bog'liqlik Markazi: " + node->name;

        std::stringstream ss;
        ss << "Kiruvchi bog'liqlik: " << node->in_degree << " ta, Ta'sir doirasi (Blast Radius): " << node->blast_radius
           << " ta tugun. Murakkablik: " << node->cyclomatic_complexity << ", Qatorlar: " << node->loc << ".";
        issue.description = ss.str();
        issue.recommendation = "SRP (Single Responsibility): Modulni kichik mustaqil xizmatlarga bo'ling.";
        issue.involved_nodes.push_back(node->id);
        issue.score = 10.0;
        issues.push_back(issue);
    }
}

void ArchitectureAnalyzer::detect_code_clones(Graph& graph, std::vector<CodeIssue>& issues, double threshold) {
    struct FnFeat {
        std::string id;
        const Node* node;
        std::unordered_map<std::string, double> tfidf;
        double tfidf_norm = 0.0;
        std::unordered_map<std::string, int> cfg_3grams;
        double cfg_norm = 0.0;
    };

    std::vector<FnFeat> fns;
    std::unordered_map<std::string, int> df;

    for (const auto& pair : graph.nodes) {
        if (pair.second.type == "function" && pair.second.loc >= 6 && pair.second.norm_token_vector.size() >= 5) {
            FnFeat f;
            f.id = pair.first;
            f.node = &pair.second;
            for (const auto& kv : pair.second.norm_token_vector) {
                df[kv.first]++;
            }
            fns.push_back(std::move(f));
        }
    }

    size_t N = fns.size();
    if (N < 2) return;

    // Precompute TF-IDF vectors and CFG 3-gram vectors
    for (auto& f : fns) {
        double sum_sq = 0.0;
        for (const auto& kv : f.node->norm_token_vector) {
            double idf = std::log((static_cast<double>(N) + 1.0) / (static_cast<double>(df[kv.first]) + 1.0)) + 1.0;
            double w = static_cast<double>(kv.second) * idf;
            f.tfidf[kv.first] = w;
            sum_sq += w * w;
        }
        f.tfidf_norm = std::sqrt(sum_sq);

        const auto& seq = f.node->cfg_sequence;
        if (seq.size() >= 3) {
            for (size_t k = 0; k + 2 < seq.size(); ++k) {
                std::string gram = seq[k] + ">" + seq[k + 1] + ">" + seq[k + 2];
                f.cfg_3grams[gram]++;
            }
            double csq = 0.0;
            for (const auto& kv : f.cfg_3grams) {
                csq += static_cast<double>(kv.second) * kv.second;
            }
            f.cfg_norm = std::sqrt(csq);
        }
    }

    struct CloneCandidate {
        size_t i;
        size_t j;
        double prob;
        double tfidf_sim;
        double cfg_sim;
        std::string kind;
    };

    std::vector<CloneCandidate> candidates;

    for (size_t i = 0; i < N; ++i) {
        const auto& f1 = fns[i];
        double sz1 = static_cast<double>(f1.node->norm_token_vector.size());

        for (size_t j = i + 1; j < N; ++j) {
            const auto& f2 = fns[j];
            if (f1.node->file_path == f2.node->file_path) continue;

            // Exact normalized hash match (Type-1 / Type-2)
            if (f1.node->exact_hash != 0 && f1.node->exact_hash == f2.node->exact_hash) {
                candidates.push_back({i, j, 0.99, 1.0, 1.0, "Type-1/2: Bir xil funksiya"});
                continue;
            }

            double sz2 = static_cast<double>(f2.node->norm_token_vector.size());
            if (std::min(sz1, sz2) / std::max(sz1, sz2) < 0.60) continue;

            double loc_ratio = static_cast<double>(std::min(f1.node->loc, f2.node->loc)) /
                               static_cast<double>(std::max(f1.node->loc, f2.node->loc));
            if (loc_ratio < 0.50) continue;

            double sim_tfidf = cosine_sparse_double(f1.tfidf, f1.tfidf_norm, f2.tfidf, f2.tfidf_norm);
            if (sim_tfidf < 0.50) continue; // Fast prune

            double sim_behav = cosine_behavior(f1.node->behavior_profile, f2.node->behavior_profile);
            double sim_cfg = 0.0;
            double prob = 0.0;

            if (f1.cfg_norm > 0.0 && f2.cfg_norm > 0.0) {
                sim_cfg = cosine_sparse_int(f1.cfg_3grams, f1.cfg_norm, f2.cfg_3grams, f2.cfg_norm);
                prob = 0.45 * sim_tfidf + 0.35 * sim_cfg + 0.20 * sim_behav;
            } else {
                prob = 0.70 * sim_tfidf + 0.30 * sim_behav;
            }

            // Same function name across different files is a strong signal of accidental re-implementation
            if (f1.node->name == f2.node->name) {
                prob = std::min(0.98, prob + 0.08);
            }

            if (prob >= threshold) {
                std::string kind = (sim_tfidf >= 0.85)
                    ? "Type-3: O'zgartirilgan nusxa"
                    : "Type-4: Ehtimoliy mantiqiy o'xshashlik";
                candidates.push_back({i, j, prob, sim_tfidf, sim_cfg, kind});
            }
        }
    }

    std::sort(candidates.begin(), candidates.end(), [](const CloneCandidate& a, const CloneCandidate& b) {
        return a.prob > b.prob;
    });

    std::unordered_map<std::string, int> per_fn_cap;
    int clone_count = 0;

    for (const auto& c : candidates) {
        if (clone_count >= 35) break;
        const Node* fn1 = fns[c.i].node;
        const Node* fn2 = fns[c.j].node;

        if (per_fn_cap[fn1->id] >= 3 || per_fn_cap[fn2->id] >= 3) continue;
        per_fn_cap[fn1->id]++;
        per_fn_cap[fn2->id]++;
        clone_count++;

        int pct = static_cast<int>(std::round(c.prob * 100.0));
        CodeIssue issue;
        issue.id = "CLONE_" + std::to_string(clone_count);
        issue.type = "CODE_DUPLICATION";
        issue.severity = (pct >= 92) ? IssueSeverity::CRITICAL : IssueSeverity::WARNING;
        issue.title = c.kind + " (" + std::to_string(pct) + "%)";

        std::stringstream ss;
        ss << "'" << fn1->name << "' (" << fn1->file_path << ":" << fn1->line_start
           << ") va '" << fn2->name << "' (" << fn2->file_path << ":" << fn2->line_start
           << ") [TF-IDF: " << static_cast<int>(c.tfidf_sim * 100) << "%, CFG: "
           << static_cast<int>(c.cfg_sim * 100) << "%].";
        issue.description = ss.str();
        issue.recommendation = "DRY: Ikkala funksiyani bitta umumiy helper/utility funksiyaga birlashtiring.";
        issue.involved_nodes.push_back(fn1->id);
        issue.involved_nodes.push_back(fn2->id);
        issue.score = (pct >= 92) ? 10.0 : 7.0;
        issues.push_back(issue);
    }
}

void ArchitectureAnalyzer::detect_useless_wrappers(Graph& graph, std::vector<CodeIssue>& issues) {
    int wrapper_count = 0;
    for (const auto& pair : graph.nodes) {
        if (pair.second.type == "function" && pair.second.is_pass_through) {
            wrapper_count++;
            if (wrapper_count > 20) break;

            CodeIssue issue;
            issue.id = "WRAPPER_" + std::to_string(wrapper_count);
            issue.type = "USELESS_WRAPPER";
            issue.severity = IssueSeverity::INFO;
            issue.title = "Tranzit Wrapper: " + pair.second.name;
            issue.description = "Funksiya shunchaki '" + pair.second.pass_through_target + "' chaqiruvini o'tkazib yubormoqda.";
            issue.recommendation = "To'g'ridan-to'g'ri nishon funksiyani chaqiring.";
            issue.involved_nodes.push_back(pair.first);
            issue.score = 3.0;
            issues.push_back(issue);
        }
    }
}

void ArchitectureAnalyzer::detect_orphan_nodes(Graph& graph, std::vector<CodeIssue>& issues) {
    std::vector<const Node*> orphans;
    for (const auto& pair : graph.nodes) {
        if (pair.second.type == "file") {
            const auto& n = pair.second;
            if (n.in_degree == 0 && n.loc > 30) {
                if (n.name.find("main") == std::string::npos &&
                    n.name.find("index") == std::string::npos &&
                    n.name.find("test") == std::string::npos) {
                    orphans.push_back(&n);
                }
            }
        }
    }

    std::sort(orphans.begin(), orphans.end(), [](const Node* a, const Node* b) {
        return a->loc > b->loc;
    });

    int orphan_count = 0;
    for (const Node* n : orphans) {
        orphan_count++;
        if (orphan_count > 15) break;

        CodeIssue issue;
        issue.id = "ORPHAN_" + std::to_string(orphan_count);
        issue.type = "ORPHAN_CODE";
        issue.severity = IssueSeverity::INFO;
        issue.title = "Yakkalangan Fayl (Orphan Code): " + n->name;
        issue.description = "Loyihada hech qaysi fayl bu modulni import qilmagan (" + std::to_string(n->loc) + " qator).";
        issue.recommendation = "Fayl haqiqatan ishlatilayotganini tekshiring yoki uni tozalang.";
        issue.involved_nodes.push_back(n->id);
        issue.score = 3.0;
        issues.push_back(issue);
    }
}

double ArchitectureAnalyzer::compute_health_score(const std::vector<CodeIssue>& issues, const Graph& /*graph*/) {
    double score = 100.0;
    for (const auto& issue : issues) {
        score -= issue.score;
    }
    return std::max(10.0, std::min(100.0, score));
}

void ArchitectureAnalyzer::analyze(Graph& graph, std::vector<CodeIssue>& issues, double& health_score) {
    calculate_coupling_and_instability(graph);
    calculate_blast_radius(graph);
    calculate_pagerank(graph);
    detect_circular_dependencies(graph, issues);
    detect_layer_violations(graph, issues);
    detect_god_objects(graph, issues);
    detect_code_clones(graph, issues, 0.78);
    detect_useless_wrappers(graph, issues);
    detect_orphan_nodes(graph, issues);
    health_score = compute_health_score(issues, graph);
}
