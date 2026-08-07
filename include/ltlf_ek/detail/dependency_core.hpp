#pragma once

#include <optional>
#include <set>

#include <spot/tl/formula.hh>
#include <spot/twa/bdddict.hh>

#include "ltlf_ek/dependency_types.hpp"
#include "ltlf_ek/output_labeled_transducer.hpp"
#include "ltlf_ek/role.hpp"
#include "ltlf_ek/variables.hpp"

// Internal shared core for the dependency-extraction tools --- not a domain
// concept of its own (same policy as bench.hpp / cli.hpp: no glossary entry).
// See docs/prd/output-dependencies-tool.md and
// docs/prd/input-dependencies-tool.md.
namespace ltlf_ek::detail {

// The direction-neutral result; the public structs (DependentOutputs,
// DependentInputs) are thin renames of it.
struct DependencyResult {
  std::set<std::string> dependent;
  VariablePartition partition;
  std::optional<OutputLabeledTransducer> transducer;
};

// The whole of \cref{lem:outdep-diagonal} / \cref{lem:indep-diagonal} and their
// transducer lemmas, once.  `role` alone selects all four direction-dependent
// axes, and they are NOT independent knobs --- a struct of four booleans would
// make invalid combinations representable, so the core derives them:
//
//   role      analysed formula   scanned set        projected   emitted Role
//   t_out     phi                partition.output_free   no     t_out
//   t_in      Not(phi)           partition.input_free    yes     t_in
//
// Throws std::invalid_argument if the role's own known-set is non-empty on
// input (I9) or an AP of `phi` lies outside partition.universe(); throws
// UnsatisfiableFormula if the analysed automaton's initial state is not live
// (I11).  Role::t_c throws std::invalid_argument.
DependencyResult run_dependency_analysis(
    const spot::formula& phi, const VariablePartition& partition, Role role,
    const spot::bdd_dict_ptr& dict, const CandidateObserver& on_candidate);

}  // namespace ltlf_ek::detail
