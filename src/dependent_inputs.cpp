#include "ltlf_ek/dependent_inputs.hpp"

#include <utility>

#include "ltlf_ek/detail/dependency_core.hpp"
#include "ltlf_ek/role.hpp"

namespace ltlf_ek {

// Thin delegation to the shared core (docs/prd/input-dependencies-tool.md
// Phase 1): I2's negation (analysing Not(phi) instead of phi) and I3's
// existential projection over O both live in detail::run_dependency_analysis,
// selected here by Role::t_in.
DependentInputs dependent_inputs(const spot::formula& phi,
                                 const VariablePartition& partition,
                                 const spot::bdd_dict_ptr& dict,
                                 const CandidateObserver& on_candidate) {
  detail::DependencyResult core = detail::run_dependency_analysis(
      phi, partition, Role::t_in, dict, on_candidate);

  DependentInputs result;
  result.dependent = std::move(core.dependent);
  result.partition = std::move(core.partition);
  result.t_in = std::move(core.transducer);
  return result;
}

}  // namespace ltlf_ek
