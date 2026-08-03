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

// Result of the maximally-input-dependent search (\cref{def:indep}).
struct DependentInputs {
  // Xdep: the maximally input-dependent set, lexicographic-greedy (I8).
  std::set<std::string> dependent;
  // `partition` with `dependent` moved input_free -> input_known, and
  // output_free / output_known passed through verbatim (I9).
  VariablePartition partition;
  // The Tin of \cref{lem:indep-transducer}: delta = the COMPLETE violation
  // automaton, lambda = the totalised projected live-letter region (I5, I7),
  // Role::t_in slices from `partition`.  nullopt iff `dependent` is empty ---
  // there is no Tin to build when Iknown is empty (see Edge cases).
  std::optional<OutputLabeledTransducer> t_in;
};

// Find a maximally input-dependent set of `phi` and materialise it as external
// knowledge.  Built on the shared `dict` (\cref{lem:indep-diagonal},
// \cref{lem:indep-transducer}; docs/prd/input-dependencies-tool.md).
//
// Throws std::invalid_argument if `partition.input_known` is non-empty (I9) or
// if an AP of `phi` lies outside `partition.universe()`, and
// UnsatisfiableFormula if `phi` is VALID (I11).
DependentInputs dependent_inputs(const spot::formula& phi,
                                 const VariablePartition& partition,
                                 const spot::bdd_dict_ptr& dict,
                                 const CandidateObserver& on_candidate = {});

}  // namespace ltlf_ek
