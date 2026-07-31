#pragma once

#include <functional>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>

#include <spot/tl/formula.hh>
#include <spot/twa/bdddict.hh>

#include "ltlf_ek/output_labeled_transducer.hpp"
#include "ltlf_ek/variables.hpp"

namespace ltlf_ek {

// Thrown by dependent_outputs when L(phi) is empty (see Edge cases in
// docs/prd/output-dependencies-tool.md): every Xdep is then VACUOUSLY
// dependent, so the greedy loop would otherwise confidently return Xdep = O.
//
// A distinct TYPE, not a distinguishing message.  dependent_outputs throws
// std::invalid_argument for three unrelated reasons and only this one earns
// its own CLI exit code; dispatching on a what() substring instead misfires
// whenever another message happens to contain the word --- an AP literally
// named `unsatisfiable` made the closed-universe refusal exit 3 --- and
// regresses silently if the wording is ever reworded.  Derives from
// std::invalid_argument so callers catching the base still catch this.
struct UnsatisfiableFormula : std::invalid_argument {
  using std::invalid_argument::invalid_argument;
};

// Optional narration hook for I6's greedy loop, called once per z in O in the
// same lexicographic order the loop walks, with the determinacy witness
// (docs/GLOSSARY.md) when the candidate is rejected and nullopt when it is
// accepted.  Purely observational --- it cannot influence the result.
//
// Exists so a diagnostic caller (`ltlf-ek-deps --verbose`) can narrate the
// REAL search rather than re-deriving it: the witness is computed by the loop
// anyway, and without a way to observe it the CLI had to rebuild the DFA and
// re-run liveness/live-regions/greedy from a copy of this file's algorithm,
// which would silently drift from it on the next fix here.
using CandidateObserver = std::function<void(
    const std::string& z, bool accepted,
    const std::optional<std::string>& undetermined)>;

// Result of the maximally-dependent-output search (\cref{def:outdep}).
struct DependentOutputs {
  // Xdep: the maximally dependent output set, lexicographic-greedy (I6).
  std::set<std::string> dependent;
  // `partition` with `dependent` moved output_free -> output_known, and
  // input_free / input_known passed through verbatim (I9).
  VariablePartition partition;
  // The Tout of \cref{lem:outdep-transducer}: delta = the COMPLETE Goal DFA,
  // lambda = the totalised live-letter region (I3, I5), Role::t_out slices from
  // `partition`.  nullopt iff `dependent` is empty --- there is no Tout to
  // build when Oknown is empty (see Edge cases).
  std::optional<OutputLabeledTransducer> t_out;
};

// Find a maximally dependent output set of `phi` and materialise it as external
// knowledge.  Built on the shared `dict` (\cref{lem:outdep-diagonal},
// \cref{lem:outdep-transducer}; docs/prd/output-dependencies-tool.md).
//
// Throws std::invalid_argument if `partition.output_known` is non-empty (I9)
// or if an AP of `phi` lies outside `partition.universe()` (the closed-universe
// rule), and the derived UnsatisfiableFormula if `phi` is unsatisfiable
// (Edge cases).
//
// `on_candidate`, when set, observes each greedy step; see CandidateObserver.
DependentOutputs dependent_outputs(const spot::formula& phi,
                                   const VariablePartition& partition,
                                   const spot::bdd_dict_ptr& dict,
                                   const CandidateObserver& on_candidate = {});

}  // namespace ltlf_ek
