#ifndef PARSER_HPP
#define PARSER_HPP

#include "graph.hpp"
#include <string>
#include <vector>
#include <unordered_map>
#include <filesystem>

namespace fs = std::filesystem;

class CodeParser {
public:
    void parse_directory(const std::string& root_dir, Graph& graph);

private:
    std::unordered_map<std::string, std::vector<std::string>> file_imports;
    void parse_file(const fs::path& file_path, const std::string& root_dir, Graph& graph);
    std::string determine_module(const fs::path& file_path, const std::string& root_dir);
    std::string strip_comments_and_strings(const std::string& code, const std::string& ext);
    int calculate_complexity(const std::string& clean_body);
    void extract_fingerprints(const std::string& clean_body, const std::string& fn_name, Node& fn_node);
    bool check_pass_through(const std::string& clean_body, std::string& target_call);
};

#endif // PARSER_HPP
