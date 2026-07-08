#include "ltlf_ek/synthesis.hpp"

#include <utility>
#include <vector>

#include <bddx.h>
#include <spot/twaalgos/synthesis.hh>

#include "ltlf_ek/detail/util.hpp"
#include "ltlf_ek/role.hpp"

namespace ltlf_ek {

OutputLabeledTransducer controller_as_transducer(const Controller& controller,
                                                 const VariablePartition& vars) {
  // controller.strategy (solved_game_to_mealy) is a SPLIT/alternating arena
  // --- an env-player node's out-edges are guarded by Ifree alone, leading to
  // a sys-player node whose out-edges are guarded by Ofree alone (Spot's
  // "state-player" named property; confirmed by unsplit_2step's own
  // precondition).  unsplit_2step collapses each Ifree-then-Ofree hop into
  // one edge guarded by their conjunction ("ins&outs", synthesis.hh), leaving
  // only the (real) Q_C states --- exactly the one-edge-per-transition shape
  // OutputLabeledTransducer expects.
  const spot::twa_graph_ptr g = spot::unsplit_2step(controller.strategy);

  const SigmaSlices slices = sigma_slices(vars, Role::t_c);
  const bdd sigma0_cube = detail::cube_of(slices.sigma0, g);
  const bdd sigma1_cube = detail::cube_of(slices.sigma1, g);

  // lambda_C(q, ...) --- the union of q's out-edge guards is exactly the
  // relation over Ifree x Ofree the Mealy strategy commits to at q (the
  // "delta via edges, output derived" idiom, docs/GLOSSARY.md
  // "Controller-as-transducer view"); a state with no out-edges commits to
  // bddfalse (undefined lambda_C there, mirrored by an equally undefined
  // delta_C --- see OutputLabeledTransducer::delta/::lambda).
  const unsigned n_states = g->num_states();
  std::vector<bdd> lambda_by_state(n_states, bddfalse);
  for (unsigned q = 0; q < n_states; ++q) {
    bdd out = bddfalse;
    for (const auto& e : g->out(q)) out |= e.cond;
    lambda_by_state[q] = out;
  }

  return OutputLabeledTransducer(g, std::move(lambda_by_state), sigma0_cube,
                                 sigma1_cube);
}

}  // namespace ltlf_ek
