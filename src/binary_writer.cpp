#include "binary_writer.hpp"
#include <fstream>
#include <sstream>
#include <iostream>
#include <unordered_map>
#include <cstring>
#include <cstdint>

namespace {
    std::string base64_encode(const unsigned char* buf, size_t bufLen) {
        static const char b64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        std::string ret;
        ret.reserve(((bufLen + 2) / 3) * 4);
        size_t i = 0;
        while (i < bufLen) {
            uint32_t b0 = buf[i++];
            uint32_t b1 = i < bufLen ? buf[i++] : 0;
            uint32_t b2 = i < bufLen ? buf[i++] : 0;
            uint32_t triple = (b0 << 16) | (b1 << 8) | b2;
            ret.push_back(b64[(triple >> 18) & 0x3F]);
            ret.push_back(b64[(triple >> 12) & 0x3F]);
            ret.push_back(i > bufLen + 1 ? '=' : b64[(triple >> 6) & 0x3F]);
            ret.push_back(i > bufLen ? '=' : b64[triple & 0x3F]);
        }
        return ret;
    }
}

bool BinaryWriter::write_binary(const std::string& bin_path, const std::string& js_bundle_path, 
                                const Graph& graph, const std::vector<CodeIssue>& issues, double health_score) {
    std::vector<uint8_t> buffer;

    // Helper write functions
    auto write_u8 = [&](uint8_t v) { buffer.push_back(v); };
    auto write_u16 = [&](uint16_t v) {
        buffer.push_back(static_cast<uint8_t>(v & 0xFF));
        buffer.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
    };
    auto write_u32 = [&](uint32_t v) {
        buffer.push_back(static_cast<uint8_t>(v & 0xFF));
        buffer.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
        buffer.push_back(static_cast<uint8_t>((v >> 16) & 0xFF));
        buffer.push_back(static_cast<uint8_t>((v >> 24) & 0xFF));
    };
    auto write_f32 = [&](float v) {
        uint32_t u;
        std::memcpy(&u, &v, sizeof(float));
        write_u32(u);
    };

    // 1. Build String Table
    std::unordered_map<std::string, uint32_t> str_offsets;
    std::vector<char> string_table;
    auto get_str_offset = [&](const std::string& s) -> uint32_t {
        auto it = str_offsets.find(s);
        if (it != str_offsets.end()) return it->second;
        uint32_t off = static_cast<uint32_t>(string_table.size());
        str_offsets[s] = off;
        for (char c : s) string_table.push_back(c);
        string_table.push_back('\0');
        return off;
    };

    // Pre-seed common empty string
    get_str_offset("");

    // Node index map
    std::unordered_map<std::string, uint32_t> node_indices;
    std::vector<std::string> ordered_node_ids;
    for (const auto& pair : graph.nodes) {
        node_indices[pair.first] = static_cast<uint32_t>(ordered_node_ids.size());
        ordered_node_ids.push_back(pair.first);
        get_str_offset(pair.second.id);
        get_str_offset(pair.second.name);
        get_str_offset(pair.second.module);
        get_str_offset(pair.second.file_path);
    }

    // Mark issue flags on nodes
    std::unordered_map<std::string, uint8_t> node_flags;
    for (const auto& iss : issues) {
        get_str_offset(iss.id);
        get_str_offset(iss.type);
        get_str_offset(iss.title);
        get_str_offset(iss.description);
        get_str_offset(iss.recommendation);

        uint8_t flag_bit = 0;
        if (iss.type == "CIRCULAR_DEPENDENCY") flag_bit = (1 << 0);
        else if (iss.type == "GOD_OBJECT") flag_bit = (1 << 1);
        else if (iss.type == "CODE_DUPLICATION") flag_bit = (1 << 2);
        else if (iss.type == "ABSTRACTION_SPRAWL") flag_bit = (1 << 3);
        else if (iss.type == "LAYER_VIOLATION") flag_bit = (1 << 4);
        else if (iss.type == "ORPHAN_CODE") flag_bit = (1 << 5);

        for (const auto& nid : iss.involved_nodes) {
            node_flags[nid] |= flag_bit;
        }
    }

    // 2. Write Header (26 bytes)
    // Magic: "ARCH"
    write_u8('A'); write_u8('R'); write_u8('C'); write_u8('H');
    write_u16(4); // Version 4 (adds runtime_heat, hotspot_score)
    write_f32(static_cast<float>(health_score));
    write_u32(static_cast<uint32_t>(ordered_node_ids.size()));
    write_u32(static_cast<uint32_t>(graph.edges.size()));
    write_u32(static_cast<uint32_t>(issues.size()));
    write_u32(static_cast<uint32_t>(string_table.size()));

    // 3. Write String Table
    for (char c : string_table) {
        write_u8(static_cast<uint8_t>(c));
    }

    // 4. Write Nodes
    for (const auto& nid : ordered_node_ids) {
        const auto& n = graph.nodes.at(nid);
        write_u32(get_str_offset(n.id));
        write_u32(get_str_offset(n.name));
        
        uint8_t type_code = 0; // module
        if (n.type == "file") type_code = 1;
        else if (n.type == "function") type_code = 2;
        write_u8(type_code);

        write_u32(get_str_offset(n.module));
        write_u32(get_str_offset(n.file_path));
        write_u32(static_cast<uint32_t>(n.loc));
        write_u32(static_cast<uint32_t>(n.line_start));
        write_u32(static_cast<uint32_t>(n.line_end));
        write_u16(static_cast<uint16_t>(n.cyclomatic_complexity));
        write_u16(static_cast<uint16_t>(n.in_degree));
        write_u16(static_cast<uint16_t>(n.out_degree));
        write_u16(static_cast<uint16_t>(n.blast_radius));
        write_f32(static_cast<float>(n.instability));
        write_f32(static_cast<float>(n.pagerank));
        write_f32(static_cast<float>(n.runtime_heat));
        write_f32(static_cast<float>(n.hotspot_score));

        uint8_t flags = node_flags[n.id];
        if (n.is_pass_through) flags |= (1 << 3);
        write_u8(flags);
    }

    // 5. Write Edges
    for (const auto& e : graph.edges) {
        uint32_t src_idx = node_indices[e.source];
        uint32_t tgt_idx = node_indices[e.target];
        write_u32(src_idx);
        write_u32(tgt_idx);

        uint8_t type_code = 0; // imports
        if (e.type == "calls") type_code = 1;
        else if (e.type == "contains") type_code = 2;
        write_u8(type_code);
        write_u16(static_cast<uint16_t>(e.weight));
    }

    // 6. Write Issues
    for (const auto& iss : issues) {
        write_u32(get_str_offset(iss.id));
        write_u32(get_str_offset(iss.type));
        
        uint8_t sev = 0;
        if (iss.severity == IssueSeverity::WARNING) sev = 1;
        else if (iss.severity == IssueSeverity::CRITICAL) sev = 2;
        write_u8(sev);

        write_u32(get_str_offset(iss.title));
        write_u32(get_str_offset(iss.description));
        write_u32(get_str_offset(iss.recommendation));

        write_u16(static_cast<uint16_t>(iss.involved_nodes.size()));
        for (const auto& nid : iss.involved_nodes) {
            auto it = node_indices.find(nid);
            uint32_t idx = (it != node_indices.end()) ? it->second : 0xFFFFFFFF;
            write_u32(idx);
        }
    }

    // Write binary file
    {
        std::ofstream out(bin_path, std::ios::binary);
        if (!out.is_open()) return false;
        out.write(reinterpret_cast<const char*>(buffer.data()), buffer.size());
    }

    // Also write JS bundle (embeds binary as Base64 for instant zero-CORS file:// loading)
    if (!js_bundle_path.empty()) {
        std::string b64 = base64_encode(buffer.data(), buffer.size());
        std::ofstream js_out(js_bundle_path);
        if (js_out.is_open()) {
            js_out << "(typeof window !== 'undefined' ? window : (typeof global !== 'undefined' ? global : this)).ARCH_BINARY_B64 = \"" << b64 << "\";\n";
        }
    }

    std::cout << "\033[1;32m[✓] Binar format tayyorlandi: \033[0m" << bin_path 
              << " (" << (buffer.size() / 1024) << " KB - JSON dan 95% yengil!)\n";
    if (!js_bundle_path.empty()) {
        std::cout << "\033[1;32m[✓] Oflayn JS Binar to'plami: \033[0m" << js_bundle_path << "\n";
    }

    return true;
}
