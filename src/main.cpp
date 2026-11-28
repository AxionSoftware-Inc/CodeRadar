#include "graph.hpp"
#include "parser.hpp"
#include "analyzer.hpp"
#include "json_writer.hpp"
#include "binary_writer.hpp"
#include "query.hpp"
#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include <chrono>
#include <thread>
#include <cstdlib>
#include <filesystem>

namespace fs = std::filesystem;

void print_banner() {
    std::cout << "\033[1;36m";
    std::cout << "=================================================================\n";
    std::cout << "      ARCH-RADAR: 3D Spatial & Architectural Linter (v2.0)       \n";
    std::cout << "    Macro-to-Micro Code Topology & Invisible Smells Detection    \n";
    std::cout << "=================================================================\n";
    std::cout << "\033[0m";
}

fs::file_time_type get_latest_write_time(const std::string& dir) {
    fs::file_time_type latest = fs::file_time_type::min();
    try {
        for (const auto& entry : fs::recursive_directory_iterator(dir)) {
            if (entry.is_regular_file()) {
                std::string ext = entry.path().extension().string();
                if (ext == ".cpp" || ext == ".hpp" || ext == ".h" || ext == ".cc" ||
                    ext == ".dart" || ext == ".cs" || ext == ".js" || ext == ".ts" || ext == ".py") {
                    auto t = fs::last_write_time(entry);
                    if (t > latest) latest = t;
                }
            }
        }
    } catch (...) {}
    return latest;
}

