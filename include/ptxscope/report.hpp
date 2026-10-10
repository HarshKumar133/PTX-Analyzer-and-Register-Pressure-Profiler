#pragma once

#include "ptxscope/model.hpp"

#include <string>

namespace ptxscope {

std::string render_text(const AnalysisReport &report, bool verbose);
std::string render_json(const AnalysisReport &report, bool include_timings = false);
std::string severity_name(Severity severity);

} // namespace ptxscope
