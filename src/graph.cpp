#include "graph.hpp"

void Graph::add_node(const Node& node) {
    nodes[node.id] = node;
}

void Graph::add_edge(const std::string& src, const std::string& tgt, const std::string& type, int weight) {
    if (src == tgt) return; // avoid trivial self-loops for dependency analysis
    for (auto& e : edges) {
        if (e.source == src && e.target == tgt && e.type == type) {
            e.weight += weight;
            return;
        }
    }
    edges.push_back({src, tgt, type, weight});
}

bool Graph::has_node(const std::string& id) const {
    return nodes.find(id) != nodes.end();
}

void Graph::rebuild_adjacencies() {
    adj.clear();
    rev_adj.clear();
    for (const auto& pair : nodes) {
        adj[pair.first] = {};
        rev_adj[pair.first] = {};
    }
    for (const auto& e : edges) {
        adj[e.source].push_back(e.target);
        rev_adj[e.target].push_back(e.source);
    }
}
