#include "ltlf_ek/ltlfsynt_verbose.hpp"

#include <cmath>
#include <cstdlib>
#include <regex>
#include <string>

namespace ltlf_ek {

LtlfsyntVerbose parse_ltlfsynt_verbose(std::string_view stderr_text) {
  static const std::regex stage_marker(R"( (?:took|created in|found in) )");
  static const std::regex stage_line(
      R"( (?:took|created in|found in) (\S+) seconds\s*$)");
  static const std::regex roots_line(R"(translation to MTDFA \((\d+) roots,)");

  LtlfsyntVerbose out;
  double total_seconds = 0;
  bool any_stage = false, malformed = false;

  const std::string text(stderr_text);
  std::size_t pos = 0;
  while (pos < text.size()) {
    std::size_t eol = text.find('\n', pos);
    if (eol == std::string::npos) eol = text.size();
    const std::string line = text.substr(pos, eol - pos);
    pos = eol + 1;

    std::smatch m;
    if (std::regex_search(line, stage_marker)) {
      // A line cut after "took", or missing its unit, is a stage line whose
      // time is unknown: it spoils the sum rather than being skipped.
      if (!std::regex_search(line, m, stage_line)) {
        malformed = true;
        continue;
      }
      const std::string num = m[1];
      char* end = nullptr;
      const double s = std::strtod(num.c_str(), &end);
      if (end == num.c_str() || *end != '\0' || !std::isfinite(s) || s < 0)
        malformed = true;
      else {
        total_seconds += s;
        any_stage = true;
      }
    }
    if (std::regex_search(line, m, roots_line))
      out.explored_roots = std::stoull(m[1]);
  }
  if (any_stage && !malformed)
    out.internal_ns = static_cast<std::uint64_t>(std::llround(total_seconds * 1e9));
  return out;
}

std::optional<bool> parse_ltlfsynt_verdict(int exit_code,
                                           std::string_view stdout_text) {
  if (exit_code == 0 && stdout_text.rfind("REALIZABLE\n", 0) == 0) return true;
  if (exit_code == 1 && stdout_text == "UNREALIZABLE\n") return false;
  return std::nullopt;
}

}  // namespace ltlf_ek
