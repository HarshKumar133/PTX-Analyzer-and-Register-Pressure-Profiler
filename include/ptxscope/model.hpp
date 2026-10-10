#pragma once

#include <cstddef>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace ptxscope {

struct SourceLocation {
    std::string file;
    std::size_t line{1};
    std::size_t column{1};
};

enum class Severity { info, warning, error };

struct Diagnostic {
    std::string code;
    Severity severity{Severity::warning};
    std::string message;
    SourceLocation location;
    std::string note;
};

enum class TokenKind {
    identifier,
    directive,
    reg,
    integer,
    floating,
    string_literal,
    punctuation,
    end
};

struct Token {
    TokenKind kind{TokenKind::end};
    std::string text;
    SourceLocation location;
};

enum class OperandKind { reg, immediate, label, symbol, address, opaque };

struct Operand {
    OperandKind kind{OperandKind::opaque};
    std::string text;
    std::vector<std::string> registers;
};

struct Instruction {
    std::size_t id{0};
    std::string opcode;
    std::vector<std::string> modifiers;
    std::vector<Operand> operands;
    std::string guard;
    bool guard_negated{false};
    SourceLocation location;
    std::string raw;
};

struct RegisterDeclaration {
    std::string type;
    std::vector<std::string> names;
    SourceLocation location;
};

struct Label {
    std::string name;
    std::size_t instruction_index{0};
    SourceLocation location;
};

struct Function {
    std::size_t id{0};
    std::string name;
    bool entry{false};
    SourceLocation location;
    std::vector<RegisterDeclaration> declarations;
    std::vector<Instruction> instructions;
    std::vector<Label> labels;
    bool parse_valid{true};
};

struct Program {
    std::string input_name;
    std::string ptx_version;
    std::string target;
    std::vector<Function> functions;
    std::vector<Diagnostic> diagnostics;
    bool parse_valid{true};
};

struct BasicBlock {
    std::size_t id{0};
    std::vector<std::size_t> instruction_indices;
    std::vector<std::string> labels;
    std::vector<std::size_t> predecessors;
    std::vector<std::size_t> successors;
    bool reachable{false};
    std::size_t peak_gpr{0};
    std::size_t peak_predicate{0};
    std::size_t peak_other{0};
    std::size_t peak_instruction{0};
};

struct InstructionLiveness {
    std::set<std::string> live_in;
    std::set<std::string> live_out;
};

struct BlockLiveness {
    std::set<std::string> use;
    std::set<std::string> def;
    std::set<std::string> live_in;
    std::set<std::string> live_out;
};

struct FunctionAnalysis {
    std::vector<BasicBlock> blocks;
    std::vector<BlockLiveness> block_liveness;
    std::vector<InstructionLiveness> instruction_liveness;
    std::size_t edge_count{0};
    std::size_t unreachable_blocks{0};
    std::size_t iterations{0};
    bool liveness_converged{true};
    bool effects_complete{true};
    std::size_t peak_gpr{0};
    std::size_t peak_predicate{0};
    std::size_t peak_other{0};
    std::size_t peak_instruction{0};
    double cfg_time_ms{0.0};
    double liveness_time_ms{0.0};
    std::set<std::string> declared_registers;
};

struct AnalysisReport {
    Program program;
    std::vector<FunctionAnalysis> functions;
    std::vector<Diagnostic> diagnostics;
    double parse_time_ms{0.0};
    double analysis_time_ms{0.0};
};

} // namespace ptxscope