bool run_single_pass(const std::string& target_dir,
                     const std::string& output_json,
                     bool ci_mode,
                     double min_score,
                     int max_critical,
                     bool use_cache,
                     const std::string& profile_path,
                     const std::string& query_cmd,
                     const std::string& query_arg) {
    auto start_time = std::chrono::high_resolution_clock::now();
    bool quiet = !query_cmd.empty();

    if (!quiet) {
        std::cout << "\n\033[1;34m[*] Skanerlanmoqda: \033[0m" << target_dir << "\n";
    }

    Graph graph;
    CodeParser parser;
    parser.parse_directory(target_dir, graph, use_cache, quiet);

    if (graph.nodes.empty()) {
        if (quiet) {
            std::cout << "{\"error\": \"no source files found\"}\n";
        } else {
            std::cerr << "\033[1;31m[-] Manba fayllari topilmadi.\033[0m\n";
        }
        return false;
    }

    if (!quiet) {
        std::cout << "\033[1;32m[+] Topilgan Tugunlar: \033[0m" << graph.nodes.size()
                  << " \033[1;32m| Bog'liqlik va Chaqiruvlar: \033[0m" << graph.edges.size() << "\n";
        std::cout << "[*] Topologik va Semantik Tahlil yuritilmoqda...\n";
    }

    ArchitectureAnalyzer analyzer;
    std::vector<CodeIssue> issues;
    double health_score = 100.0;
    analyzer.analyze(graph, issues, health_score, profile_path);

    if (quiet) {
        return QueryEngine::execute(query_cmd, query_arg, graph, issues, health_score) == 0;
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    double duration = std::chrono::duration<double>(end_time - start_time).count();

    // Group issues by severity
    int critical_count = 0, warning_count = 0, info_count = 0;
    for (const auto& iss : issues) {
        if (iss.severity == IssueSeverity::CRITICAL) critical_count++;
        else if (iss.severity == IssueSeverity::WARNING) warning_count++;
        else info_count++;
    }

    std::string grade = "A";
    std::string color = "\033[1;32m";
    if (health_score < 40) { grade = "F (Xavfli)"; color = "\033[1;31m"; }
    else if (health_score < 60) { grade = "D (Zaif)"; color = "\033[1;31m"; }
    else if (health_score < 75) { grade = "C (O'rtacha)"; color = "\033[1;33m"; }
    else if (health_score < 90) { grade = "B (Yaxshi)"; color = "\033[1;36m"; }
    else { grade = "A (A'lo)"; color = "\033[1;32m"; }

    std::cout << "\n\033[1;37m┌────────────────── ARXITEKTURA NATIJALARI (LINT) ─────────────────┐\033[0m\n";
    std::cout << "│  Sog'lomlik Indeksi : " << color << std::fixed << std::setprecision(1) << std::setw(5) << health_score << " / 100 (Baho: " << grade << ")\033[0m\n";
    std::cout << "│  Jami Muammolar     : " << issues.size() << " ta (Xavfli: \033[1;31m" << critical_count 
              << "\033[0m, Ogohlantirish: \033[1;33m" << warning_count 
              << "\033[0m, Ma'lumot: \033[1;35m" << info_count << "\033[0m)\n";
    std::cout << "│  Tahlil Vaqti       : " << std::fixed << std::setprecision(2) << duration << " soniya\n";
    std::cout << "\033[1;37m└───────────────────────────────────────────────────────────────────┘\033[0m\n\n";

    // Print top critical issues
    int shown = 0;
    for (const auto& issue : issues) {
        if (shown >= 8) {
            std::cout << "\033[1;30m... va yana " << (issues.size() - shown) << " ta boshqa muammolar hisobotda mavjud.\033[0m\n";
            break;
        }
        std::string tag = (issue.severity == IssueSeverity::CRITICAL ? "\033[1;31m[CRITICAL]\033[0m" : 
                          (issue.severity == IssueSeverity::WARNING  ? "\033[1;33m[WARNING] \033[0m"  : "\033[1;35m[INFO]    \033[0m"));
        std::cout << tag << " \033[1m" << issue.title << "\033[0m\n";
        std::cout << "         " << issue.description << "\n";
        shown++;
    }

    // Write high-performance binary format & JS bundle for both legacy and Next.js public directory
    BinaryWriter::write_binary("web/arch_data.bin", "web/data.js", graph, issues, health_score);
    BinaryWriter::write_binary("web/public/arch_data.bin", "web/public/data.js", graph, issues, health_score);

    if (JsonWriter::write_report(output_json, graph, issues, health_score)) {
        std::cout << "\033[1;32m[✓] JSON hisobot saqlandi: \033[0m" << output_json << "\n";
    }

    // CI/CD check
    if (ci_mode) {
        if (health_score < min_score || critical_count > max_critical) {
            std::cerr << "\n\033[1;31m[LINT FAILED] Kod arxitekturasi belgilangan standartlarga mos kelmadi!\033[0m\n";
            std::cerr << "Talab: Min Score >= " << min_score << " (Hozir: " << health_score << "), Max Critical <= " 
                      << max_critical << " (Hozir: " << critical_count << ")\n";
            return false;
        } else {
            std::cout << "\n\033[1;32m[LINT PASSED] Arxitektura testlaridan muvaffaqiyatli o'tdi!\033[0m\n";
        }
    }
    return true;
}

int main(int argc, char* argv[]) {
    std::string target_dir = ".";
    std::string output_json = "analysis_result.json";
    bool ci_mode = false;
    bool watch_mode = false;
    bool use_cache = true;
    double min_score = 70.0;
    int max_critical = 0;

    bool serve_mode = false;
    int port = 8080;
    std::string profile_path;
    std::string query_cmd;
    std::string query_arg;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--ci") {
            ci_mode = true;
        } else if (arg == "--watch" || arg == "-w") {
            watch_mode = true;
        } else if (arg == "--no-cache") {
            use_cache = false;
        } else if (arg == "--profile" && i + 1 < argc) {
            profile_path = argv[++i];
        } else if (arg == "--query" && i + 1 < argc) {
            query_cmd = argv[++i];
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                query_arg = argv[++i];
            }
        } else if (arg == "--serve" || arg == "-s") {
            serve_mode = true;
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                port = std::stoi(argv[++i]);
            }
        } else if (arg == "--threshold" && i + 1 < argc) {
            min_score = std::stod(argv[++i]);
        } else if (arg == "--max-critical" && i + 1 < argc) {
            max_critical = std::stoi(argv[++i]);
        } else if (arg[0] != '-') {
            if (target_dir == ".") {
                target_dir = arg;
            } else if (output_json == "analysis_result.json") {
                output_json = arg;
            }
        }
    }

    if (query_cmd.empty()) {
        print_banner();
    }

    if (!watch_mode) {
        bool ok = run_single_pass(target_dir, output_json, ci_mode, min_score, max_critical, use_cache, profile_path, query_cmd, query_arg);
        if (serve_mode) {
            std::cout << "\n\033[1;36m[*] Mahalliy Web Server ishga tushirilmoqda: http://localhost:" << port << "\033[0m\n";
            std::string cmd = "python3 -m http.server " + std::to_string(port) + " -d web > /dev/null 2>&1 &";
            int rc1 = std::system(cmd.c_str()); (void)rc1;
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            std::string open_cmd = "xdg-open http://localhost:" + std::to_string(port) + " > /dev/null 2>&1 &";
            int rc2 = std::system(open_cmd.c_str()); (void)rc2;
            std::cout << "\033[1;32m[✓] 3D Arxitektura brauzerda ochildi! To'xtatish uchun: Ctrl+C\033[0m\n";
            while (true) std::this_thread::sleep_for(std::chrono::seconds(10));
        }
        return ok ? 0 : 1;
    }

    // Watch mode: continually monitors directory
    std::cout << "\033[1;33m[*] WATCH MODE FAOL: Kod bazasidagi o'zgarishlar kuzatilmoqda... (Chiqish uchun Ctrl+C)\033[0m\n";
    run_single_pass(target_dir, output_json, ci_mode, min_score, max_critical, use_cache, profile_path, "", "");

    auto last_time = get_latest_write_time(target_dir);
    while (true) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        auto current_time = get_latest_write_time(target_dir);
        if (current_time > last_time) {
            std::cout << "\n\033[1;33m[!] O'zgarish sezildi! Qayta tahlil qilinmoqda...\033[0m\n";
            last_time = current_time;
            run_single_pass(target_dir, output_json, ci_mode, min_score, max_critical, use_cache, profile_path, "", "");
        }
    }

    return 0;
}
