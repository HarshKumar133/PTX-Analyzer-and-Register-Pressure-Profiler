#pragma once

#include "ptxscope/model.hpp"

#include <string_view>
#include <vector>

namespace ptxscope {

struct LexResult {
    std::vector<Token> tokens;
    std::vector<Diagnostic> diagnostics;
};

LexResult lex(std::string_view source, const std::string &file);

} // namespace ptxscope
