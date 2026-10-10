#include "ptxscope/parser.hpp"

#include <algorithm>
#include <cctype>
#include <sstream>

namespace ptxscope {
namespace {

class Parser {
  public:
    Parser(const LexResult &lexed, const std::string &name)
        : tokens_(lexed.tokens), input_name_(name) {
        program_.input_name = name;
        program_.diagnostics = lexed.diagnostics;
    }

    Program run() {
        while (!at_end()) {
            if (is_directive(".entry") || is_directive(".func")) {
                parse_function();
            } else if (is_directive(".version") || is_directive(".target") ||
                       is_directive(".address_size")) {
                parse_metadata();
            } else if (is_directive(".visible")) {
                take();
            } else if (peek().kind == TokenKind::directive) {
                const auto token = take();
                program_.diagnostics.push_back(
                    {"PTX1101", Severity::warning,
                     "directive '" + token.text + "' is not interpreted by this subset",
                     token.location, "declaration syntax is skipped through the next semicolon"});
                skip_statement();
            } else {
                take();
            }
        }
        for (const auto &d : program_.diagnostics) {
            if (d.severity == Severity::error)
                program_.parse_valid = false;
        }
        return std::move(program_);
    }

  private:
    const Token &peek(std::size_t offset = 0) const {
        const auto index = std::min(position_ + offset, tokens_.size() - 1);
        return tokens_[index];
    }
    Token take() {
        return tokens_[position_++];
    }
    bool at_end() const {
        return peek().kind == TokenKind::end;
    }
    bool is(const std::string &text) const {
        return peek().text == text;
    }
    bool is_directive(const std::string &text) const {
        return peek().kind == TokenKind::directive && peek().text == text;
    }
    void error(std::string code, std::string message, const SourceLocation &location,
               std::string note = {}) {
        program_.diagnostics.push_back(
            {std::move(code), Severity::error, std::move(message), location, std::move(note)});
    }
    void skip_statement() {
        while (!at_end() && !is(";"))
            take();
        if (is(";"))
            take();
    }

    void parse_metadata() {
        const Token directive = take();
        if (at_end()) {
            error("PTX1102", "missing value after '" + directive.text + "'", directive.location);
            return;
        }
        const Token value = take();
        if (directive.text == ".version")
            program_.ptx_version = value.text;
        if (directive.text == ".target")
            program_.target = value.text;
        while (!at_end() && !is(";") && peek().kind != TokenKind::directive)
            take();
        if (is(";"))
            take();
    }

    void parse_function() {
        const Token start = take();
        const bool entry = start.text == ".entry";
        std::size_t name_index = tokens_.size();
        if (entry) {
            for (std::size_t j = position_; j < tokens_.size() && tokens_[j].text != "{"; ++j) {
                if (tokens_[j].kind == TokenKind::identifier) {
                    name_index = j;
                    break;
                }
            }
        } else {
            for (std::size_t j = position_; j + 1 < tokens_.size() && tokens_[j].text != "{"; ++j) {
                if (tokens_[j].kind == TokenKind::identifier && tokens_[j + 1].text == "(") {
                    name_index = j;
                }
            }
        }
        if (name_index == tokens_.size()) {
            error("PTX1103", "could not find a function name and body", start.location);
            skip_statement();
            return;
        }
        Function function;
        function.id = program_.functions.size();
        function.name = tokens_[name_index].text;
        function.entry = entry;
        function.location = start.location;
        position_ = name_index + 1;
        while (!at_end() && !is("{"))
            take();
        if (!is("{")) {
            program_.diagnostics.push_back({"PTX1104",
                                            Severity::warning,
                                            "function declaration has no body and is not analyzed",
                                            function.location,
                                            {}});
            return;
        }
        take();
        bool closed = false;
        while (!at_end()) {
            if (is("}")) {
                take();
                closed = true;
                break;
            }
            if (peek().kind == TokenKind::directive && peek().text == ".reg") {
                parse_register_declaration(function);
            } else if (peek().kind == TokenKind::directive) {
                const auto directive = take();
                program_.diagnostics.push_back(
                    {"PTX1101", Severity::warning,
                     "directive '" + directive.text + "' is not semantically modeled",
                     directive.location,
                     "its declaration is skipped; memory effects are not analyzed"});
                skip_statement();
            } else if (peek().kind == TokenKind::identifier && peek(1).text == ":") {
                const Token label = take();
                take();
                function.labels.push_back(
                    Label{label.text, function.instructions.size(), label.location});
            } else {
                parse_instruction(function);
            }
        }
        if (!closed) {
            function.parse_valid = false;
            error("PTX1105", "unterminated function body", function.location);
        }
        program_.functions.push_back(std::move(function));
    }

