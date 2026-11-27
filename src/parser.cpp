#include "parser.hpp"
#include <fstream>
#include <sstream>
#include <regex>
#include <iostream>
#include <algorithm>
#include <cmath>
#include <cctype>
#include <unordered_set>

namespace {

const std::unordered_set<std::string>& get_keywords() {
    static const std::unordered_set<std::string> kw = {
        "if", "else", "for", "while", "do", "switch", "case", "default", "break", "continue",
        "return", "try", "catch", "finally", "throw", "throws", "new", "delete", "this", "self",
        "super", "true", "false", "null", "nullptr", "None", "True", "False", "void", "int",
        "double", "float", "bool", "boolean", "char", "string", "String", "var", "let", "const",
        "final", "auto", "static", "public", "private", "protected", "internal", "virtual",
        "override", "async", "await", "yield", "import", "from", "using", "namespace", "class",
        "struct", "enum", "interface", "extends", "implements", "def", "function", "lambda",
        "in", "is", "as", "of", "and", "or", "not", "sizeof", "typedef", "template", "typename",
        "std", "size_t", "uint32_t", "uint16_t", "uint8_t", "int32_t", "int64_t", "List", "Map",
        "Set", "Future", "Task", "Widget", "BuildContext", "dynamic", "Object"
    };
    return kw;
}

uint64_t fnv1a_hash(const std::vector<std::string>& tokens) {
    uint64_t h = 14695981039346656037ULL;
    for (const auto& t : tokens) {
        for (unsigned char c : t) {
            h ^= c;
            h *= 1099511628211ULL;
        }
        h ^= 0xFF;
        h *= 1099511628211ULL;
    }
    return h;
}

int count_newlines_up_to(const std::string& s, size_t pos) {
    int lines = 1;
    size_t lim = std::min(pos, s.size());
    for (size_t i = 0; i < lim; ++i) {
        if (s[i] == '\n') lines++;
    }
    return lines;
}

} // namespace

std::string CodeParser::determine_module(const fs::path& file_path, const std::string& root_dir) {
    fs::path rel = fs::relative(file_path, root_dir);
    auto it = rel.begin();
    if (it != rel.end()) {
        std::string top = it->string();
        ++it;
        if ((top == "src" || top == "apps" || top == "tools" || top == "lib" || top == "packages") &&
            it != rel.end() && it->string() != rel.filename().string()) {
            return top + "/" + it->string();
        }
        return top;
    }
    return "root";
}

std::string CodeParser::strip_comments_and_strings(const std::string& code, const std::string& ext) {
    std::string out;
    out.reserve(code.size());
    bool is_py = (ext == ".py");
    size_t n = code.size();
    size_t i = 0;

    while (i < n) {
        char c = code[i];
        char nxt = (i + 1 < n) ? code[i + 1] : '\0';

        // Line comment
        if ((!is_py && c == '/' && nxt == '/') || (is_py && c == '#')) {
            while (i < n && code[i] != '\n') {
                out.push_back(' ');
                i++;
            }
            continue;
        }
        // Block comment (C-style)
        if (!is_py && c == '/' && nxt == '*') {
            out.push_back(' ');
            out.push_back(' ');
            i += 2;
            while (i < n) {
                if (code[i] == '*' && i + 1 < n && code[i + 1] == '/') {
                    out.push_back(' ');
                    out.push_back(' ');
                    i += 2;
                    break;
                }
                out.push_back(code[i] == '\n' ? '\n' : ' ');
                i++;
            }
            continue;
        }
        // String literals (' or ")
        if (c == '"' || c == '\'') {
            char quote = c;
            // Python triple-quote check
            if (is_py && i + 2 < n && code[i + 1] == quote && code[i + 2] == quote) {
                out.append(" $STR ");
                i += 3;
                while (i < n) {
                    if (code[i] == quote && i + 2 < n && code[i + 1] == quote && code[i + 2] == quote) {
                        i += 3;
                        break;
                    }
                    if (code[i] == '\n') out.push_back('\n');
                    i++;
                }
                continue;
            }
            out.append(" $STR ");
            i++;
            while (i < n) {
                if (code[i] == '\\' && i + 1 < n) {
                    i += 2;
                    continue;
                }
                if (code[i] == quote) {
                    i++;
                    break;
                }
                if (code[i] == '\n') {
                    out.push_back('\n');
                    break;
                }
                i++;
            }
            continue;
        }

        out.push_back(c);
        i++;
    }
    return out;
}

