#pragma once

#include <optional>
#include <set>
#include <string>

#include <spot/tl/formula.hh>
#include <spot/twa/bdddict.hh>

#include "ltlf_ek/dependency_types.hpp"
#include "ltlf_ek/output_labeled_transducer.hpp"
#include "ltlf_ek/variables.hpp"

namespace ltlf_ek {

// UnsatisfiableFormula and CandidateObserver now live in dependency_types.hpp
// (the shared core header both dependent_outputs and dependent_inputs use);
// included above so this header's own public surface --- ltlf_ek::
// UnsatisfiableFormula, ltlf_ek::CandidateObserver --- is unchanged for every
// existing call site.

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
