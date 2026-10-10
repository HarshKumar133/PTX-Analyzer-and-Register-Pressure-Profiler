#include "ptxscope/lexer.hpp"

#include <cctype>

namespace ptxscope {
namespace {

bool alpha(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' || c == '$';
}

bool digit(char c) {
    return c >= '0' && c <= '9';
}

bool identifier_char(char c) {
    return alpha(c) || digit(c);
}

} // namespace

LexResult lex(std::string_view source, const std::string &file) {
    LexResult result;
    std::size_t i = 0;
    std::size_t line = 1;
    std::size_t column = 1;
    auto loc = [&]() { return SourceLocation{file, line, column}; };
    auto advance = [&]() {
        if (source[i] == '\n') {
            ++line;
            column = 1;
        } else {
            ++column;
        }
        ++i;
    };
    auto emit = [&](TokenKind kind, std::size_t start, SourceLocation start_loc) {
        result.tokens.push_back(
            Token{kind, std::string(source.substr(start, i - start)), start_loc});
    };

    while (i < source.size()) {
        const char c = source[i];
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\f') {
            advance();
            continue;
        }
        if (c == '/' && i + 1 < source.size() && source[i + 1] == '/') {
            while (i < source.size() && source[i] != '\n')
                advance();
            continue;
        }
        if (c == '/' && i + 1 < source.size() && source[i + 1] == '*') {
            const auto start_loc = loc();
            advance();
            advance();
            bool closed = false;
            while (i < source.size()) {
                if (source[i] == '*' && i + 1 < source.size() && source[i + 1] == '/') {
                    advance();
                    advance();
                    closed = true;
                    break;
                }
                advance();
            }
            if (!closed) {
                result.diagnostics.push_back({"PTX1002", Severity::error,
                                              "unterminated block comment", start_loc,
                                              "close the comment with */"});
            }
            continue;
        }

        const auto start = i;
        const auto start_loc = loc();
        if (c == '.') {
            advance();
            while (i < source.size() && identifier_char(source[i]))
                advance();
            if (i == start + 1) {
                result.diagnostics.push_back({"PTX1001",
                                              Severity::error,
                                              "expected a directive or modifier name after '.'",
                                              start_loc,
                                              {}});
            }
            emit(TokenKind::directive, start, start_loc);
            continue;
        }
        if (c == '%') {
            advance();
            while (i < source.size() && identifier_char(source[i]))
                advance();
            while (i < source.size() && source[i] == '.') {
                advance();
                while (i < source.size() && identifier_char(source[i]))
                    advance();
            }
            if (i == start + 1) {
                result.diagnostics.push_back({"PTX1001",
                                              Severity::error,
                                              "expected a register name after '%'",
                                              start_loc,
                                              {}});
            }
            emit(TokenKind::reg, start, start_loc);
            continue;
        }
        if (alpha(c)) {
            advance();
            while (i < source.size() && identifier_char(source[i]))
                advance();
            emit(TokenKind::identifier, start, start_loc);
            continue;
        }
        if (digit(c)) {
            advance();
            while (i < source.size() && (identifier_char(source[i]) || source[i] == '.'))
                advance();
            if (i < source.size() && (source[i] == '+' || source[i] == '-') && i > start &&
                (source[i - 1] == 'e' || source[i - 1] == 'E')) {
                advance();
                while (i < source.size() && (identifier_char(source[i]) || source[i] == '.'))
                    advance();
            }
            const auto spelling = source.substr(start, i - start);
            const bool is_float = spelling.find('.') != std::string_view::npos ||
                                  spelling.find('e') != std::string_view::npos ||
                                  spelling.find('E') != std::string_view::npos;
            emit(is_float ? TokenKind::floating : TokenKind::integer, start, start_loc);
            continue;
        }
        if (c == '"') {
            advance();
            bool closed = false;
            while (i < source.size()) {
                if (source[i] == '\\' && i + 1 < source.size()) {
                    advance();
                    advance();
                } else if (source[i] == '"') {
                    advance();
                    closed = true;
                    break;
                } else if (source[i] == '\n') {
                    break;
                } else {
                    advance();
                }
            }
            emit(TokenKind::string_literal, start, start_loc);
            if (!closed) {
                result.diagnostics.push_back(
                    {"PTX1003", Severity::error, "unterminated string literal", start_loc, {}});
            }
            continue;
        }

        constexpr std::string_view punctuation = "{}[](),;:!@+-*/|^&~<>=";
        if (punctuation.find(c) != std::string_view::npos) {
            advance();
            if (i < source.size() && ((source[i - 1] == '<' && source[i] == '<') ||
                                      (source[i - 1] == '>' && source[i] == '>')))
                advance();
            emit(TokenKind::punctuation, start, start_loc);
            continue;
        }
        result.diagnostics.push_back(
            {"PTX1001", Severity::error, "invalid character in PTX source", start_loc, {}});
        advance();
    }
    result.tokens.push_back(Token{TokenKind::end, "", SourceLocation{file, line, column}});
    return result;
}

} // namespace ptxscope
