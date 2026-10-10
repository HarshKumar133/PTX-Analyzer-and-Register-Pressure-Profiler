#include "ptxscope/report.hpp"

#include <sstream>

namespace ptxscope {
namespace {

std::string json_escape(const std::string &input) {
    std::string output;
    output.reserve(input.size() + 2);
    for (const char raw : input) {
        const auto c = static_cast<unsigned char>(raw);
        switch (c) {
        case '"':
            output += "\\\"";
            break;
        case '\\':
            output += "\\\\";
            break;
        case '\b':
            output += "\\b";
            break;
        case '\f':
            output += "\\f";
            break;
        case '\n':
            output += "\\n";
            break;
        case '\r':
            output += "\\r";
            break;
        case '\t':
            output += "\\t";
            break;
        default:
            if (c < 0x20) {
                constexpr char hex[] = "0123456789abcdef";
                output += "\\u00";
                output.push_back(hex[(c >> 4U) & 0x0fU]);
                output.push_back(hex[c & 0x0fU]);
            } else
                output.push_back(static_cast<char>(c));
        }
    }
    return output;
}

void write_set(std::ostringstream &out, const std::set<std::string> &values) {
    out << '[';
    bool first = true;
    for (const auto &value : values) {
        if (!first)
            out << ',';
        first = false;
        out << '"' << json_escape(value) << '"';
    }
    out << ']';
}

} // namespace

std::string severity_name(Severity severity) {
    switch (severity) {
    case Severity::info:
        return "info";
    case Severity::warning:
        return "warning";
    case Severity::error:
        return "error";
    }
    return "warning";
}

std::string render_text(const AnalysisReport &report, bool verbose) {
    std::ostringstream out;
    out << "PTXScope Analysis Report\n"
        << "Input: " << report.program.input_name << '\n'
        << "PTX metadata: version="
        << (report.program.ptx_version.empty() ? "(undeclared)" : report.program.ptx_version)
        << ", target=" << (report.program.target.empty() ? "(undeclared)" : report.program.target)
        << '\n'
        << "Parsing: "
        << (report.program.parse_valid ? "valid for the documented subset" : "errors found")
        << '\n';
    for (std::size_t f = 0; f < report.program.functions.size(); ++f) {
        const auto &function = report.program.functions[f];
        const auto &analysis = report.functions[f];
        out << "\nFunction " << function.name << (function.entry ? " [entry]" : "") << '\n'
            << "  Instructions: " << function.instructions.size() << '\n'
            << "  Basic blocks: " << analysis.blocks.size() << ", edges: " << analysis.edge_count
            << ", unreachable: " << analysis.unreachable_blocks << '\n'
            << "  Declared virtual registers: " << analysis.declared_registers.size() << '\n'
            << "  Liveness: " << (analysis.liveness_converged ? "converged" : "not converged")
            << " in " << analysis.iterations << " iterations; effects "
            << (analysis.effects_complete ? "modeled for this subset" : "incomplete") << '\n'
            << "  Peak simultaneously live estimate at instruction " << analysis.peak_instruction
            << ": GPR-like=" << analysis.peak_gpr << ", predicate=" << analysis.peak_predicate
            << ", other=" << analysis.peak_other << '\n';
        if (verbose) {
            for (const auto &block : analysis.blocks) {
                out << "  block " << block.id << " (" << block.instruction_indices.size()
                    << " instructions)" << (block.reachable ? " reachable" : " unreachable")
                    << " preds=";
                for (const auto pred : block.predecessors)
                    out << pred << ' ';
                out << "succs=";
                for (const auto succ : block.successors)
                    out << succ << ' ';
                if (!block.labels.empty()) {
                    out << "labels=";
                    for (const auto &label : block.labels)
                        out << label << ' ';
                }
                out << "pressure(GPR=" << block.peak_gpr << ",pred=" << block.peak_predicate
                    << ",other=" << block.peak_other << ")@i" << block.peak_instruction << ' ';
                out << '\n';
                const auto &live = analysis.block_liveness[block.id];
                out << "    USE=";
                write_set(out, live.use);
                out << " DEF=";
                write_set(out, live.def);
                out << " IN=";
                write_set(out, live.live_in);
                out << " OUT=";
                write_set(out, live.live_out);
                out << '\n';
            }
            for (std::size_t i = 0; i < function.instructions.size(); ++i) {
                out << "    i" << i << " @" << function.instructions[i].location.line << ':'
                    << function.instructions[i].location.column << " "
                    << function.instructions[i].raw << " IN=";
                write_set(out, analysis.instruction_liveness[i].live_in);
                out << " OUT=";
                write_set(out, analysis.instruction_liveness[i].live_out);
                out << '\n';
            }
        }
    }
    out << "\nDiagnostics: " << report.diagnostics.size() << '\n';
    for (const auto &diagnostic : report.diagnostics) {
        out << diagnostic.location.file << ':' << diagnostic.location.line << ':'
            << diagnostic.location.column << ": " << severity_name(diagnostic.severity) << '['
            << diagnostic.code << "]: " << diagnostic.message;
        if (verbose && !diagnostic.note.empty())
            out << " (" << diagnostic.note << ')';
        out << '\n';
    }
    out << "\nLimitations: register-pressure values estimate live virtual PTX registers; they do "
           "not predict "
           "PTXAS allocation, occupancy, spilling, or runtime. Unknown effects make liveness "
           "incomplete.\n";
    return out.str();
}

std::string render_json(const AnalysisReport &report, bool include_timings) {
    std::ostringstream out;
    out << "{\"schema_version\":1,\"input\":\"" << json_escape(report.program.input_name)
        << "\",\"ptx_version\":\"" << json_escape(report.program.ptx_version) << "\",\"target\":\""
        << json_escape(report.program.target)
        << "\",\"parse_valid\":" << (report.program.parse_valid ? "true" : "false")
        << ",\"functions\":[";
    for (std::size_t f = 0; f < report.program.functions.size(); ++f) {
        if (f)
            out << ',';
        const auto &function = report.program.functions[f];
        const auto &analysis = report.functions[f];
        out << "{\"name\":\"" << json_escape(function.name)
            << "\",\"entry\":" << (function.entry ? "true" : "false")
            << ",\"instruction_count\":" << function.instructions.size()
            << ",\"basic_block_count\":" << analysis.blocks.size()
            << ",\"edge_count\":" << analysis.edge_count
            << ",\"unreachable_block_count\":" << analysis.unreachable_blocks
            << ",\"declared_register_count\":" << analysis.declared_registers.size()
            << ",\"liveness_iterations\":" << analysis.iterations
            << ",\"liveness_converged\":" << (analysis.liveness_converged ? "true" : "false")
            << ",\"effects_complete\":" << (analysis.effects_complete ? "true" : "false")
            << ",\"peak_pressure\":{\"instruction\":" << analysis.peak_instruction
            << ",\"gpr_like\":" << analysis.peak_gpr << ",\"predicate\":" << analysis.peak_predicate
            << ",\"other\":" << analysis.peak_other << '}';
        if (include_timings) {
            out << ",\"timings_ms\":{\"cfg\":" << analysis.cfg_time_ms
                << ",\"liveness\":" << analysis.liveness_time_ms << '}';
        }
        out << ",\"blocks\":[";
        for (std::size_t b = 0; b < analysis.blocks.size(); ++b) {
            if (b)
                out << ',';
            const auto &block = analysis.blocks[b];
            const auto &live = analysis.block_liveness[b];
            out << "{\"id\":" << block.id
                << ",\"reachable\":" << (block.reachable ? "true" : "false")
                << ",\"instructions\":[";
            for (std::size_t i = 0; i < block.instruction_indices.size(); ++i) {
                if (i)
                    out << ',';
                out << block.instruction_indices[i];
            }
            out << "],\"predecessors\":[";
            for (std::size_t i = 0; i < block.predecessors.size(); ++i) {
                if (i)
                    out << ',';
                out << block.predecessors[i];
            }
            out << "],\"successors\":[";
            for (std::size_t i = 0; i < block.successors.size(); ++i) {
                if (i)
                    out << ',';
                out << block.successors[i];
            }
            out << "],\"peak_pressure\":{\"instruction\":" << block.peak_instruction
                << ",\"gpr_like\":" << block.peak_gpr << ",\"predicate\":" << block.peak_predicate
                << ",\"other\":" << block.peak_other << "},\"use\":";
            write_set(out, live.use);
            out << ",\"def\":";
            write_set(out, live.def);
            out << ",\"live_in\":";
            write_set(out, live.live_in);
            out << ",\"live_out\":";
            write_set(out, live.live_out);
            out << '}';
        }
        out << "],\"instruction_liveness\":[";
        for (std::size_t i = 0; i < analysis.instruction_liveness.size(); ++i) {
            if (i)
                out << ',';
            out << "{\"instruction\":" << i << ",\"live_in\":";
            write_set(out, analysis.instruction_liveness[i].live_in);
            out << ",\"live_out\":";
            write_set(out, analysis.instruction_liveness[i].live_out);
            out << '}';
        }
        out << "]}";
    }
    out << ']';
    if (include_timings) {
        out << ",\"timings_ms\":{\"parse\":" << report.parse_time_ms
            << ",\"analysis\":" << report.analysis_time_ms << '}';
    }
    out << ",\"diagnostics\":[";
    for (std::size_t i = 0; i < report.diagnostics.size(); ++i) {
        if (i)
            out << ',';
        const auto &d = report.diagnostics[i];
        out << "{\"code\":\"" << json_escape(d.code) << "\",\"severity\":\""
            << severity_name(d.severity) << "\",\"message\":\"" << json_escape(d.message)
            << "\",\"file\":\"" << json_escape(d.location.file) << "\",\"line\":" << d.location.line
            << ",\"column\":" << d.location.column << ",\"note\":\"" << json_escape(d.note)
            << "\"}";
    }
    out << "]}";
    return out.str();
}

} // namespace ptxscope
