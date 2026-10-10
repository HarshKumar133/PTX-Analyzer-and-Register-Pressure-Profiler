#include "ptxscope/analysis.hpp"
#include "ptxscope/lexer.hpp"
#include "ptxscope/parser.hpp"
#include "ptxscope/report.hpp"

#include <cstdlib>
#include <iostream>
#include <string>

namespace {

int failures = 0;

void expect(bool condition, const std::string &message) {
    if (!condition) {
        ++failures;
        std::cerr << "FAIL: " << message << '\n';
    }
}

ptxscope::AnalysisReport analyze_text(const std::string &source) {
    const auto tokens = ptxscope::lex(source, "unit.ptx");
    return ptxscope::analyze(ptxscope::parse(tokens, "unit.ptx"));
}

void lexer_test() {
    const auto result =
        ptxscope::lex("// comment\n.reg .pred %p0; /* ok */ add.s32 %r1, %r2, 1;", "lex.ptx");
    expect(result.diagnostics.empty(), "comments lex without diagnostics");
    expect(result.tokens.front().text == ".reg",
           "line comments are skipped and directives tokenized");
    expect(result.tokens.front().location.line == 2, "source line is preserved");
    const auto invalid = ptxscope::lex("/* not closed", "bad.ptx");
    expect(!invalid.diagnostics.empty() && invalid.diagnostics.front().code == "PTX1002",
           "unterminated comment is diagnosed");
}

void parser_test() {
    const std::string source = R"(.version 7.0
.target sm_70
.visible .entry alpha() { .reg .b32 %r<2>; mov.u32 %r0, 1; ret; }
.func beta() { .reg .pred %p0; ret; }
)";
    const auto parsed = ptxscope::parse(ptxscope::lex(source, "parse.ptx"), "parse.ptx");
    expect(parsed.parse_valid, "small supported functions parse");
    expect(parsed.functions.size() == 2, "multiple functions parse");
    expect(parsed.ptx_version == "7.0" && parsed.target == "sm_70", "PTX metadata is retained");
    expect(parsed.functions[0].declarations.front().names.size() == 2,
           "register declaration arrays expand to individual names");
}

void cfg_and_liveness_test() {
    const std::string source = R"(.entry loop() {
.reg .pred %p0;
.reg .b32 %r0, %r1;
mov.u32 %r0, 1;
L: add.s32 %r0, %r0, %r1;
setp.ne.s32 %p0, %r0, 8;
@%p0 bra L;
ret;
}
)";
    const auto report = analyze_text(source);
    expect(report.program.parse_valid, "loop fixture parses");
    expect(report.functions.size() == 1, "loop function is analyzed");
    const auto &analysis = report.functions.front();
    expect(analysis.blocks.size() == 3, "loop forms entry, loop, and exit blocks");
    expect(analysis.edge_count == 3, "conditional back edge and fallthrough edges are present");
    expect(analysis.liveness_converged, "loop liveness reaches a fixed point");
    expect(analysis.instruction_liveness[1].live_in.contains("%r1"),
           "register read in loop is live at instruction entry");
    expect(analysis.instruction_liveness[2].live_out.contains("%p0"),
           "predicate remains live through guarded branch");
}

void diamond_and_predicated_write_test() {
    const auto report = analyze_text(R"(.entry diamond() {
.reg .pred %p0;
.reg .b32 %r0, %r1;
setp.eq.s32 %p0, %r1, 0;
@%p0 bra LEFT;
@%p0 add.s32 %r0, %r0, 1;
bra JOIN;
LEFT: mov.u32 %r0, 2;
JOIN: ret;
}
)");
    const auto &analysis = report.functions.front();
    expect(analysis.blocks.size() == 4, "diamond splits at target and after branch");
    expect(analysis.blocks[0].successors.size() == 2,
           "conditional branch has target and fallthrough");
    expect(analysis.instruction_liveness[2].live_in.contains("%r0"),
           "predicated destination old value remains live");
}

void unreachable_and_parse_error_test() {
    const auto report = analyze_text(R"(.entry dead_code() {
.reg .b32 %r0;
bra DONE;
DEAD: mov.u32 %r0, 0;
DONE: ret;
}
)");
    expect(report.functions.front().unreachable_blocks == 1, "unreachable block is identified");
    bool unreachable_diagnostic = false;
    for (const auto &diagnostic : report.diagnostics)
        unreachable_diagnostic |= diagnostic.code == "PTX2202";
    expect(unreachable_diagnostic, "unreachable block diagnostic has a stable code");
    const auto bad =
        ptxscope::parse(ptxscope::lex(".entry bad() { .reg .b32 %r0 }", "bad.ptx"), "bad.ptx");
    expect(!bad.parse_valid, "missing declaration semicolon is rejected");
}

void diagnostics_and_json_test() {
    const auto report = analyze_text(R"(.entry bad() {
.reg .b32 %r0;
mov.u32 %r0, %r0;
bra nowhere;
}
)");
    bool unresolved = false;
    bool self_move = false;
    for (const auto &diagnostic : report.diagnostics) {
        unresolved |= diagnostic.code == "PTX2102";
        self_move |= diagnostic.code == "PTX2203";
    }
    expect(unresolved, "undefined branch target is diagnosed");
    expect(self_move, "self-move pattern is reported");
    const auto json = ptxscope::render_json(report);
    expect(json.find("\"schema_version\":1") != std::string::npos,
           "JSON report includes a schema version");
    expect(json.find("\"PTX2102\"") != std::string::npos,
           "JSON report includes structured diagnostic codes");
    const auto unknown = analyze_text(".entry f() { .reg .b32 %r0; mystery.u32 %r0; ret; }");
    expect(!unknown.functions.front().effects_complete,
           "unknown instructions mark effects incomplete");
}

} // namespace

int main() {
    lexer_test();
    parser_test();
    cfg_and_liveness_test();
    diamond_and_predicated_write_test();
    unreachable_and_parse_error_test();
    diagnostics_and_json_test();
    if (failures)
        return EXIT_FAILURE;
    std::cout << "All PTXScope unit checks passed.\n";
    return EXIT_SUCCESS;
}