    void parse_register_declaration(Function &function) {
        const Token start = take();
        RegisterDeclaration decl;
        decl.location = start.location;
        if (peek().kind == TokenKind::directive)
            decl.type = take().text;
        else {
            function.parse_valid = false;
            error("PTX1110", "expected a register type after .reg", peek().location);
        }
        while (!at_end() && !is(";") && !is("}")) {
            if (peek().kind == TokenKind::reg) {
                std::string base = take().text;
                if (is("<") && peek(1).kind == TokenKind::integer && peek(2).text == ">") {
                    take();
                    const std::string count_text = take().text;
                    take();
                    try {
                        const auto count = static_cast<std::size_t>(std::stoul(count_text));
                        if (count > 4096)
                            throw std::out_of_range("large register array");
                        for (std::size_t n = 0; n < count; ++n)
                            decl.names.push_back(base + std::to_string(n));
                    } catch (...) {
                        function.parse_valid = false;
                        error("PTX1111", "invalid or excessive register array size",
                              start.location);
                    }
                } else {
                    decl.names.push_back(std::move(base));
                }
            } else if (is(",")) {
                take();
            } else {
                function.parse_valid = false;
                error("PTX1113", "unexpected token in register declaration", peek().location,
                      "expected a register name or register-array size");
                skip_statement();
                break;
            }
        }
        if (is(";"))
            take();
        else {
            function.parse_valid = false;
            error("PTX1112", "expected ';' after register declaration", peek().location);
        }
        function.declarations.push_back(std::move(decl));
    }

    static std::string join_tokens(const std::vector<Token> &tokens) {
        std::string out;
        for (const auto &token : tokens) {
            if (!out.empty() && token.text != "]" && token.text != ")" && token.text != "," &&
                token.text != ":" && out.back() != '[' && out.back() != '(')
                out.push_back(' ');
            out += token.text;
        }
        return out;
    }

    void parse_instruction(Function &function) {
        const auto start_position = position_;
        const auto start_location = peek().location;
        Instruction instruction;
        instruction.id = function.instructions.size();
        instruction.location = start_location;
        if (is("@")) {
            take();
            if (is("!")) {
                instruction.guard_negated = true;
                take();
            }
            if (peek().kind == TokenKind::reg)
                instruction.guard = take().text;
            else {
                function.parse_valid = false;
                error("PTX1120", "expected a predicate register after instruction guard",
                      peek().location);
            }
        }
        if (peek().kind != TokenKind::identifier) {
            function.parse_valid = false;
            error("PTX1121", "expected an instruction mnemonic", peek().location);
            recover_to_semicolon();
            return;
        }
        instruction.opcode = take().text;
        while (peek().kind == TokenKind::directive) {
            auto mod = take().text;
            if (!mod.empty() && mod.front() == '.')
                mod.erase(mod.begin());
            instruction.modifiers.push_back(std::move(mod));
        }
        std::vector<Token> current;
        int square_depth = 0;
        int paren_depth = 0;
        while (!at_end() && !is(";") && !is("}")) {
            if (is("["))
                ++square_depth;
            if (is("]"))
                --square_depth;
            if (is("("))
                ++paren_depth;
            if (is(")"))
                --paren_depth;
            if (is(",") && square_depth == 0 && paren_depth == 0) {
                instruction.operands.push_back(make_operand(current));
                current.clear();
                take();
            } else {
                current.push_back(take());
            }
        }
        if (!current.empty())
            instruction.operands.push_back(make_operand(current));
        if (is(";"))
            take();
        else {
            function.parse_valid = false;
            error("PTX1122", "expected ';' after instruction", peek().location);
        }
        std::vector<Token> raw_tokens(tokens_.begin() + static_cast<std::ptrdiff_t>(start_position),
                                      tokens_.begin() + static_cast<std::ptrdiff_t>(position_));
        instruction.raw = join_tokens(raw_tokens);
        function.instructions.push_back(std::move(instruction));
    }

    Operand make_operand(const std::vector<Token> &tokens) {
        Operand operand;
        operand.text = join_tokens(tokens);
        for (const auto &token : tokens) {
            if (token.kind == TokenKind::reg)
                operand.registers.push_back(token.text);
        }
        if (tokens.empty())
            return operand;
        if (tokens.front().text == "[")
            operand.kind = OperandKind::address;
        else if (tokens.front().kind == TokenKind::reg)
            operand.kind = OperandKind::reg;
        else if (tokens.front().kind == TokenKind::integer ||
                 tokens.front().kind == TokenKind::floating || tokens.front().text == "-" ||
                 tokens.front().text == "+")
            operand.kind = OperandKind::immediate;
        else if (tokens.size() == 1 && tokens.front().kind == TokenKind::identifier)
            operand.kind = OperandKind::symbol;
        else
            operand.kind = OperandKind::opaque;
        return operand;
    }

    void recover_to_semicolon() {
        while (!at_end() && !is(";") && !is("}"))
            take();
        if (is(";"))
            take();
    }

    const std::vector<Token> &tokens_;
    std::string input_name_;
    std::size_t position_{0};
    Program program_;
};

} // namespace

Program parse(const LexResult &lexed, const std::string &input_name) {
    return Parser(lexed, input_name).run();
}

} // namespace ptxscope
