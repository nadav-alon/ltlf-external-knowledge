#include "ltlf_ek/dependent_outputs.hpp"

#include <utility>

#include "ltlf_ek/detail/dependency_core.hpp"
#include "ltlf_ek/role.hpp"

namespace ltlf_ek {

// Thin delegation to the shared core (docs/prd/input-dependencies-tool.md
// Phase 1): the whole of the former inline implementation --- compute_live,
// compute_live_regions, the greedy loop, totalisation and the unsatisfiable
// check --- now lives once in detail::run_dependency_analysis, selected here
// by Role::t_out.  This function's own signature, and DependentOutputs'
// members, are unchanged so no existing call site or test is touched.
DependentOutputs dependent_outputs(const spot::formula& phi,
                                   const VariablePartition& partition,
                                   const spot::bdd_dict_ptr& dict,
                                   const CandidateObserver& on_candidate) {
  detail::DependencyResult core = detail::run_dependency_analysis(
      phi, partition, Role::t_out, dict, on_candidate);

  DependentOutputs result;
  result.dependent = std::move(core.dependent);
  result.partition = std::move(core.partition);
  result.t_out = std::move(core.transducer);
  return result;
}

}  // namespace ltlf_ek
