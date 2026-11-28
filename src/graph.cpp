#include "graph.hpp"

void Graph::add_node(const Node& node) {
    nodes[node.id] = node;
}

void Graph::add_edge(const std::string& src, const std::string& tgt, const std::string& type, int weight) {
    if (src == tgt) return; // avoid trivial self-loops for dependency analysis
    std::string key = src + "\x1f" + tgt + "\x1f" + type;
    auto it = edge_index.find(key);
    if (it != edge_index.end()) {
        edges[it->second].weight += weight;
        return;
    }
    edge_index[std::move(key)] = edges.size();
    edges.push_back({src, tgt, type, weight});
}

bool Graph::has_node(const std::string& id) const {
    return nodes.find(id) != nodes.end();
}

void Graph::rebuild_adjacencies() {
    adj.clear();
    rev_adj.clear();
    dep_adj.clear();
    dep_rev_adj.clear();
    for (const auto& pair : nodes) {
        adj[pair.first] = {};
        rev_adj[pair.first] = {};
        dep_adj[pair.first] = {};
        dep_rev_adj[pair.first] = {};
    }
    for (const auto& e : edges) {
        adj[e.source].push_back(e.target);
        rev_adj[e.target].push_back(e.source);
        if (e.type != "contains") {
            dep_adj[e.source].push_back(e.target);
            dep_rev_adj[e.target].push_back(e.source);
        }
    }
}
