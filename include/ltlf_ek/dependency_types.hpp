#pragma once

#include <functional>
#include <optional>
#include <stdexcept>
#include <string>

namespace ltlf_ek {

// Thrown when the language the dependency analysis runs on is empty --- L(phi)
// for `dependent_outputs` (phi unsatisfiable), L(!phi) for `dependent_inputs`
// (phi valid).  See Edge cases in docs/prd/output-dependencies-tool.md and
// docs/prd/input-dependencies-tool.md.  In both cases every Xdep would
// otherwise be VACUOUSLY dependent, so the greedy loop would confidently
// return Xdep = the whole scanned set.
//
// A distinct TYPE, not a distinguishing message.  Both entry points throw
// std::invalid_argument for unrelated reasons too, and only this one earns its
// own CLI exit code; dispatching on a what() substring instead misfires
// whenever another message happens to contain the word --- an AP literally
// named `unsatisfiable` made the closed-universe refusal exit 3 --- and
// regresses silently if the wording is ever reworded.  Derives from
// std::invalid_argument so callers catching the base still catch this.
struct UnsatisfiableFormula : std::invalid_argument {
  using std::invalid_argument::invalid_argument;
};

// Optional narration hook for the greedy loop (I6 / I8), called once per
// candidate z in the same lexicographic order the loop walks, with the
// determinacy witness (docs/GLOSSARY.md) when the candidate is rejected and
// nullopt when it is accepted.  Purely observational --- it cannot influence
// the result.
//
// Exists so a diagnostic caller (`ltlf-ek-deps --verbose`) can narrate the
// REAL search rather than re-deriving it: the witness is computed by the loop
// anyway, and without a way to observe it the CLI had to rebuild the DFA and
// re-run liveness/live-regions/greedy from a copy of this file's algorithm,
// which would silently drift from it on the next fix here.
using CandidateObserver = std::function<void(
    const std::string& z, bool accepted,
    const std::optional<std::string>& undetermined)>;

}  // namespace ltlf_ek
