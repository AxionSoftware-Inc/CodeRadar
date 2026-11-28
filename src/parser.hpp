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
    void parse_directory(const std::string& root_dir, Graph& graph, bool use_cache = true, bool quiet = false);

private:
    struct ParsedFileResult {
        std::string rel_path;
        int64_t mtime_ns = 0;
        uint64_t file_size = 0;
        std::string module_name;
        Node file_node;
        std::vector<Node> fn_nodes;
        std::vector<std::string> imports;
    };

    ParsedFileResult parse_file_isolated(const fs::path& file_path, const std::string& root_dir) const;
    std::string determine_module(const fs::path& file_path, const std::string& root_dir) const;
    std::string strip_comments_and_strings(const std::string& code, const std::string& ext) const;
    int calculate_complexity(const std::string& clean_body) const;
    void extract_fingerprints(const std::string& clean_body, const std::string& fn_name, Node& fn_node) const;
    bool check_pass_through(const std::string& clean_body, std::string& target_call) const;

    static bool load_cache(const std::string& cache_path, std::unordered_map<std::string, ParsedFileResult>& cache_map);
    static bool save_cache(const std::string& cache_path, const std::unordered_map<std::string, ParsedFileResult>& cache_map);
};

#endif // PARSER_HPP
