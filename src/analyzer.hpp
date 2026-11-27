#ifndef ANALYZER_HPP
#define ANALYZER_HPP

#include "graph.hpp"
#include <vector>

class ArchitectureAnalyzer {
public:
    void analyze(Graph& graph, std::vector<CodeIssue>& issues, double& health_score);

private:
    void calculate_coupling_and_instability(Graph& graph);
    void calculate_blast_radius(Graph& graph);
    void calculate_pagerank(Graph& graph, double damping = 0.85, int iterations = 30);
    void detect_circular_dependencies(Graph& graph, std::vector<CodeIssue>& issues);
    void detect_layer_violations(Graph& graph, std::vector<CodeIssue>& issues);
    void detect_god_objects(Graph& graph, std::vector<CodeIssue>& issues);
    void detect_code_clones(Graph& graph, std::vector<CodeIssue>& issues, double threshold = 0.78);
    void detect_useless_wrappers(Graph& graph, std::vector<CodeIssue>& issues);
    void detect_orphan_nodes(Graph& graph, std::vector<CodeIssue>& issues);
    double compute_health_score(const std::vector<CodeIssue>& issues, const Graph& graph);

    std::vector<std::string> find_shortest_cycle(const std::string& start_node, const std::vector<std::string>& scc, const Graph& graph);
};

#endif // ANALYZER_HPP
