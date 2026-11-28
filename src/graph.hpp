#ifndef GRAPH_HPP
#define GRAPH_HPP

#include <string>
#include <vector>
#include <array>
#include <cstdint>
#include <unordered_map>
#include <unordered_set>
#include <memory>

struct Node {
    std::string id;
    std::string name;
    std::string type;       // "module", "file", "function"
    std::string file_path;
    std::string module;
    std::string scope;       // Enclosing class/struct/namespace if detected
    int line_start = 0;
    int line_end = 0;
    int loc = 0;             // lines of code
    int cyclomatic_complexity = 1;
    
    // Metrics calculated by analyzer
    double pagerank = 0.0;
    int in_degree = 0;       // Afferent coupling (Ca)
    int out_degree = 0;      // Efferent coupling (Ce)
    double instability = 0.0;// Ce / (Ca + Ce)
    int blast_radius = 0;    // Transitive reverse-dependency count
    double runtime_heat = 0.0;   // Normalized [0..1] dynamic profiler heat
    double hotspot_score = 0.0;  // Combined structural + runtime bottleneck score
    
    // Normalized token & structural fingerprints for probabilistic clone detection
    std::unordered_map<std::string, int> token_vector;
    std::unordered_map<std::string, int> norm_token_vector;
    std::vector<std::string> cfg_sequence;
    std::vector<std::string> called_identifiers;
    std::vector<std::pair<std::string, std::string>> qualified_calls; // (qualifier, fn_name)
    std::array<double, 8> behavior_profile{};
    uint64_t exact_hash = 0;

    std::string signature;
    bool is_pass_through = false; // Just calls another function and returns
    std::string pass_through_target;
};

struct Edge {
    std::string source;
    std::string target;
    std::string type;        // "imports", "calls", "contains"
    int weight = 1;
};

enum class IssueSeverity {
    INFO,
    WARNING,
    CRITICAL
};

struct CodeIssue {
    std::string id;
    std::string type;        // "CIRCULAR_DEPENDENCY", "GOD_OBJECT", "CODE_DUPLICATION", "USELESS_WRAPPER", "HIGH_INSTABILITY"
    IssueSeverity severity;
    std::string title;
    std::string description;
    std::string recommendation;
    std::vector<std::string> involved_nodes;
    double score = 0.0;
};

class Graph {
public:
    std::unordered_map<std::string, Node> nodes;
    std::vector<Edge> edges;
    
    // Adjacency lists for fast traversal
    std::unordered_map<std::string, std::vector<std::string>> adj;
    std::unordered_map<std::string, std::vector<std::string>> rev_adj;
    // Dependency-only adjacency lists (excluding "contains" edges)
    std::unordered_map<std::string, std::vector<std::string>> dep_adj;
    std::unordered_map<std::string, std::vector<std::string>> dep_rev_adj;
    // O(1) edge deduplication index
    std::unordered_map<std::string, size_t> edge_index;
    
    void add_node(const Node& node);
    void add_edge(const std::string& src, const std::string& tgt, const std::string& type, int weight = 1);
    bool has_node(const std::string& id) const;
    void rebuild_adjacencies();
};

#endif // GRAPH_HPP