int CodeParser::calculate_complexity(const std::string& clean_body) {
    int complexity = 1;
    size_t n = clean_body.size();
    size_t i = 0;
    while (i < n) {
        if (std::isalpha(static_cast<unsigned char>(clean_body[i])) || clean_body[i] == '_') {
            size_t start = i;
            while (i < n && (std::isalnum(static_cast<unsigned char>(clean_body[i])) || clean_body[i] == '_')) i++;
            std::string word = clean_body.substr(start, i - start);
            if (word == "if" || word == "for" || word == "while" || word == "case" || word == "catch" || word == "elif") {
                complexity++;
            }
        } else if (i + 1 < n && ((clean_body[i] == '&' && clean_body[i + 1] == '&') ||
                                 (clean_body[i] == '|' && clean_body[i + 1] == '|'))) {
            complexity++;
            i += 2;
        } else if (clean_body[i] == '?') {
            complexity++;
            i++;
        } else {
            i++;
        }
    }
    return complexity;
}

void CodeParser::extract_fingerprints(const std::string& clean_body, const std::string& fn_name, Node& fn_node) {
    const auto& keywords = get_keywords();
    std::unordered_map<std::string, std::string> var_alias;
    int var_counter = 0;

    std::vector<std::string> norm_seq;
    fn_node.token_vector.clear();
    fn_node.norm_token_vector.clear();
    fn_node.cfg_sequence.clear();
    fn_node.called_identifiers.clear();
    fn_node.behavior_profile.fill(0.0);

    std::unordered_set<std::string> seen_calls;
    size_t n = clean_body.size();
    size_t i = 0;

    // Skip signature header up to first '{' or ':' to fingerprint the actual body
    size_t body_start = clean_body.find('{');
    if (body_start == std::string::npos) body_start = clean_body.find(':');
    if (body_start != std::string::npos && body_start + 1 < n) {
        i = body_start + 1;
    }

    while (i < n) {
        char c = clean_body[i];
        if (std::isspace(static_cast<unsigned char>(c))) {
            i++;
            continue;
        }

        // Identifiers / Keywords / $STR
        if (c == '$' || std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
            size_t start = i;
            i++;
            while (i < n && (std::isalnum(static_cast<unsigned char>(clean_body[i])) || clean_body[i] == '_')) i++;
            std::string tok = clean_body.substr(start, i - start);

            if (tok == "$STR") {
                norm_seq.push_back("$STR");
                fn_node.norm_token_vector["$STR"]++;
                continue;
            }

            // Look ahead for '(' to detect function call
            size_t j = i;
            while (j < n && std::isspace(static_cast<unsigned char>(clean_body[j]))) j++;
            bool is_call = (j < n && clean_body[j] == '(');

            if (keywords.count(tok)) {
                norm_seq.push_back(tok);
                fn_node.norm_token_vector[tok]++;

                if (tok == "if" || tok == "elif") {
                    fn_node.cfg_sequence.push_back("IF");
                    fn_node.behavior_profile[1] += 1.0;
                } else if (tok == "else") {
                    fn_node.cfg_sequence.push_back("ELSE");
                } else if (tok == "for" || tok == "while" || tok == "do") {
                    fn_node.cfg_sequence.push_back("LOOP");
                    fn_node.behavior_profile[0] += 1.0;
                } else if (tok == "switch") {
                    fn_node.cfg_sequence.push_back("SWITCH");
                    fn_node.behavior_profile[1] += 1.0;
                } else if (tok == "case") {
                    fn_node.cfg_sequence.push_back("CASE");
                } else if (tok == "try") {
                    fn_node.cfg_sequence.push_back("TRY");
                } else if (tok == "catch") {
                    fn_node.cfg_sequence.push_back("CATCH");
                    fn_node.behavior_profile[1] += 1.0;
                } else if (tok == "return") {
                    fn_node.cfg_sequence.push_back("RET");
                    fn_node.behavior_profile[2] += 1.0;
                } else if (tok == "throw") {
                    fn_node.cfg_sequence.push_back("THROW");
                }
            } else {
                if (tok.length() > 2) {
                    fn_node.token_vector[tok]++;
                }
                if (is_call && tok != fn_name) {
                    fn_node.cfg_sequence.push_back("CALL");
                    fn_node.behavior_profile[3] += 1.0;
                    if (seen_calls.insert(tok).second) {
                        fn_node.called_identifiers.push_back(tok);
                    }
                    // Keep call arity/role normalized as $CALL_x or $CALL
                    norm_seq.push_back("$CALL");
                    fn_node.norm_token_vector["$CALL"]++;
                } else {
                    auto it = var_alias.find(tok);
                    std::string alias;
                    if (it == var_alias.end()) {
                        // Cap positional buckets at 16 so long functions still generalize
                        alias = "$V" + std::to_string(var_counter % 16);
                        var_alias[tok] = alias;
                        var_counter++;
                    } else {
                        alias = it->second;
                    }
                    norm_seq.push_back(alias);
                    fn_node.norm_token_vector[alias]++;
                }
            }
            continue;
        }

        // Numbers
        if (std::isdigit(static_cast<unsigned char>(c))) {
            while (i < n && (std::isalnum(static_cast<unsigned char>(clean_body[i])) || clean_body[i] == '.')) i++;
            norm_seq.push_back("$NUM");
            fn_node.norm_token_vector["$NUM"]++;
            continue;
        }

        // Two-char operators
        char nxt = (i + 1 < n) ? clean_body[i + 1] : '\0';
        std::string op2;
        if ((c == '=' && nxt == '=') || (c == '!' && nxt == '=') ||
            (c == '<' && nxt == '=') || (c == '>' && nxt == '=') ||
            (c == '&' && nxt == '&') || (c == '|' && nxt == '|') ||
            (c == '+' && nxt == '=') || (c == '-' && nxt == '=') ||
            (c == '*' && nxt == '=') || (c == '/' && nxt == '=') ||
            (c == '+' && nxt == '+') || (c == '-' && nxt == '-') ||
            (c == '-' && nxt == '>') || (c == '=' && nxt == '>')) {
            op2 = std::string(1, c) + nxt;
            i += 2;
            norm_seq.push_back(op2);
            fn_node.norm_token_vector[op2]++;

            if (op2 == "==" || op2 == "!=" || op2 == "<=" || op2 == ">=") {
                fn_node.cfg_sequence.push_back("CMP");
                fn_node.behavior_profile[5] += 1.0;
            } else if (op2 == "+=" || op2 == "-=" || op2 == "*=" || op2 == "/=") {
                fn_node.cfg_sequence.push_back("ASSIGN");
                fn_node.behavior_profile[4] += 1.0;
            } else if (op2 == "&&" || op2 == "||") {
                fn_node.cfg_sequence.push_back("LOGIC");
                fn_node.behavior_profile[1] += 1.0;
            }
            continue;
        }

        // Single-char operators & punctuation
        std::string op1(1, c);
        i++;
        if (c == '=') {
            norm_seq.push_back("=");
            fn_node.norm_token_vector["="]++;
            fn_node.cfg_sequence.push_back("ASSIGN");
            fn_node.behavior_profile[4] += 1.0;
        } else if (c == '<' || c == '>') {
            norm_seq.push_back(op1);
            fn_node.norm_token_vector[op1]++;
            fn_node.cfg_sequence.push_back("CMP");
            fn_node.behavior_profile[5] += 1.0;
        } else if (c == '+' || c == '-' || c == '*' || c == '/' || c == '%') {
            norm_seq.push_back(op1);
            fn_node.norm_token_vector[op1]++;
            fn_node.cfg_sequence.push_back("MATH");
            fn_node.behavior_profile[6] += 1.0;
        } else if (c == '{' || c == '}' || c == '[' || c == ']') {
            norm_seq.push_back(op1);
        }
    }

    fn_node.behavior_profile[7] = static_cast<double>(fn_node.cyclomatic_complexity);
    fn_node.exact_hash = norm_seq.empty() ? 0 : fnv1a_hash(norm_seq);
}

