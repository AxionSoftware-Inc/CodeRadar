#include "parser.hpp"
#include <fstream>
#include <sstream>
#include <regex>
#include <iostream>
#include <algorithm>
#include <cmath>
#include <cctype>
#include <thread>
#include <chrono>
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
        "Set", "Future", "Task", "Widget", "BuildContext", "dynamic", "Object", "fun", "val", "object", "companion"
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

std::string CodeParser::determine_module(const fs::path& file_path, const std::string& root_dir) const {
    fs::path rel = fs::relative(file_path, root_dir);
    std::vector<std::string> parts;
    for (const auto& p : rel) {
        parts.push_back(p.string());
    }
    if (parts.empty()) return "root";

    // Handle deep feature/plugin/android subtrees inside apps/<app>/... (e.g. Flutter + Kotlin)
    if (parts.size() >= 2 && (parts[0] == "apps" || parts[0] == "packages" || parts[0] == "tools")) {
        // Check for apps/<app>/lib/src/features/<feature>/... or plugins/<plugin>/...
        for (size_t i = 2; i + 1 < parts.size(); ++i) {
            if ((parts[i] == "features" || parts[i] == "plugins") && i + 2 < parts.size()) {
                return parts[1] + "/" + parts[i + 1];
            }
            if (parts[i] == "src" && i + 1 < parts.size() - 1 &&
                parts[i + 1] != "features" && parts[i + 1] != "plugins") {
                return parts[1] + "/" + parts[i + 1];
            }
            if (parts[i] == "android" && i + 1 < parts.size()) {
                return parts[1] + "/android";
            }
            if (parts[i] == "test" || parts[i] == "integration_test") {
                return parts[1] + "/tests";
            }
        }
        if (parts.size() >= 3) {
            return parts[0] + "/" + parts[1];
        }
    }

    if (parts.size() >= 3 && (parts[0] == "src" || parts[0] == "lib")) {
        return parts[0] + "/" + parts[1];
    }
    return parts[0];
}

std::string CodeParser::strip_comments_and_strings(const std::string& code, const std::string& ext) const {
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

int CodeParser::calculate_complexity(const std::string& clean_body) const {
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

void CodeParser::extract_fingerprints(const std::string& clean_body, const std::string& fn_name, Node& fn_node) const {
    const auto& keywords = get_keywords();
    std::unordered_map<std::string, std::string> var_alias;
    std::unordered_map<std::string, std::string> var_types;
    int var_counter = 0;

    std::vector<std::string> norm_seq;
    fn_node.token_vector.clear();
    fn_node.norm_token_vector.clear();
    fn_node.wl_vector.clear();
    fn_node.shingle_hashes.clear();
    fn_node.cfg_sequence.clear();
    fn_node.called_identifiers.clear();
    fn_node.qualified_calls.clear();
    fn_node.behavior_profile.fill(0.0);

    std::unordered_set<std::string> seen_calls;
    std::unordered_set<std::string> seen_qual_calls;
    size_t n = clean_body.size();
    size_t i = 0;
    std::string prev_ident;
    bool prev_member_op = false;

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

        // Track member/scope operators (., ->, ::) for qualified call resolution
        if (c == '.') {
            prev_member_op = true;
            i++;
            continue;
        }
        if (i + 1 < n && ((c == '-' && clean_body[i + 1] == '>') || (c == ':' && clean_body[i + 1] == ':'))) {
            prev_member_op = true;
            norm_seq.push_back(std::string(1, c) + clean_body[i + 1]);
            fn_node.norm_token_vector[std::string(1, c) + clean_body[i + 1]]++;
            i += 2;
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
                prev_ident.clear();
                prev_member_op = false;
                continue;
            }

            // Look ahead for '(' to detect function call
            size_t j = i;
            while (j < n && std::isspace(static_cast<unsigned char>(clean_body[j]))) j++;
            bool is_call = (j < n && clean_body[j] == '(');

            if (keywords.count(tok)) {
                norm_seq.push_back(tok);
                fn_node.norm_token_vector[tok]++;
                prev_ident.clear();
                prev_member_op = false;

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
                    if (prev_member_op && !prev_ident.empty()) {
                        std::string resolved_qual = prev_ident;
                        auto vt_it = var_types.find(prev_ident);
                        if (vt_it != var_types.end()) {
                            resolved_qual = vt_it->second;
                        }
                        std::string qkey = resolved_qual + "\x1f" + tok;
                        if (seen_qual_calls.insert(qkey).second) {
                            fn_node.qualified_calls.push_back({resolved_qual, tok});
                        }
                    }
                    // Keep call arity/role normalized as $CALL_x or $CALL
                    norm_seq.push_back("$CALL");
                    fn_node.norm_token_vector["$CALL"]++;
                    prev_ident.clear();
                    prev_member_op = false;
                } else {
                    // Track "TypeName varName" declarations (e.g. "AlphaService svc;") for receiver resolution
                    if (!prev_member_op && !prev_ident.empty() &&
                        std::isupper(static_cast<unsigned char>(prev_ident[0]))) {
                        var_types[tok] = prev_ident;
                    }
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
                    prev_ident = tok;
                    prev_member_op = false;
                }
            }
            continue;
        }

        prev_ident.clear();
        prev_member_op = false;

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

    // Build order-invariant WL 1-hop neighborhood hashes & 5-token sliding-window shingles
    // Replace positional $V0..$V15 with $VAR so variable declaration order does not alter hashes
    std::vector<std::string> role_seq;
    role_seq.reserve(norm_seq.size());
    for (const auto& t : norm_seq) {
        if (t.size() >= 3 && t[0] == '$' && t[1] == 'V' && std::isdigit(static_cast<unsigned char>(t[2]))) {
            role_seq.push_back("$VAR");
        } else {
            role_seq.push_back(t);
        }
    }
    if (role_seq.size() >= 3) {
        for (size_t k = 1; k + 1 < role_seq.size(); ++k) {
            uint64_t wh = fnv1a_hash({role_seq[k - 1], role_seq[k], role_seq[k + 1]});
            fn_node.wl_vector[wh]++;
        }
    }
    if (role_seq.size() >= 5) {
        std::unordered_set<uint64_t> uniq_shingles;
        for (size_t k = 0; k + 4 < role_seq.size(); ++k) {
            uint64_t sh = fnv1a_hash({role_seq[k], role_seq[k + 1], role_seq[k + 2], role_seq[k + 3], role_seq[k + 4]});
            uniq_shingles.insert(sh);
        }
        fn_node.shingle_hashes.assign(uniq_shingles.begin(), uniq_shingles.end());
        std::sort(fn_node.shingle_hashes.begin(), fn_node.shingle_hashes.end());
    }
}

