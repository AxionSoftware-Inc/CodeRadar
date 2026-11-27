#ifndef BINARY_WRITER_HPP
#define BINARY_WRITER_HPP

#include "graph.hpp"
#include <string>
#include <vector>

class BinaryWriter {
public:
    static bool write_binary(const std::string& bin_path, const std::string& js_bundle_path, 
                             const Graph& graph, const std::vector<CodeIssue>& issues, double health_score);
};

#endif // BINARY_WRITER_HPP
