#pragma once

#include <cstdint>
#include <optional>
#include <string_view>

namespace ltlf_ek {

// What `ltlfsynt --verbose` reports about its own work on stderr. A field is
// nullopt when the transcript does not carry it; a missing figure is never 0.
struct LtlfsyntVerbose {
  // Sum of the stage times (every "... took S seconds" line), in nanoseconds:
  // the solver's work without process startup. nullopt when no stage line
  // parses, or any line containing " took " does not match that form.
  std::optional<std::uint64_t> internal_ns;
  // The root count of the "translation to MTDFA (R roots, A APs)" line.
  std::optional<std::uint64_t> explored_roots;
};

LtlfsyntVerbose parse_ltlfsynt_verbose(std::string_view stderr_text);

}  // namespace ltlf_ek
