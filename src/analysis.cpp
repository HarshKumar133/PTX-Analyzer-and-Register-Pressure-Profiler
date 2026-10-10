#include "ptxscope/analysis.hpp"

#include <algorithm>
#include <chrono>
#include <deque>
#include <map>
#include <set>
#include <string_view>

namespace ptxscope {
namespace {

using RegisterSet = std::set<std::string>;

bool contains(const std::set<std::string> &values, const std::string &value) {
    return values.find(value) != values.end();
}

bool is_terminator(const Instruction &instruction) {
    return instruction.opcode == "bra" || instruction.opcode == "ret" ||
           instruction.opcode == "exit" || instruction.opcode == "trap";
}

bool is_predicate(const std::string &reg) {
    return reg.size() > 1 && reg[0] == '%' && reg[1] == 'p';
}

std::string register_class(const std::string &reg) {
    if (is_predicate(reg))
        return "predicate";
    if (reg.rfind("%tid.", 0) == 0 || reg.rfind("%ctaid.", 0) == 0 || reg.rfind("%ntid.", 0) == 0 ||
        reg.rfind("%nctaid.", 0) == 0 || reg.rfind("%laneid", 0) == 0 ||
        reg.rfind("%warpid", 0) == 0 || reg.rfind("%clock", 0) == 0 ||
        reg.rfind("%globaltimer", 0) == 0)
        return "other";
    return "gpr";
}

std::string base_opcode(const Instruction &instruction) {
    return instruction.opcode;
}

bool is_known_opcode(const std::string &op) {
    static const std::set<std::string> known{
        "mov",  "add",  "sub",   "mul", "mad",   "fma",  "div",  "rem",  "min",  "max",
        "and",  "or",   "xor",   "not", "shl",   "shr",  "neg",  "abs",  "cvt",  "cvta",
        "setp", "selp", "ld",    "st",  "bra",   "ret",  "exit", "trap", "nop",  "bar",
        "vote", "slct", "popc",  "clz", "bfind", "brev", "bfe",  "bfi",  "prmt", "testp",
        "rcp",  "sqrt", "rsqrt", "sin", "cos",   "ex2",  "lg2"};
    return known.contains(op);
}

struct Effects {
    RegisterSet use;
    RegisterSet def;
    bool complete{true};
};

Effects effects_of(const Instruction &instruction) {
    Effects effects;
    const auto op = base_opcode(instruction);
    auto add_operand_regs = [&](std::size_t index, RegisterSet &into) {
        if (index < instruction.operands.size()) {
            into.insert(instruction.operands[index].registers.begin(),
                        instruction.operands[index].registers.end());
        }
    };
    if (!is_known_opcode(op)) {
        effects.complete = false;
        for (const auto &operand : instruction.operands)
            effects.use.insert(operand.registers.begin(), operand.registers.end());
    } else if (op == "bra" || op == "ret" || op == "exit" || op == "trap" || op == "nop") {
        // No explicit register operands in this subset; a guard is handled below.
    } else if (op == "st") {
        for (const auto &operand : instruction.operands)
            effects.use.insert(operand.registers.begin(), operand.registers.end());
    } else if (op == "ld") {
        if (!instruction.operands.empty())
            add_operand_regs(0, effects.def);
        for (std::size_t i = 1; i < instruction.operands.size(); ++i)
            add_operand_regs(i, effects.use);
    } else if (op == "setp") {
        if (!instruction.operands.empty())
            add_operand_regs(0, effects.def);
        for (std::size_t i = 1; i < instruction.operands.size(); ++i)
            add_operand_regs(i, effects.use);
    } else if (op == "bar") {
        for (const auto &operand : instruction.operands)
            effects.use.insert(operand.registers.begin(), operand.registers.end());
    } else {
        if (!instruction.operands.empty())
            add_operand_regs(0, effects.def);
        for (std::size_t i = 1; i < instruction.operands.size(); ++i)
            add_operand_regs(i, effects.use);
    }

    if (!instruction.guard.empty())
        effects.use.insert(instruction.guard);
    if (!instruction.guard.empty()) {
        // A predicated write may leave the old destination unchanged when the guard is false.
        effects.use.insert(effects.def.begin(), effects.def.end());
    }
    return effects;
}

void add_edge(std::vector<BasicBlock> &blocks, std::size_t from, std::size_t to) {
    if (from >= blocks.size() || to >= blocks.size())
        return;
    auto &succ = blocks[from].successors;
    if (std::find(succ.begin(), succ.end(), to) == succ.end()) {
        succ.push_back(to);
        blocks[to].predecessors.push_back(from);
    }
}

void sort_graph(std::vector<BasicBlock> &blocks) {
    for (auto &block : blocks) {
        std::sort(block.predecessors.begin(), block.predecessors.end());
        std::sort(block.successors.begin(), block.successors.end());
    }
}

std::vector<std::string> counts(const RegisterSet &regs, const std::string &wanted) {
    std::vector<std::string> result;
    for (const auto &reg : regs)
        if (register_class(reg) == wanted)
            result.push_back(reg);
    return result;
}

} // namespace

AnalysisReport analyze(Program program) {
    AnalysisReport report;
    report.program = std::move(program);
    report.diagnostics = report.program.diagnostics;

    for (const auto &function : report.program.functions) {
        const auto cfg_start = std::chrono::steady_clock::now();
        FunctionAnalysis result;
        result.instruction_liveness.resize(function.instructions.size());
        std::set<std::size_t> boundaries{0};
        std::map<std::string, std::size_t> label_positions;
        for (const auto &label : function.labels) {
            auto [it, inserted] = label_positions.emplace(label.name, label.instruction_index);
            if (!inserted) {
                report.diagnostics.push_back(
                    {"PTX2101", Severity::error,
                     "duplicate label '" + label.name + "' in function '" + function.name + "'",
                     label.location, "branch targets must be unique within a function"});
            }
            if (label.instruction_index < function.instructions.size())
                boundaries.insert(label.instruction_index);
        }
        for (std::size_t i = 0; i < function.instructions.size(); ++i) {
            if (is_terminator(function.instructions[i]) && i + 1 < function.instructions.size())
                boundaries.insert(i + 1);
        }
        std::vector<std::size_t> starts(boundaries.begin(), boundaries.end());
        if (!function.instructions.empty()) {
            for (std::size_t b = 0; b < starts.size(); ++b) {
                BasicBlock block;
                block.id = b;
                const auto end =
                    b + 1 < starts.size() ? starts[b + 1] : function.instructions.size();
                for (std::size_t i = starts[b]; i < end; ++i)
                    block.instruction_indices.push_back(i);
                result.blocks.push_back(std::move(block));
            }
        }
        std::vector<std::size_t> instruction_to_block(function.instructions.size(), 0);
        for (auto &block : result.blocks) {
            for (const auto index : block.instruction_indices)
                instruction_to_block[index] = block.id;
        }
        for (const auto &label : function.labels) {
            if (label.instruction_index < instruction_to_block.size())
                result.blocks[instruction_to_block[label.instruction_index]].labels.push_back(
                    label.name);
        }

        for (const auto &instruction : function.instructions) {
            if (instruction.opcode == "brx" || instruction.opcode == "brk" ||
                instruction.opcode == "cont") {
                result.effects_complete = false;
                report.diagnostics.push_back(
                    {"PTX2103", Severity::warning,
                     "control-flow instruction '" + instruction.opcode +
                         "' is not modeled in the CFG for function '" + function.name + "'",
                     instruction.location,
                     "dependent control-flow and liveness results are incomplete"});
            }
        }

        for (std::size_t b = 0; b < result.blocks.size(); ++b) {
            const auto &block = result.blocks[b];
            if (block.instruction_indices.empty())
                continue;
            const auto &last = function.instructions[block.instruction_indices.back()];
            if (last.opcode == "bra") {
                std::string target;
                if (!last.operands.empty())
                    target = last.operands.front().text;
                const auto target_it = label_positions.find(target);
                if (target_it == label_positions.end() ||
                    target_it->second >= instruction_to_block.size()) {
                    report.diagnostics.push_back({"PTX2102", Severity::error,
                                                  "unresolved branch target '" + target +
                                                      "' in function '" + function.name + "'",
                                                  last.location, "the function CFG is incomplete"});
                    result.effects_complete = false;
                } else {
                    add_edge(result.blocks, b, instruction_to_block[target_it->second]);
                }
                if (!last.guard.empty() && b + 1 < result.blocks.size())
                    add_edge(result.blocks, b, b + 1);
            } else if (last.opcode != "ret" && last.opcode != "exit" && last.opcode != "trap" &&
                       b + 1 < result.blocks.size()) {
                add_edge(result.blocks, b, b + 1);
            }
        }
        sort_graph(result.blocks);
        for (const auto &block : result.blocks)
            result.edge_count += block.successors.size();

        if (!result.blocks.empty()) {
            std::deque<std::size_t> work{0};
            result.blocks[0].reachable = true;
            while (!work.empty()) {
                const auto current = work.front();
                work.pop_front();
                for (const auto next : result.blocks[current].successors) {
                    if (!result.blocks[next].reachable) {
                        result.blocks[next].reachable = true;
                        work.push_back(next);
                    }
                }
            }
        }
        for (const auto &block : result.blocks) {
            if (!block.reachable) {
                ++result.unreachable_blocks;
                SourceLocation where = function.location;
                if (!block.instruction_indices.empty())
                    where = function.instructions[block.instruction_indices.front()].location;
                report.diagnostics.push_back(
                    {"PTX2202", Severity::warning,
                     "unreachable basic block " + std::to_string(block.id) + " in '" +
                         function.name + "'",
                     where, "reachability is computed from the function entry"});
            }
        }
        result.cfg_time_ms =
            std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - cfg_start)
                .count();

        for (const auto &decl : function.declarations)
            result.declared_registers.insert(decl.names.begin(), decl.names.end());
        std::set<std::string> observed;
        std::vector<Effects> instruction_effects;
        instruction_effects.reserve(function.instructions.size());
        for (const auto &instruction : function.instructions) {
            auto effects = effects_of(instruction);
            observed.insert(effects.use.begin(), effects.use.end());
            observed.insert(effects.def.begin(), effects.def.end());
            if (!effects.complete) {
                result.effects_complete = false;
                report.diagnostics.push_back(
                    {"PTX2201", Severity::warning,
                     "register effects for instruction '" + instruction.opcode +
                         "' are unknown; liveness is incomplete",
                     instruction.location, "unknown operands are conservatively treated as reads"});
            }
            if (instruction.opcode == "mov" && instruction.operands.size() == 2 &&
                instruction.operands[0].text == instruction.operands[1].text &&
                !instruction.operands[0].registers.empty()) {
                report.diagnostics.push_back(
                    {"PTX2203", Severity::info,
                     "self-move of '" + instruction.operands[0].text + "' is potentially redundant",
                     instruction.location,
                     "review the instruction in its full type and control-flow context"});
            }
            instruction_effects.push_back(std::move(effects));
        }
        for (const auto &reg : result.declared_registers) {
            if (!contains(observed, reg)) {
                report.diagnostics.push_back(
                    {"PTX2204", Severity::info,
                     "declared register '" + reg + "' is not referenced in function '" +
                         function.name + "'",
                     function.location,
                     "declaration arrays are expanded to individual virtual registers"});
            }
        }

        const auto liveness_start = std::chrono::steady_clock::now();
        result.block_liveness.resize(result.blocks.size());
        for (const auto &block : result.blocks) {
            auto &live = result.block_liveness[block.id];
            for (const auto index : block.instruction_indices) {
                const auto &effects = instruction_effects[index];
                for (const auto &reg : effects.use)
                    if (!contains(live.def, reg))
                        live.use.insert(reg);
                live.def.insert(effects.def.begin(), effects.def.end());
            }
        }
        bool changed = true;
        while (changed && result.iterations <= result.blocks.size() * (observed.size() + 1) + 1) {
            changed = false;
            ++result.iterations;
            for (std::size_t reverse = result.blocks.size(); reverse > 0; --reverse) {
                const auto b = reverse - 1;
                auto &live = result.block_liveness[b];
                RegisterSet new_out;
                for (const auto successor : result.blocks[b].successors) {
                    const auto &successor_in = result.block_liveness[successor].live_in;
                    new_out.insert(successor_in.begin(), successor_in.end());
                }
                RegisterSet new_in = live.use;
                for (const auto &reg : new_out)
                    if (!contains(live.def, reg))
                        new_in.insert(reg);
                if (new_out != live.live_out || new_in != live.live_in) {
                    live.live_out = std::move(new_out);
                    live.live_in = std::move(new_in);
                    changed = true;
                }
            }
        }
        if (changed) {
            result.liveness_converged = false;
            report.diagnostics.push_back(
                {"PTX2301",
                 Severity::error,
                 "liveness solver did not converge within its safety bound for '" + function.name +
                     "'",
                 function.location,
                 {}});
        }
        for (const auto &block : result.blocks) {
            RegisterSet live = result.block_liveness[block.id].live_out;
            for (auto reverse = block.instruction_indices.rbegin();
                 reverse != block.instruction_indices.rend(); ++reverse) {
                const auto index = *reverse;
                auto &at = result.instruction_liveness[index];
                at.live_out = live;
                for (const auto &reg : instruction_effects[index].def)
                    live.erase(reg);
                live.insert(instruction_effects[index].use.begin(),
                            instruction_effects[index].use.end());
                at.live_in = live;
            }
        }

        for (std::size_t i = 0; i < result.instruction_liveness.size(); ++i) {
            RegisterSet boundary = result.instruction_liveness[i].live_in;
            boundary.insert(result.instruction_liveness[i].live_out.begin(),
                            result.instruction_liveness[i].live_out.end());
            const auto gpr_count = counts(boundary, "gpr").size();
            const auto pred_count = counts(boundary, "predicate").size();
            const auto other_count = counts(boundary, "other").size();
            const auto block_id = instruction_to_block[i];
            auto &block = result.blocks[block_id];
            if (gpr_count + pred_count + other_count >
                block.peak_gpr + block.peak_predicate + block.peak_other) {
                block.peak_gpr = gpr_count;
                block.peak_predicate = pred_count;
                block.peak_other = other_count;
                block.peak_instruction = i;
            }
            if (gpr_count + pred_count + other_count >
                result.peak_gpr + result.peak_predicate + result.peak_other) {
                result.peak_gpr = gpr_count;
                result.peak_predicate = pred_count;
                result.peak_other = other_count;
                result.peak_instruction = i;
            }
        }
        result.liveness_time_ms = std::chrono::duration<double, std::milli>(
                                      std::chrono::steady_clock::now() - liveness_start)
                                      .count();
        report.functions.push_back(std::move(result));
    }
    std::stable_sort(report.diagnostics.begin(), report.diagnostics.end(),
                     [](const Diagnostic &a, const Diagnostic &b) {
                         if (a.location.file != b.location.file)
                             return a.location.file < b.location.file;
                         if (a.location.line != b.location.line)
                             return a.location.line < b.location.line;
                         if (a.location.column != b.location.column)
                             return a.location.column < b.location.column;
                         return a.code < b.code;
                     });
    return report;
}

} // namespace ptxscope
