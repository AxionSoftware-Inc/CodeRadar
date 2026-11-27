#ifndef JSON_WRITER_HPP
#define JSON_WRITER_HPP

#include "graph.hpp"
#include <string>
#include <vector>

class JsonWriter {
public:
    static bool write_report(const std::string& output_path, const Graph& graph, const std::vector<CodeIssue>& issues, double health_score);

private:
    static std::string escape(const std::string& s);
};

#endif // JSON_WRITER_HPP