bool CodeParser::check_pass_through(const std::string& clean_body, std::string& target_call) {
    size_t open_brace = clean_body.find('{');
    size_t close_brace = clean_body.rfind('}');
    std::string inner = clean_body;
    if (open_brace != std::string::npos && close_brace != std::string::npos && close_brace > open_brace) {
        inner = clean_body.substr(open_brace + 1, close_brace - open_brace - 1);
    }

    std::string compact;
    compact.reserve(inner.size());
    for (char c : inner) {
        if (c != '\n' && c != '\r') compact.push_back(c);
    }

    size_t first = compact.find_first_not_of(" \t");
    if (first == std::string::npos) return false;
    size_t last = compact.find_last_not_of(" \t");
    compact = compact.substr(first, last - first + 1);

    static const std::regex pass_through_regex(R"(^(?:return\s+)?(?:await\s+)?(?:[a-zA-Z_][a-zA-Z0-9_]*\.)*([a-zA-Z_][a-zA-Z0-9_]*)\s*\([^;{}]*\)\s*;?$)");
    std::smatch match;
    if (std::regex_match(compact, match, pass_through_regex)) {
        if (match.size() >= 2) {
            target_call = match[1].str();
            return true;
        }
    }
    return false;
}

void CodeParser::parse_file(const fs::path& file_path, const std::string& root_dir, Graph& graph) {
    std::ifstream file(file_path);
    if (!file.is_open()) return;

    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    std::string ext = file_path.extension().string();
    std::string rel_path = fs::relative(file_path, root_dir).string();
    std::string module_name = determine_module(file_path, root_dir);

    std::string clean_content = strip_comments_and_strings(content, ext);

    // Create file node
    Node file_node;
    file_node.id = "file:" + rel_path;
    file_node.name = file_path.filename().string();
    file_node.type = "file";
    file_node.file_path = rel_path;
    file_node.module = module_name;

    int loc = 0;
    {
        std::stringstream ss(content);
        std::string line;
        while (std::getline(ss, line)) {
            if (!line.empty() && line.find_first_not_of(" \t\r\n") != std::string::npos) {
                loc++;
            }
        }
    }
    file_node.loc = loc;
    file_node.cyclomatic_complexity = calculate_complexity(clean_content);
    graph.add_node(file_node);

    // Ensure module node exists
    std::string mod_id = "module:" + module_name;
    if (!graph.has_node(mod_id)) {
        Node mod_node;
        mod_node.id = mod_id;
        mod_node.name = module_name;
        mod_node.type = "module";
        mod_node.module = module_name;
        graph.add_node(mod_node);
    }
    graph.add_edge(mod_id, file_node.id, "contains");

    // 1. Record Imports / Includes (on raw content because import paths are in strings!)
    static const std::regex inc_cpp(R"(#include\s*["<]([^">]+)[">])");
    static const std::regex imp_js(R"((?:import\s+.*?\s+from\s*['"]([^'"]+)['"]|require\s*\(\s*['"]([^'"]+)['"]\s*\)))");
    static const std::regex imp_py(R"((?:from\s+([a-zA-Z0-9_\.]+)\s+import|import\s+([a-zA-Z0-9_\.]+)))");
    static const std::regex imp_dart(R"((?:import|part(?:\s+of)?|export)\s+['"]([^'"]+)['"])");
    static const std::regex imp_cs(R"(using\s+([a-zA-Z0-9_\.]+)\s*;)");

    auto process_import = [&](const std::string& imported_target) {
        fs::path imp_p(imported_target);
        std::string fname = imp_p.filename().string();
        if (!fname.empty()) {
            file_imports[file_node.id].push_back(fname);
        }
    };

    if (ext == ".cpp" || ext == ".hpp" || ext == ".h" || ext == ".cc" || ext == ".c") {
        for (std::sregex_iterator i(content.begin(), content.end(), inc_cpp); i != std::sregex_iterator(); ++i) {
            process_import((*i)[1].str());
        }
    } else if (ext == ".dart") {
        for (std::sregex_iterator i(content.begin(), content.end(), imp_dart); i != std::sregex_iterator(); ++i) {
            process_import((*i)[1].str());
        }
    } else if (ext == ".cs") {
        for (std::sregex_iterator i(content.begin(), content.end(), imp_cs); i != std::sregex_iterator(); ++i) {
            // C# using Namespace.Sub -> also push Sub.cs candidate
            std::string ns = (*i)[1].str();
            size_t dot = ns.rfind('.');
            std::string last = (dot != std::string::npos) ? ns.substr(dot + 1) : ns;
            file_imports[file_node.id].push_back(last + ".cs");
        }
    } else if (ext == ".js" || ext == ".ts") {
        for (std::sregex_iterator i(content.begin(), content.end(), imp_js); i != std::sregex_iterator(); ++i) {
            std::string t = (*i)[1].matched ? (*i)[1].str() : (*i)[2].str();
            process_import(t);
        }
    } else if (ext == ".py") {
        for (std::sregex_iterator i(content.begin(), content.end(), imp_py); i != std::sregex_iterator(); ++i) {
            std::string t = (*i)[1].matched ? (*i)[1].str() : (*i)[2].str();
            size_t dot = t.rfind('.');
            std::string last = (dot != std::string::npos) ? t.substr(dot + 1) : t;
            file_imports[file_node.id].push_back(last + ".py");
        }
    }

    // 2. Parse Functions with Exact Brace/Indentation Boundaries
    static const std::unordered_set<std::string> control_words = {
        "if", "while", "for", "switch", "catch", "elif", "return", "sizeof", "alignof", "decltype"
    };

    struct FnSpan {
        std::string name;
        size_t start;
        size_t end;
    };
    std::vector<FnSpan> spans;

    if (ext == ".py") {
        static const std::regex py_fn(R"((?:^|\n)([ \t]*)(?:async\s+)?def\s+([a-zA-Z_][a-zA-Z0-9_]*)\s*\([^)]*\)\s*(?:->[^:]+)?\s*:)");
        for (std::sregex_iterator it(clean_content.begin(), clean_content.end(), py_fn); it != std::sregex_iterator(); ++it) {
            size_t indent = (*it)[1].str().size();
            std::string fn_name = (*it)[2].str();
            size_t start = static_cast<size_t>(it->position());
            size_t header_end = start + static_cast<size_t>(it->length());

            // Walk lines until indentation <= indent (and non-empty line)
            size_t pos = header_end;
            size_t end = clean_content.size();
            while (pos < clean_content.size()) {
                size_t nl = clean_content.find('\n', pos);
                if (nl == std::string::npos) nl = clean_content.size();
                std::string line_str = clean_content.substr(pos, nl - pos);
                size_t first_ns = line_str.find_first_not_of(" \t\r");
                if (first_ns != std::string::npos && first_ns <= indent) {
                    end = pos;
                    break;
                }
                pos = (nl < clean_content.size()) ? nl + 1 : nl;
            }
            spans.push_back({fn_name, start, end});
        }
    } else {
        // Brace-based languages (C++, Dart, C#, JS/TS)
        static const std::regex brace_fn(
            R"((?:^|\n)[ \t]*(?:(?:public|private|protected|internal|virtual|override|static|inline|constexpr|explicit|async|export|default|function|final|const|extern)\s+)*(?:[a-zA-Z_][a-zA-Z0-9_:<>,\*\?\[\]\s]*\s+)?(?:[a-zA-Z_][a-zA-Z0-9_]*::)?([a-zA-Z_][a-zA-Z0-9_]*)\s*\([^;{}]*\)\s*(?:const|override|final|async)?\s*\{)"
        );

        size_t min_next_pos = 0;
        for (std::sregex_iterator it(clean_content.begin(), clean_content.end(), brace_fn); it != std::sregex_iterator(); ++it) {
            size_t match_pos = static_cast<size_t>(it->position());
            if (match_pos < min_next_pos) continue;

            std::string fn_name = (*it)[1].str();
            if (control_words.count(fn_name)) continue;

            size_t open_brace = match_pos + static_cast<size_t>(it->length()) - 1;
            int depth = 1;
            size_t cur = open_brace + 1;
            while (cur < clean_content.size() && depth > 0) {
                if (clean_content[cur] == '{') depth++;
                else if (clean_content[cur] == '}') depth--;
                cur++;
            }
            if (depth == 0) {
                spans.push_back({fn_name, match_pos, cur});
                min_next_pos = cur;
            }
        }
    }

    std::unordered_map<std::string, int> fn_name_counts;
    for (const auto& sp : spans) {
        std::string clean_body = clean_content.substr(sp.start, sp.end - sp.start);
        int dup_idx = fn_name_counts[sp.name]++;
        std::string unique_suffix = (dup_idx > 0) ? ("#" + std::to_string(dup_idx)) : "";

        Node fn_node;
        fn_node.id = "fn:" + rel_path + ":" + sp.name + unique_suffix;
        fn_node.name = sp.name;
        fn_node.type = "function";
        fn_node.file_path = rel_path;
        fn_node.module = module_name;
        fn_node.line_start = count_newlines_up_to(clean_content, sp.start);
        fn_node.line_end = count_newlines_up_to(clean_content, sp.end);
        fn_node.loc = std::max(1, fn_node.line_end - fn_node.line_start + 1);
        fn_node.cyclomatic_complexity = calculate_complexity(clean_body);

        extract_fingerprints(clean_body, sp.name, fn_node);

        std::string pass_target;
        if (check_pass_through(clean_body, pass_target)) {
            fn_node.is_pass_through = true;
            fn_node.pass_through_target = pass_target;
        }

        graph.add_node(fn_node);
        graph.add_edge(file_node.id, fn_node.id, "contains");
    }
}

void CodeParser::parse_directory(const std::string& root_dir, Graph& graph) {
    if (!fs::exists(root_dir)) {
        std::cerr << "Directory does not exist: " << root_dir << std::endl;
        return;
    }

    file_imports.clear();
    std::vector<fs::path> paths;
    for (const auto& entry : fs::recursive_directory_iterator(root_dir)) {
        if (entry.is_regular_file()) {
            std::string ext = entry.path().extension().string();
            std::string p_str = entry.path().string();
            if (p_str.find("/.") != std::string::npos ||
                p_str.find("node_modules") != std::string::npos ||
                p_str.find("/build/") != std::string::npos ||
                p_str.find("/dist/") != std::string::npos ||
                p_str.find("/fixtures/") != std::string::npos ||
                p_str.find("/sample_imports/") != std::string::npos ||
                p_str.find("/test_cache/") != std::string::npos) {
                continue;
            }
            if (ext == ".cpp" || ext == ".hpp" || ext == ".h" || ext == ".cc" || ext == ".c" ||
                ext == ".dart" || ext == ".cs" || ext == ".js" || ext == ".ts" || ext == ".py") {
                paths.push_back(entry.path());
            }
        }
    }

    std::cout << "    [+] Topilgan manba fayllari: " << paths.size() << " ta\n";

    for (const auto& p : paths) {
        parse_file(p, root_dir, graph);
    }

    // Build O(1) indices
    std::unordered_map<std::string, std::vector<std::string>> file_by_name;
    std::unordered_map<std::string, std::vector<std::string>> fn_by_name;
    std::unordered_map<std::string, std::string> file_id_by_fn;

    for (const auto& pair : graph.nodes) {
        if (pair.second.type == "file") {
            file_by_name[pair.second.name].push_back(pair.first);
        } else if (pair.second.type == "function") {
            fn_by_name[pair.second.name].push_back(pair.first);
            file_id_by_fn[pair.first] = "file:" + pair.second.file_path;
        }
    }

    // Resolve file imports
    for (const auto& imp_pair : file_imports) {
        const std::string& src_file_id = imp_pair.first;
        const std::string& src_mod = graph.nodes[src_file_id].module;

        for (const std::string& target_filename : imp_pair.second) {
            auto it = file_by_name.find(target_filename);
            if (it != file_by_name.end()) {
                for (const std::string& target_file_id : it->second) {
                    if (target_file_id != src_file_id) {
                        graph.add_edge(src_file_id, target_file_id, "imports");
                        const std::string& tgt_mod = graph.nodes[target_file_id].module;
                        if (src_mod != tgt_mod) {
                            graph.add_edge("module:" + src_mod, "module:" + tgt_mod, "imports");
                        }
                    }
                }
            }
        }
    }

    // Common boilerplate / framework identifiers that cause false-positive cycles
    static const std::unordered_set<std::string> BOILERPLATE_NAMES = {
        "build", "dispose", "shouldRepaint", "paint", "toString", "hashCode", "equals",
        "main", "init", "reset", "update", "apply", "Text", "Color", "Icon", "SizedBox",
        "Offset", "Clear", "WriteString", "add", "first", "begin", "trim", "expect", "fail",
        "clear", "redo", "insert", "dot", "validateAll", "addListener", "near", "copy",
        "copyWith", "parse", "render", "push", "pop", "set", "get", "size", "length",
        "empty", "end", "find", "count", "substr", "append", "reserve", "resize", "erase"
    };

    // Resolve function-to-function calls (and propagate file-to-file call edges if distinct)
    for (const auto& pair : graph.nodes) {
        if (pair.second.type == "function") {
            const Node& src_fn = pair.second;
            for (const std::string& called_name : src_fn.called_identifiers) {
                if (BOILERPLATE_NAMES.count(called_name)) continue;

                auto it = fn_by_name.find(called_name);
                if (it != fn_by_name.end()) {
                    for (const std::string& target_id : it->second) {
                        if (target_id != src_fn.id) {
                            graph.add_edge(src_fn.id, target_id, "calls");
                            const std::string& tgt_mod = graph.nodes[target_id].module;
                            if (src_fn.module != tgt_mod) {
                                graph.add_edge("module:" + src_fn.module, "module:" + tgt_mod, "calls");
                            }
                        }
                    }
                }
            }
        }
    }

    graph.rebuild_adjacencies();
}
