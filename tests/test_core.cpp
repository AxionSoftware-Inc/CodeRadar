#include "graph.hpp"
#include "parser.hpp"
#include "analyzer.hpp"
#include "binary_writer.hpp"
#include "json_writer.hpp"
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>

namespace fs = std::filesystem;

namespace {

void write_text_file(const fs::path& p, const std::string& content) {
    fs::create_directories(p.parent_path());
    std::ofstream out(p);
    assert(out.is_open());
    out << content;
}

uint32_t read_u32_le(const std::vector<uint8_t>& buf, size_t off) {
    return static_cast<uint32_t>(buf[off]) |
           (static_cast<uint32_t>(buf[off + 1]) << 8) |
           (static_cast<uint32_t>(buf[off + 2]) << 16) |
           (static_cast<uint32_t>(buf[off + 3]) << 24);
}

uint16_t read_u16_le(const std::vector<uint8_t>& buf, size_t off) {
    return static_cast<uint16_t>(buf[off]) | (static_cast<uint16_t>(buf[off + 1]) << 8);
}

bool has_issue_type(const std::vector<CodeIssue>& issues, const std::string& type) {
    for (const auto& iss : issues) {
        if (iss.type == type) return true;
    }
    return false;
}

Node make_node(const std::string& id, const std::string& name, const std::string& type,
               const std::string& file_path, const std::string& module) {
    Node n;
    n.id = id;
    n.name = name;
    n.type = type;
    n.file_path = file_path;
    n.module = module;
    return n;
}

void test_pagerank_and_tarjan_consistency() {
    Graph g;
    Node m1 = make_node("module:core", "core", "module", "", "core");
    Node m2 = make_node("module:api", "api", "module", "", "api");
    Node f1 = make_node("file:core/a.cpp", "a.cpp", "file", "core/a.cpp", "core");
    Node f2 = make_node("file:core/b.cpp", "b.cpp", "file", "core/b.cpp", "core");
    Node f3 = make_node("file:api/c.cpp", "c.cpp", "file", "api/c.cpp", "api");
    g.add_node(m1); g.add_node(m2);
    g.add_node(f1); g.add_node(f2); g.add_node(f3);

    // Containment edges must NOT create cycles or distort out_degree/PageRank
    g.add_edge("module:core", "file:core/a.cpp", "contains");
    g.add_edge("module:core", "file:core/b.cpp", "contains");
    g.add_edge("module:api", "file:api/c.cpp", "contains");

    // True dependency cycle: a -> b -> a, plus layer violation: core/a -> api/c
    g.add_edge("file:core/a.cpp", "file:core/b.cpp", "imports");
    g.add_edge("file:core/b.cpp", "file:core/a.cpp", "imports");
    g.add_edge("file:core/a.cpp", "file:api/c.cpp", "imports");
    g.rebuild_adjacencies();

    ArchitectureAnalyzer analyzer;
    std::vector<CodeIssue> issues;
    double score = 100.0;
    analyzer.analyze(g, issues, score);

    double pr_sum = 0.0;
    for (const auto& kv : g.nodes) pr_sum += kv.second.pagerank;
    assert(std::abs(pr_sum - 1.0) < 1e-3);
    assert(g.nodes["module:core"].out_degree == 0); // contains excluded
    assert(has_issue_type(issues, "CIRCULAR_DEPENDENCY"));
    assert(has_issue_type(issues, "LAYER_VIOLATION"));
    std::cout << "[PASS] test_pagerank_and_tarjan_consistency\n";
}

void test_wl_order_invariance_and_subblock_and_mdl() {
    fs::path tmp = "/tmp/arch_radar_golden_test";
    fs::remove_all(tmp);

    // 1. Variable-declaration-order swapped clones (WL should keep high similarity)
    write_text_file(tmp / "billing" / "calc_a.cpp", R"(
#include "calc_a.hpp"
double compute_invoice_a(double base_price, double tax_rate, double discount) {
    double net_amount = base_price - discount;
    if (net_amount < 0.0) {
        net_amount = 0.0;
    }
    double tax_val = net_amount * tax_rate;
    double fee = validate_fee(net_amount);
    double total = net_amount + tax_val + fee;
    log_audit(total);
    return total;
}
)");

    write_text_file(tmp / "orders" / "calc_b.cpp", R"(
#include "calc_b.hpp"
double compute_invoice_b(double discount, double tax_rate, double base_price) {
    double fee = 0.0;
    double tax_val = 0.0;
    double net_amount = base_price - discount;
    if (net_amount < 0.0) {
        net_amount = 0.0;
    }
    tax_val = net_amount * tax_rate;
    fee = validate_fee(net_amount);
    double total = net_amount + tax_val + fee;
    log_audit(total);
    return total;
}
)");

    // 2. Scope-aware call resolution test
    write_text_file(tmp / "services" / "runners.cpp", R"(
class AlphaService {
public:
    void execute_task(int x) {
        int y = x + 1;
        log_alpha(y);
    }
};
class BetaService {
public:
    void execute_task(int x) {
        int z = x * 2;
        log_beta(z);
    }
};
void dispatch_alpha() {
    AlphaService svc;
    svc.execute_task(42);
}
)");

    Graph g;
    CodeParser parser;
    parser.parse_directory(tmp.string(), g, false, true);

    // Verify qualified call resolved to AlphaService::execute_task, not BetaService::execute_task
    bool found_alpha_edge = false, found_beta_edge = false;
    for (const auto& e : g.edges) {
        if (e.type == "calls" && e.source.find("dispatch_alpha") != std::string::npos) {
            if (g.nodes[e.target].scope == "AlphaService") found_alpha_edge = true;
            if (g.nodes[e.target].scope == "BetaService") found_beta_edge = true;
        }
    }
    assert(found_alpha_edge && !found_beta_edge);

    ArchitectureAnalyzer analyzer;
    std::vector<CodeIssue> issues;
    double score = 100.0;
    analyzer.analyze(g, issues, score);

    assert(has_issue_type(issues, "CODE_DUPLICATION"));
    fs::remove_all(tmp);
    std::cout << "[PASS] test_wl_order_invariance_and_scope_resolution\n";
}

void test_binary_v3_roundtrip_large_lines() {
    Graph g;
    Node n;
    n.id = "fn:big.cpp:huge_fn";
    n.name = "huge_fn";
    n.type = "function";
    n.file_path = "big.cpp";
    n.module = "core";
    n.line_start = 70000; // Exceeds uint16_t (65535)
    n.line_end = 70120;
    n.loc = 121;
    n.blast_radius = 4;
    g.add_node(n);
    g.rebuild_adjacencies();

    std::vector<CodeIssue> issues;
    std::string bin_path = "/tmp/arch_test_v3.bin";
    assert(BinaryWriter::write_binary(bin_path, "", g, issues, 92.5));

    std::ifstream in(bin_path, std::ios::binary);
    std::vector<uint8_t> buf((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    assert(buf.size() >= 26);
    assert(buf[0] == 'A' && buf[1] == 'R' && buf[2] == 'C' && buf[3] == 'H');
    assert(read_u16_le(buf, 4) == 4); // Version 4
    uint32_t str_size = read_u32_le(buf, 22);
    size_t node_off = 26 + str_size;
    // Skip id(4), name(4), type(1), mod(4), file(4), loc(4) -> +21 bytes
    uint32_t ls = read_u32_le(buf, node_off + 21);
    uint32_t le = read_u32_le(buf, node_off + 25);
    assert(ls == 70000);
    assert(le == 70120);

    fs::remove(bin_path);
    std::cout << "[PASS] test_binary_v3_roundtrip_large_lines\n";
}

void test_scale_stress_1500_functions() {
    Graph g;
    for (int i = 0; i < 1500; ++i) {
        Node fn;
        fn.id = "fn:mod" + std::to_string(i % 20) + "/f.cpp:fn_" + std::to_string(i);
        fn.name = "fn_" + std::to_string(i);
        fn.type = "function";
        fn.file_path = "mod" + std::to_string(i % 20) + "/f.cpp";
        fn.module = "mod" + std::to_string(i % 20);
        fn.loc = 15 + (i % 20);
        fn.cyclomatic_complexity = 3 + (i % 5);
        fn.norm_token_vector = {{"if", 2}, {"return", 1}, {"$CALL", 3}, {"$V0", 4}, {"=", 3}};
        fn.cfg_sequence = {"IF", "CALL", "ASSIGN", "CALL", "RET"};
        fn.called_identifiers = {"helper_" + std::to_string(i % 30)};
        fn.wl_vector = {{100ULL + (i % 25), 2}, {200ULL + (i % 10), 1}};
        fn.shingle_hashes = {1000ULL + (i % 50), 2000ULL + (i % 50), 3000ULL + (i % 50)};
        g.add_node(fn);
    }
    g.rebuild_adjacencies();

    auto t0 = std::chrono::high_resolution_clock::now();
    ArchitectureAnalyzer analyzer;
    std::vector<CodeIssue> issues;
    double score = 100.0;
    analyzer.analyze(g, issues, score);
    auto t1 = std::chrono::high_resolution_clock::now();
    double secs = std::chrono::duration<double>(t1 - t0).count();

    assert(secs < 2.0);
    std::cout << "[PASS] test_scale_stress_1500_functions (" << secs << "s)\n";
}

} // namespace

int main() {
    test_pagerank_and_tarjan_consistency();
    test_wl_order_invariance_and_subblock_and_mdl();
    test_binary_v3_roundtrip_large_lines();
    test_scale_stress_1500_functions();
    std::cout << "ALL GOLDEN & STRESS TESTS PASSED!\n";
    return 0;
}
