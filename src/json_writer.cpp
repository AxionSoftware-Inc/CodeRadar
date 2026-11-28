#include "json_writer.hpp"
#include <fstream>
#include <sstream>

std::string JsonWriter::escape(const std::string& s) {
    std::ostringstream o;
    for (char c : s) {
        if (c == '"') o << "\\\"";
        else if (c == '\\') o << "\\\\";
        else if (c == '\b') o << "\\b";
        else if (c == '\f') o << "\\f";
        else if (c == '\n') o << "\\n";
        else if (c == '\r') o << "\\r";
        else if (c == '\t') o << "\\t";
        else if ('\x00' <= c && c <= '\x1f') {
            o << "\\u" << std::hex << (int)c;
        } else {
            o << c;
        }
    }
    return o.str();
}

bool JsonWriter::write_report(const std::string& output_path, const Graph& graph, const std::vector<CodeIssue>& issues, double health_score) {
    std::ofstream out(output_path);
    if (!out.is_open()) return false;

    out << "{\n";
    
    // Summary
    out << "  \"summary\": {\n";
    out << "    \"health_score\": " << health_score << ",\n";
    out << "    \"total_nodes\": " << graph.nodes.size() << ",\n";
    out << "    \"total_edges\": " << graph.edges.size() << ",\n";
    out << "    \"total_issues\": " << issues.size() << "\n";
    out << "  },\n";

    // Issues
    out << "  \"issues\": [\n";
    for (size_t i = 0; i < issues.size(); ++i) {
        const auto& issue = issues[i];
        out << "    {\n";
        out << "      \"id\": \"" << escape(issue.id) << "\",\n";
        out << "      \"type\": \"" << escape(issue.type) << "\",\n";
        out << "      \"severity\": \"" << (issue.severity == IssueSeverity::CRITICAL ? "CRITICAL" : (issue.severity == IssueSeverity::WARNING ? "WARNING" : "INFO")) << "\",\n";
        out << "      \"title\": \"" << escape(issue.title) << "\",\n";
        out << "      \"description\": \"" << escape(issue.description) << "\",\n";
        out << "      \"recommendation\": \"" << escape(issue.recommendation) << "\",\n";
        out << "      \"involved_nodes\": [";
        for (size_t k = 0; k < issue.involved_nodes.size(); ++k) {
            out << "\"" << escape(issue.involved_nodes[k]) << "\"" << (k + 1 < issue.involved_nodes.size() ? ", " : "");
        }
        out << "]\n";
        out << "    }" << (i + 1 < issues.size() ? "," : "") << "\n";
    }
    out << "  ],\n";

    // Nodes
    out << "  \"nodes\": [\n";
    size_t node_idx = 0;
    for (auto it = graph.nodes.begin(); it != graph.nodes.end(); ++it, ++node_idx) {
        const auto& n = it->second;
        out << "    {\n";
        out << "      \"id\": \"" << escape(n.id) << "\",\n";
        out << "      \"name\": \"" << escape(n.name) << "\",\n";
        out << "      \"type\": \"" << escape(n.type) << "\",\n";
        out << "      \"module\": \"" << escape(n.module) << "\",\n";
        out << "      \"file_path\": \"" << escape(n.file_path) << "\",\n";
        out << "      \"loc\": " << n.loc << ",\n";
        out << "      \"complexity\": " << n.cyclomatic_complexity << ",\n";
        out << "      \"in_degree\": " << n.in_degree << ",\n";
        out << "      \"out_degree\": " << n.out_degree << ",\n";
        out << "      \"instability\": " << n.instability << ",\n";
        out << "      \"pagerank\": " << n.pagerank << ",\n";
        out << "      \"blast_radius\": " << n.blast_radius << ",\n";
        out << "      \"runtime_heat\": " << n.runtime_heat << ",\n";
        out << "      \"hotspot_score\": " << n.hotspot_score << ",\n";
        out << "      \"is_pass_through\": " << (n.is_pass_through ? "true" : "false") << "\n";
        out << "    }" << (node_idx + 1 < graph.nodes.size() ? "," : "") << "\n";
    }
    out << "  ],\n";

    // Edges
    out << "  \"edges\": [\n";
    for (size_t i = 0; i < graph.edges.size(); ++i) {
        const auto& e = graph.edges[i];
        out << "    {\n";
        out << "      \"source\": \"" << escape(e.source) << "\",\n";
        out << "      \"target\": \"" << escape(e.target) << "\",\n";
        out << "      \"type\": \"" << escape(e.type) << "\",\n";
        out << "      \"weight\": " << e.weight << "\n";
        out << "    }" << (i + 1 < graph.edges.size() ? "," : "") << "\n";
    }
    out << "  ]\n";

    out << "}\n";
    return true;
}
