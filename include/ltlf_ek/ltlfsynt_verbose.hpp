#pragma once

#include <cstdint>
#include <optional>
#include <string_view>

namespace ltlf_ek {

// What `ltlfsynt --verbose` reports about its own work on stderr. A field is
// nullopt when the transcript does not carry it; a missing figure is never 0.
struct LtlfsyntVerbose {
  // Sum of the stage times (every "... took S seconds", "... created in S
  // seconds" or "... found in S seconds" line), in nanoseconds: the solver's
  // work without process startup. nullopt when no stage line parses, or any
  // such line does not match that form. Other lines, such as "game solved in",
  // are not summed.
  std::optional<std::uint64_t> internal_ns;
};

LtlfsyntVerbose parse_ltlfsynt_verbose(std::string_view stderr_text);

// The verdict of a full-synthesis `ltlfsynt` run: true for exit code 0 with
// stdout opening "REALIZABLE" (the strategy follows it), false for exit code 1
// with stdout exactly "UNREALIZABLE". nullopt for anything else.
std::optional<bool> parse_ltlfsynt_verdict(int exit_code,
                                           std::string_view stdout_text);

}  // namespace ltlf_ek