bool CodeParser::check_pass_through(const std::string& clean_body, std::string& target_call) const {
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

CodeParser::ParsedFileResult CodeParser::parse_file_isolated(const fs::path& file_path, const std::string& root_dir) const {
    ParsedFileResult res;
    std::ifstream file(file_path);
    if (!file.is_open()) return res;

    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    std::string ext = file_path.extension().string();
    res.rel_path = fs::relative(file_path, root_dir).string();
    res.module_name = determine_module(file_path, root_dir);

    std::error_code ec;
    auto ftime = fs::last_write_time(file_path, ec);
    res.mtime_ns = ec ? 0 : static_cast<int64_t>(ftime.time_since_epoch().count());
    res.file_size = static_cast<uint64_t>(content.size());

    std::string clean_content = strip_comments_and_strings(content, ext);

    // Create file node
    res.file_node.id = "file:" + res.rel_path;
    res.file_node.name = file_path.filename().string();
    res.file_node.type = "file";
    res.file_node.file_path = res.rel_path;
    res.file_node.module = res.module_name;

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
    res.file_node.loc = loc;
    res.file_node.cyclomatic_complexity = calculate_complexity(clean_content);

    // 1. Record Imports / Includes (on raw content because import paths are in strings!)
    static const std::regex inc_cpp(R"(#include\s*["<]([^">]+)[">])");
    static const std::regex imp_js(R"((?:import\s+.*?\s+from\s*['"]([^'"]+)['"]|require\s*\(\s*['"]([^'"]+)['"]\s*\)))");
    static const std::regex imp_py(R"((?:from\s+([a-zA-Z0-9_\.]+)\s+import|import\s+([a-zA-Z0-9_\.]+)))");
    static const std::regex imp_dart(R"((?:import|part(?:\s+of)?|export)\s+['"]([^'"]+)['"])");
    static const std::regex imp_cs(R"(using\s+([a-zA-Z0-9_\.]+)\s*;)");
    static const std::regex imp_kt(R"(import\s+([a-zA-Z0-9_\.]+))");

    auto process_import = [&](const std::string& imported_target) {
        fs::path imp_p(imported_target);
        std::string fname = imp_p.filename().string();
        if (!fname.empty()) {
            res.imports.push_back(fname);
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
            std::string ns = (*i)[1].str();
            size_t dot = ns.rfind('.');
            std::string last = (dot != std::string::npos) ? ns.substr(dot + 1) : ns;
            res.imports.push_back(last + ".cs");
        }
    } else if (ext == ".kt" || ext == ".kts") {
        for (std::sregex_iterator i(content.begin(), content.end(), imp_kt); i != std::sregex_iterator(); ++i) {
            std::string ns = (*i)[1].str();
            size_t dot = ns.rfind('.');
            std::string last = (dot != std::string::npos) ? ns.substr(dot + 1) : ns;
            res.imports.push_back(last + ".kt");
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
            res.imports.push_back(last + ".py");
        }
    }

    // 2. Detect Class/Struct/Namespace Scopes for Scope-Aware Call Resolution
    struct ScopeSpan {
        std::string name;
        size_t start;
        size_t end;
    };
    std::vector<ScopeSpan> scopes;
    if (ext != ".py") {
        static const std::regex scope_re(R"((?:^|\n)[ \t]*(?:export\s+)?(?:abstract\s+)?(?:class|struct|namespace)\s+([a-zA-Z_][a-zA-Z0-9_]*)[^{;]*\{)");
        for (std::sregex_iterator it(clean_content.begin(), clean_content.end(), scope_re); it != std::sregex_iterator(); ++it) {
            size_t match_pos = static_cast<size_t>(it->position());
            size_t open_brace = match_pos + static_cast<size_t>(it->length()) - 1;
            int depth = 1;
            size_t cur = open_brace + 1;
            while (cur < clean_content.size() && depth > 0) {
                if (clean_content[cur] == '{') depth++;
                else if (clean_content[cur] == '}') depth--;
                cur++;
            }
            if (depth == 0) {
                scopes.push_back({(*it)[1].str(), match_pos, cur});
            }
        }
    } else {
        static const std::regex py_cls(R"((?:^|\n)([ \t]*)class\s+([a-zA-Z_][a-zA-Z0-9_]*)\s*(?:\([^)]*\))?\s*:)");
        for (std::sregex_iterator it(clean_content.begin(), clean_content.end(), py_cls); it != std::sregex_iterator(); ++it) {
            size_t indent = (*it)[1].str().size();
            size_t start = static_cast<size_t>(it->position());
            size_t pos = start + static_cast<size_t>(it->length());
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
            scopes.push_back({(*it)[2].str(), start, end});
        }
    }

    // 3. Parse Functions with Exact Brace/Indentation Boundaries
    static const std::unordered_set<std::string> control_words = {
        "if", "while", "for", "switch", "catch", "elif", "return", "sizeof", "alignof", "decltype"
    };

    struct FnSpan {
        std::string name;
        std::string explicit_scope;
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
            spans.push_back({fn_name, "", start, end});
        }
    } else {
        static const std::regex brace_fn(
            R"((?:^|\n)[ \t]*(?:template\s*<[^;{}]*>\s*)?(?:(?:public|private|protected|internal|virtual|override|static|inline|constexpr|explicit|async|export|default|function|final|const|extern)\s+)*(?:[a-zA-Z_][a-zA-Z0-9_:<>,\*\?\[\]\s]*\s+)?(?:([a-zA-Z_][a-zA-Z0-9_]*)::)?([a-zA-Z_][a-zA-Z0-9_]*)\s*\([^;{}]*\)\s*(?:const|override|final|async|noexcept)?(?:\s*->\s*[a-zA-Z_][a-zA-Z0-9_:<>,\*\?\[\]\s]*)?(?:\s*:\s*[^{;]+)?\s*\{)"
        );

        auto extract_brace_body = [&](size_t open_brace) -> size_t {
            int depth = 1;
            size_t cur = open_brace + 1;
            while (cur < clean_content.size() && depth > 0) {
                if (clean_content[cur] == '{') depth++;
                else if (clean_content[cur] == '}') depth--;
                cur++;
            }
            return (depth == 0) ? cur : 0;
        };

        size_t min_next_pos = 0;
        for (std::sregex_iterator it(clean_content.begin(), clean_content.end(), brace_fn); it != std::sregex_iterator(); ++it) {
            size_t match_pos = static_cast<size_t>(it->position());
            if (match_pos < min_next_pos) continue;

            std::string explicit_scope = (*it)[1].matched ? (*it)[1].str() : "";
            std::string fn_name = (*it)[2].str();
            if (control_words.count(fn_name)) continue;

            size_t open_brace = match_pos + static_cast<size_t>(it->length()) - 1;
            size_t cur = extract_brace_body(open_brace);
            if (cur > 0) {
                spans.push_back({fn_name, explicit_scope, match_pos, cur});
                min_next_pos = cur;
            }
        }

        if (ext == ".js" || ext == ".ts") {
            static const std::regex arrow_fn(
                R"((?:^|\n)[ \t]*(?:export\s+)?(?:const|let|var)\s+([a-zA-Z_][a-zA-Z0-9_]*)\s*=\s*(?:async\s+)?(?:\([^;{}]*\)|[a-zA-Z_][a-zA-Z0-9_]*)\s*(?::\s*[^=>{;\n]+)?\s*=>\s*\{)"
            );
            for (std::sregex_iterator it(clean_content.begin(), clean_content.end(), arrow_fn); it != std::sregex_iterator(); ++it) {
                size_t match_pos = static_cast<size_t>(it->position());
                bool overlaps = false;
                for (const auto& sp : spans) {
                    if (match_pos >= sp.start && match_pos < sp.end) {
                        overlaps = true;
                        break;
                    }
                }
                if (overlaps) continue;

                std::string fn_name = (*it)[1].str();
                if (control_words.count(fn_name)) continue;

                size_t open_brace = match_pos + static_cast<size_t>(it->length()) - 1;
                size_t cur = extract_brace_body(open_brace);
                if (cur > 0) {
                    spans.push_back({fn_name, "", match_pos, cur});
                }
            }
        }
    }

    std::unordered_map<std::string, int> fn_name_counts;
    for (const auto& sp : spans) {
        std::string clean_body = clean_content.substr(sp.start, sp.end - sp.start);
        int dup_idx = fn_name_counts[sp.name]++;
        std::string unique_suffix = (dup_idx > 0) ? ("#" + std::to_string(dup_idx)) : "";

        Node fn_node;
        fn_node.id = "fn:" + res.rel_path + ":" + sp.name + unique_suffix;
        fn_node.name = sp.name;
        fn_node.type = "function";
        fn_node.file_path = res.rel_path;
        fn_node.module = res.module_name;

        if (!sp.explicit_scope.empty()) {
            fn_node.scope = sp.explicit_scope;
        } else {
            // Find innermost enclosing class/struct/namespace
            size_t best_len = SIZE_MAX;
            for (const auto& sc : scopes) {
                if (sp.start >= sc.start && sp.end <= sc.end) {
                    size_t len = sc.end - sc.start;
                    if (len < best_len) {
                        best_len = len;
                        fn_node.scope = sc.name;
                    }
                }
            }
        }

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

        res.fn_nodes.push_back(std::move(fn_node));
    }

    return res;
}

namespace {
    void write_str_bin(std::ostream& out, const std::string& s) {
        uint32_t len = static_cast<uint32_t>(s.size());
        out.write(reinterpret_cast<const char*>(&len), sizeof(len));
        if (len > 0) out.write(s.data(), len);
    }
    bool read_str_bin(std::istream& in, std::string& s) {
        uint32_t len = 0;
        if (!in.read(reinterpret_cast<char*>(&len), sizeof(len))) return false;
        if (len > 10000000) return false;
        s.resize(len);
        if (len > 0 && !in.read(&s[0], len)) return false;
        return true;
    }
    void write_node_bin(std::ostream& out, const Node& n) {
        write_str_bin(out, n.id);
        write_str_bin(out, n.name);
        write_str_bin(out, n.type);
        write_str_bin(out, n.file_path);
        write_str_bin(out, n.module);
        write_str_bin(out, n.scope);
        out.write(reinterpret_cast<const char*>(&n.line_start), sizeof(n.line_start));
        out.write(reinterpret_cast<const char*>(&n.line_end), sizeof(n.line_end));
        out.write(reinterpret_cast<const char*>(&n.loc), sizeof(n.loc));
        out.write(reinterpret_cast<const char*>(&n.cyclomatic_complexity), sizeof(n.cyclomatic_complexity));
        out.write(reinterpret_cast<const char*>(&n.exact_hash), sizeof(n.exact_hash));
        out.write(reinterpret_cast<const char*>(&n.is_pass_through), sizeof(n.is_pass_through));
        write_str_bin(out, n.pass_through_target);
        out.write(reinterpret_cast<const char*>(n.behavior_profile.data()), sizeof(double) * 8);

        uint32_t nt_cnt = static_cast<uint32_t>(n.norm_token_vector.size());
        out.write(reinterpret_cast<const char*>(&nt_cnt), sizeof(nt_cnt));
        for (const auto& kv : n.norm_token_vector) {
            write_str_bin(out, kv.first);
            out.write(reinterpret_cast<const char*>(&kv.second), sizeof(kv.second));
        }

        uint32_t cfg_cnt = static_cast<uint32_t>(n.cfg_sequence.size());
        out.write(reinterpret_cast<const char*>(&cfg_cnt), sizeof(cfg_cnt));
        for (const auto& s : n.cfg_sequence) write_str_bin(out, s);

        uint32_t call_cnt = static_cast<uint32_t>(n.called_identifiers.size());
        out.write(reinterpret_cast<const char*>(&call_cnt), sizeof(call_cnt));
        for (const auto& s : n.called_identifiers) write_str_bin(out, s);

        uint32_t q_cnt = static_cast<uint32_t>(n.qualified_calls.size());
        out.write(reinterpret_cast<const char*>(&q_cnt), sizeof(q_cnt));
        for (const auto& qp : n.qualified_calls) {
            write_str_bin(out, qp.first);
            write_str_bin(out, qp.second);
        }

        uint32_t wl_cnt = static_cast<uint32_t>(n.wl_vector.size());
        out.write(reinterpret_cast<const char*>(&wl_cnt), sizeof(wl_cnt));
        for (const auto& kv : n.wl_vector) {
            out.write(reinterpret_cast<const char*>(&kv.first), sizeof(kv.first));
            out.write(reinterpret_cast<const char*>(&kv.second), sizeof(kv.second));
        }

        uint32_t sh_cnt = static_cast<uint32_t>(n.shingle_hashes.size());
        out.write(reinterpret_cast<const char*>(&sh_cnt), sizeof(sh_cnt));
        if (sh_cnt > 0) {
            out.write(reinterpret_cast<const char*>(n.shingle_hashes.data()), sh_cnt * sizeof(uint64_t));
        }
    }
    bool read_node_bin(std::istream& in, Node& n) {
        if (!read_str_bin(in, n.id) || !read_str_bin(in, n.name) || !read_str_bin(in, n.type) ||
            !read_str_bin(in, n.file_path) || !read_str_bin(in, n.module) || !read_str_bin(in, n.scope)) return false;
        if (!in.read(reinterpret_cast<char*>(&n.line_start), sizeof(n.line_start))) return false;
        if (!in.read(reinterpret_cast<char*>(&n.line_end), sizeof(n.line_end))) return false;
        if (!in.read(reinterpret_cast<char*>(&n.loc), sizeof(n.loc))) return false;
        if (!in.read(reinterpret_cast<char*>(&n.cyclomatic_complexity), sizeof(n.cyclomatic_complexity))) return false;
        if (!in.read(reinterpret_cast<char*>(&n.exact_hash), sizeof(n.exact_hash))) return false;
        if (!in.read(reinterpret_cast<char*>(&n.is_pass_through), sizeof(n.is_pass_through))) return false;
        if (!read_str_bin(in, n.pass_through_target)) return false;
        if (!in.read(reinterpret_cast<char*>(n.behavior_profile.data()), sizeof(double) * 8)) return false;

        uint32_t nt_cnt = 0;
        if (!in.read(reinterpret_cast<char*>(&nt_cnt), sizeof(nt_cnt))) return false;
        for (uint32_t i = 0; i < nt_cnt; ++i) {
            std::string k; int v = 0;
            if (!read_str_bin(in, k) || !in.read(reinterpret_cast<char*>(&v), sizeof(v))) return false;
            n.norm_token_vector[std::move(k)] = v;
        }
        uint32_t cfg_cnt = 0;
        if (!in.read(reinterpret_cast<char*>(&cfg_cnt), sizeof(cfg_cnt))) return false;
        n.cfg_sequence.resize(cfg_cnt);
        for (uint32_t i = 0; i < cfg_cnt; ++i) if (!read_str_bin(in, n.cfg_sequence[i])) return false;

        uint32_t call_cnt = 0;
        if (!in.read(reinterpret_cast<char*>(&call_cnt), sizeof(call_cnt))) return false;
        n.called_identifiers.resize(call_cnt);
        for (uint32_t i = 0; i < call_cnt; ++i) if (!read_str_bin(in, n.called_identifiers[i])) return false;

        uint32_t q_cnt = 0;
        if (!in.read(reinterpret_cast<char*>(&q_cnt), sizeof(q_cnt))) return false;
        n.qualified_calls.resize(q_cnt);
        for (uint32_t i = 0; i < q_cnt; ++i) {
            if (!read_str_bin(in, n.qualified_calls[i].first) || !read_str_bin(in, n.qualified_calls[i].second)) return false;
        }

        uint32_t wl_cnt = 0;
        if (!in.read(reinterpret_cast<char*>(&wl_cnt), sizeof(wl_cnt))) return false;
        for (uint32_t i = 0; i < wl_cnt; ++i) {
            uint64_t k = 0; int v = 0;
            if (!in.read(reinterpret_cast<char*>(&k), sizeof(k)) || !in.read(reinterpret_cast<char*>(&v), sizeof(v))) return false;
            n.wl_vector[k] = v;
        }

        uint32_t sh_cnt = 0;
        if (!in.read(reinterpret_cast<char*>(&sh_cnt), sizeof(sh_cnt))) return false;
        n.shingle_hashes.resize(sh_cnt);
        if (sh_cnt > 0 && !in.read(reinterpret_cast<char*>(n.shingle_hashes.data()), sh_cnt * sizeof(uint64_t))) return false;
        return true;
    }
}

bool CodeParser::load_cache(const std::string& cache_path, std::unordered_map<std::string, ParsedFileResult>& cache_map) {
    std::ifstream in(cache_path, std::ios::binary);
    if (!in.is_open()) return false;
    char magic[4];
    if (!in.read(magic, 4) || std::string(magic, 4) != "ARC2") return false;
    uint32_t count = 0;
    if (!in.read(reinterpret_cast<char*>(&count), sizeof(count))) return false;
    for (uint32_t i = 0; i < count; ++i) {
        ParsedFileResult r;
        if (!read_str_bin(in, r.rel_path)) return false;
        if (!in.read(reinterpret_cast<char*>(&r.mtime_ns), sizeof(r.mtime_ns))) return false;
        if (!in.read(reinterpret_cast<char*>(&r.file_size), sizeof(r.file_size))) return false;
        if (!read_str_bin(in, r.module_name)) return false;
        if (!read_node_bin(in, r.file_node)) return false;
        uint32_t fn_cnt = 0;
        if (!in.read(reinterpret_cast<char*>(&fn_cnt), sizeof(fn_cnt))) return false;
        r.fn_nodes.resize(fn_cnt);
        for (uint32_t f = 0; f < fn_cnt; ++f) if (!read_node_bin(in, r.fn_nodes[f])) return false;
        uint32_t imp_cnt = 0;
        if (!in.read(reinterpret_cast<char*>(&imp_cnt), sizeof(imp_cnt))) return false;
        r.imports.resize(imp_cnt);
        for (uint32_t im = 0; im < imp_cnt; ++im) if (!read_str_bin(in, r.imports[im])) return false;
        cache_map[r.rel_path] = std::move(r);
    }
    return true;
}

bool CodeParser::save_cache(const std::string& cache_path, const std::unordered_map<std::string, ParsedFileResult>& cache_map) {
    std::ofstream out(cache_path, std::ios::binary);
    if (!out.is_open()) return false;
    out.write("ARC2", 4);
    uint32_t count = static_cast<uint32_t>(cache_map.size());
    out.write(reinterpret_cast<const char*>(&count), sizeof(count));
    for (const auto& kv : cache_map) {
        const auto& r = kv.second;
        write_str_bin(out, r.rel_path);
        out.write(reinterpret_cast<const char*>(&r.mtime_ns), sizeof(r.mtime_ns));
        out.write(reinterpret_cast<const char*>(&r.file_size), sizeof(r.file_size));
        write_str_bin(out, r.module_name);
        write_node_bin(out, r.file_node);
        uint32_t fn_cnt = static_cast<uint32_t>(r.fn_nodes.size());
        out.write(reinterpret_cast<const char*>(&fn_cnt), sizeof(fn_cnt));
        for (const auto& fn : r.fn_nodes) write_node_bin(out, fn);
        uint32_t imp_cnt = static_cast<uint32_t>(r.imports.size());
        out.write(reinterpret_cast<const char*>(&imp_cnt), sizeof(imp_cnt));
        for (const auto& im : r.imports) write_str_bin(out, im);
    }
    return true;
}

void CodeParser::parse_directory(const std::string& root_dir, Graph& graph, bool use_cache, bool quiet) {
    if (!fs::exists(root_dir)) {
        std::cerr << "Directory does not exist: " << root_dir << std::endl;
        return;
    }

    std::vector<fs::path> paths;
    for (const auto& entry : fs::recursive_directory_iterator(root_dir)) {
        if (entry.is_regular_file()) {
            std::string ext = entry.path().extension().string();
            std::string p_str = entry.path().string();
            if (p_str.find("/.") != std::string::npos ||
                p_str.find("node_modules") != std::string::npos ||
                p_str.find("/build/") != std::string::npos ||
                p_str.find("/dist/") != std::string::npos ||
                p_str.find("/__tests__/") != std::string::npos ||
                p_str.find(".spec.") != std::string::npos ||
                p_str.find(".test.") != std::string::npos ||
                p_str.find("/fixtures/") != std::string::npos ||
                p_str.find("/sample_imports/") != std::string::npos ||
                p_str.find("/test_cache/") != std::string::npos) {
                continue;
            }
            if (ext == ".cpp" || ext == ".hpp" || ext == ".h" || ext == ".cc" || ext == ".c" ||
                ext == ".dart" || ext == ".cs" || ext == ".kt" || ext == ".kts" ||
                ext == ".js" || ext == ".ts" || ext == ".py") {
                paths.push_back(entry.path());
            }
        }
    }

    std::string canon_root = fs::weakly_canonical(root_dir).string();
    uint64_t root_hash = fnv1a_hash({canon_root});
    std::string cache_path = "/tmp/arch_radar_cache_" + std::to_string(root_hash) + ".bin";

    std::unordered_map<std::string, ParsedFileResult> cache_map;
    if (use_cache) {
        load_cache(cache_path, cache_map);
    }

    std::vector<ParsedFileResult> results(paths.size());
    std::vector<size_t> to_parse_indices;
    size_t cache_hits = 0;

    for (size_t i = 0; i < paths.size(); ++i) {
        std::string rel = fs::relative(paths[i], root_dir).string();
        std::error_code ec;
        auto ftime = fs::last_write_time(paths[i], ec);
        int64_t mtime_ns = ec ? 0 : static_cast<int64_t>(ftime.time_since_epoch().count());
        uint64_t fsize = ec ? 0 : static_cast<uint64_t>(fs::file_size(paths[i], ec));

        auto cit = cache_map.find(rel);
        if (use_cache && cit != cache_map.end() && cit->second.mtime_ns == mtime_ns && cit->second.file_size == fsize) {
            results[i] = cit->second;
            cache_hits++;
        } else {
            to_parse_indices.push_back(i);
        }
    }

    unsigned int hw = std::thread::hardware_concurrency();
    unsigned int num_threads = std::min<unsigned int>(hw > 0 ? hw : 4, static_cast<unsigned int>(std::max<size_t>(1, to_parse_indices.size())));

    if (!quiet) {
        std::cout << "    [+] Topilgan manba fayllari: " << paths.size() << " ta"
                  << " (Kesh: " << cache_hits << ", Parallel oqim: " << num_threads << ")\n";
    }

    if (!to_parse_indices.empty()) {
        std::vector<std::thread> workers;
        for (unsigned int t = 0; t < num_threads; ++t) {
            workers.emplace_back([&, t]() {
                for (size_t idx = t; idx < to_parse_indices.size(); idx += num_threads) {
                    size_t pi = to_parse_indices[idx];
                    results[pi] = parse_file_isolated(paths[pi], root_dir);
                }
            });
        }
        for (auto& w : workers) w.join();
    }

    // Update cache and populate Graph
    std::unordered_map<std::string, ParsedFileResult> new_cache;
    std::unordered_map<std::string, std::vector<std::string>> file_imports;

    for (const auto& res : results) {
        if (res.file_node.id.empty()) continue;
        new_cache[res.rel_path] = res;

        graph.add_node(res.file_node);
        std::string mod_id = "module:" + res.module_name;
        if (!graph.has_node(mod_id)) {
            Node mod_node;
            mod_node.id = mod_id;
            mod_node.name = res.module_name;
            mod_node.type = "module";
            mod_node.module = res.module_name;
            graph.add_node(mod_node);
        }
        graph.add_edge(mod_id, res.file_node.id, "contains");

        if (!res.imports.empty()) {
            file_imports[res.file_node.id] = res.imports;
        }
        for (const auto& fn : res.fn_nodes) {
            graph.add_node(fn);
            graph.add_edge(res.file_node.id, fn.id, "contains");
        }
    }

    if (use_cache) {
        save_cache(cache_path, new_cache);
    }

    // Build O(1) indices
    std::unordered_map<std::string, std::vector<std::string>> file_by_name;
    std::unordered_map<std::string, std::vector<std::string>> fn_by_name;
    std::unordered_map<std::string, std::unordered_set<std::string>> imported_files_by_file;

    for (const auto& pair : graph.nodes) {
        if (pair.second.type == "file") {
            file_by_name[pair.second.name].push_back(pair.first);
        } else if (pair.second.type == "function") {
            fn_by_name[pair.second.name].push_back(pair.first);
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
                        imported_files_by_file[src_file_id].insert(target_file_id);
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
        "empty", "end", "find", "count", "substr", "append", "reserve", "resize", "erase",
        "forEach", "map", "filter", "reduce", "join", "slice", "splice", "concat", "includes",
        "some", "every", "keys", "values", "entries", "describe", "it", "test", "h",
        "indexOf", "lastIndexOf", "error", "warn", "log", "info", "debug", "assert",
        "valueOf", "hasOwnProperty", "bind", "call"
    };

    // Scope-aware & Import-aware function call resolution
    for (const auto& pair : graph.nodes) {
        if (pair.second.type != "function") continue;
        const Node& src_fn = pair.second;
        std::string src_file_id = "file:" + src_fn.file_path;
        const auto& imp_set = imported_files_by_file[src_file_id];

        auto emit_call = [&](const std::string& target_id) {
            if (target_id == src_fn.id) return;
            graph.add_edge(src_fn.id, target_id, "calls");
            const std::string& tgt_mod = graph.nodes[target_id].module;
            if (src_fn.module != tgt_mod) {
                graph.add_edge("module:" + src_fn.module, "module:" + tgt_mod, "calls");
            }
        };

        std::unordered_set<std::string> resolved_names;

        // 1. Try qualified calls (e.g., Class::method or obj.method)
        for (const auto& q : src_fn.qualified_calls) {
            if (BOILERPLATE_NAMES.count(q.second)) continue;
            auto it = fn_by_name.find(q.second);
            if (it == fn_by_name.end()) continue;

            std::vector<std::string> scoped_matches;
            for (const std::string& tid : it->second) {
                const Node& tgt = graph.nodes[tid];
                if (!tgt.scope.empty() && tgt.scope == q.first) {
                    scoped_matches.push_back(tid);
                }
            }
            if (!scoped_matches.empty()) {
                for (const std::string& tid : scoped_matches) emit_call(tid);
                resolved_names.insert(q.second);
            }
        }

        // 2. Resolve remaining calls: prefer same-file or imported-file targets, fallback to unique/small global matches
        for (const std::string& called_name : src_fn.called_identifiers) {
            if (BOILERPLATE_NAMES.count(called_name) || resolved_names.count(called_name)) continue;

            auto it = fn_by_name.find(called_name);
            if (it == fn_by_name.end()) continue;

            std::vector<std::string> local_or_imported;
            for (const std::string& tid : it->second) {
                const Node& tgt = graph.nodes[tid];
                std::string tgt_file_id = "file:" + tgt.file_path;
                if (tgt.file_path == src_fn.file_path || imp_set.count(tgt_file_id) ||
                    (!src_fn.scope.empty() && tgt.scope == src_fn.scope)) {
                    local_or_imported.push_back(tid);
                }
            }

            if (!local_or_imported.empty()) {
                for (const std::string& tid : local_or_imported) emit_call(tid);
            } else if (it->second.size() <= 3) {
                for (const std::string& tid : it->second) emit_call(tid);
            }
        }
    }

    graph.rebuild_adjacencies();
}
