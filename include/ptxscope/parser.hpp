#pragma once

#include "ptxscope/lexer.hpp"

namespace ptxscope {

Program parse(const LexResult &lexed, const std::string &input_name);

} // namespace ptxscope
