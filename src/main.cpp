#include "ptxscope/analysis.hpp"
#include "ptxscope/lexer.hpp"
#include "ptxscope/parser.hpp"
#include "ptxscope/report.hpp"

#include <chrono>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

namespace {

constexpr int usage_error = 2;
constexpr int input_error = 3;
constexpr int analysis_error = 4;
constexpr int output_error = 5;

void usage(std::ostream &out) {
    out << "PTXScope " PTXSCOPE_VERSION "\n"
        << "Usage:\n"
        << "  ptxscope analyze <input.ptx> [--format text|json] [--output <path>]\n"
        << "                     [--function <name>] [--verbose] [--diagnostics-only] [--strict] "
           "[--timings]\n"
        << "  ptxscope help\n"
        << "  ptxscope version\n";
}

bool has_errors(const ptxscope::AnalysisReport &report) {
    for (const auto &diagnostic : report.diagnostics)
        if (diagnostic.severity == ptxscope::Severity::error)
            return true;
    return false;
}

} // namespace

int main(int argc, char **argv) {
    if (argc == 1 || std::string(argv[1]) == "help" || std::string(argv[1]) == "--help" ||
        std::string(argv[1]) == "-h") {
        usage(std::cout);
        return 0;
    }
    if (std::string(argv[1]) == "version" || std::string(argv[1]) == "--version") {
        std::cout << "PTXScope " PTXSCOPE_VERSION << '\n';
        return 0;
    }
    if (std::string(argv[1]) != "analyze" || argc < 3) {
        usage(std::cerr);
        return usage_error;
    }

    const std::string input_path = argv[2];
    std::string format = "text";
    std::string output_path;
    std::string selected_function;
    bool verbose = false;
    bool diagnostics_only = false;
    bool strict = false;
    bool timings = false;
    for (int i = 3; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--format" && i + 1 < argc)
            format = argv[++i];
        else if (arg == "--output" && i + 1 < argc)
            output_path = argv[++i];
        else if (arg == "--function" && i + 1 < argc)
            selected_function = argv[++i];
        else if (arg == "--verbose")
            verbose = true;
        else if (arg == "--diagnostics-only")
            diagnostics_only = true;
        else if (arg == "--strict")
            strict = true;
        else if (arg == "--timings")
            timings = true;
        else if (arg == "--cfg" || arg == "--liveness" ||
                 arg == "--pressure") { /* all are reported */
        } else {
            std::cerr << "ptxscope: invalid or incomplete option '" << arg << "'\n";
            usage(std::cerr);
            return usage_error;
        }
    }
    if (format != "text" && format != "json") {
        std::cerr << "ptxscope: --format must be 'text' or 'json'\n";
        return usage_error;
    }

    std::ifstream input(input_path, std::ios::binary);
    if (!input) {
        std::cerr << "ptxscope: cannot open input file '" << input_path << "'\n";
        return input_error;
    }
    const std::string source{std::istreambuf_iterator<char>(input),
                             std::istreambuf_iterator<char>()};
    if (input.bad()) {
        std::cerr << "ptxscope: failed while reading input file '" << input_path << "'\n";
        return input_error;
    }

    const auto total_start = std::chrono::steady_clock::now();
    auto lexed = ptxscope::lex(source, input_path);
    auto program = ptxscope::parse(lexed, input_path);
    const auto parse_end = std::chrono::steady_clock::now();
    if (!selected_function.empty()) {
        bool found = false;
        for (auto it = program.functions.begin(); it != program.functions.end();) {
            if (it->name == selected_function)
                found = true;
            if (it->name != selected_function)
                it = program.functions.erase(it);
            else
                ++it;
        }
        if (!found) {
            std::cerr << "ptxscope: function '" << selected_function << "' was not found\n";
            return input_error;
        }
    }
    const auto analysis_start = std::chrono::steady_clock::now();
    auto report = ptxscope::analyze(std::move(program));
    const auto analysis_end = std::chrono::steady_clock::now();
    report.parse_time_ms =
        std::chrono::duration<double, std::milli>(parse_end - total_start).count();
    report.analysis_time_ms =
        std::chrono::duration<double, std::milli>(analysis_end - analysis_start).count();
    std::string rendered;
    if (format == "json") {
        rendered = ptxscope::render_json(report, timings);
    } else if (diagnostics_only) {
        for (const auto &d : report.diagnostics) {
            rendered += d.location.file + ":" + std::to_string(d.location.line) + ":" +
                        std::to_string(d.location.column) + ": " +
                        ptxscope::severity_name(d.severity) + "[" + d.code + "]: " + d.message +
                        "\n";
        }
        if (rendered.empty())
            rendered = "No diagnostics.\n";
    } else {
        rendered = ptxscope::render_text(report, verbose);
    }
    if (output_path.empty())
        std::cout << rendered;
    else {
        std::ofstream output(output_path, std::ios::binary | std::ios::trunc);
        if (!output) {
            std::cerr << "ptxscope: cannot write output file '" << output_path << "'\n";
            return output_error;
        }
        output << rendered;
        if (!output) {
            std::cerr << "ptxscope: failed while writing output file '" << output_path << "'\n";
            return output_error;
        }
    }
    if (!report.program.parse_valid)
        return input_error;
    if (has_errors(report))
        return analysis_error;
    if (strict) {
        for (const auto &diagnostic : report.diagnostics)
            if (diagnostic.severity == ptxscope::Severity::warning)
                return analysis_error;
        for (const auto &function : report.functions)
            if (!function.effects_complete || !function.liveness_converged)
                return analysis_error;
    }
    return 0;
}
