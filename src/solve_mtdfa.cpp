#include "ltlf_ek/solve_mtdfa.hpp"

#include <set>
#include <string>

#include <bddx.h>
#include <spot/tl/formula.hh>
#include <spot/twaalgos/synthesis.hh>

#include "ltlf_ek/turn_order.hpp"

namespace ltlf_ek {

namespace {

// Variable-cube of `names`, resolved purely by LOOKUP on `dict` (bdd_ithvar +
// dict->varnum) --- never registers.  Safe only once require_turn_order_aps
// has already confirmed every name is registered (step 0 below); unlike
// detail::cube_of (which registers, for a twa_graph registrar) this needs no
// registrar of its own, since it must never touch AP ownership.
bdd cube_of(const spot::bdd_dict_ptr& dict, const std::set<std::string>& names) {
  bdd cube = bddtrue;
  for (const auto& n : names) cube &= bdd_ithvar(dict->varnum(spot::formula::ap(n)));
  return cube;
}

}  // namespace

std::optional<Controller> solve_mtdfa(const spot::mtdfa_ptr& product,
                                      const VariablePartition& vars) {
  const spot::bdd_dict_ptr dict = product->get_dict();

  // Step 0 (Phase 0/Q2): fail loudly on a bad AP order rather than silently
  // returning a wrong "unrealizable".
  require_turn_order_aps(vars, dict);

  // Step 1 (decision 2): Ofree u Iknown u Oknown are ALL controllable --- the
  // pinned variables are forced system moves, not a free environment choice.
  std::set<std::string> controllable = vars.output_free;
  const std::set<std::string> known = vars.known();
  controllable.insert(known.begin(), known.end());
  product->set_controllable_variables(cube_of(dict, controllable));

  // Step 2: the linear-time (backprop_nodes) route --- this PRD's whole
  // claim is cost.
  const spot::mtdfa_ptr strategy = spot::mtdfa_winning_strategy(
      product, /*backprop_nodes=*/true);

  // Step 3 (Phase 0/Q3): the pinned unrealizable test --- NOT num_roots()==0
  // alone, which never happens.  Touch nothing else on `strategy`: Spot
  // leaves it without registered APs or controllable vars on this path.
  if (strategy->num_roots() == 0 || strategy->states[0] == bddfalse)
    return std::nullopt;

  // Step 4 (Phase 0/Q4): loop=true keeps lambda_C functional (loop=false
  // leaves a free-choice bddtrue self-loop once phi is fulfilled, making
  // lambda_C a relation).  labels=false: the LTLf state names are never
  // read.
  spot::twa_graph_ptr mealy = spot::mtdfa_strategy_to_mealy(
      strategy, /*labels=*/false, /*loop=*/true);

  // Step 5: project Iknown, Oknown out of every edge (src/solve_dfa.cpp:49
  // idiom, one stage later).  A guard collapsing to bddfalse is left in
  // place --- functionally absent, matching this codebase's
  // bddfalse-is-undefined idiom (Cube / Output agreement) throughout.
  const bdd known_cube = cube_of(dict, known);
  const unsigned n_states = mealy->num_states();
  for (unsigned s = 0; s < n_states; ++s)
    for (auto& e : mealy->out(s)) e.cond = bdd_exist(e.cond, known_cube);

  // Step 6: this mealy is ALREADY unsplit (no "state-player" property) ---
  // unlike solve_dfa's split arena.  controller_as_transducer is taught to
  // unsplit only when that property is present (Phase 0/Q4 follow-up).
  spot::set_synthesis_outputs(mealy, cube_of(dict, vars.output_free));

  Controller controller;
  controller.strategy = mealy;
  return controller;
}

}  // namespace ltlf_ek
