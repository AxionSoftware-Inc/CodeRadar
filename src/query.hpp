#ifndef QUERY_HPP
#define QUERY_HPP

#include "graph.hpp"
#include <string>
#include <vector>

class QueryEngine {
public:
    static int execute(const std::string& command,
                       const std::string& arg,
                       const Graph& graph,
                       const std::vector<CodeIssue>& issues,
                       double health_score);
};

#endif // QUERY_HPP
