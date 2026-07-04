#include "ltlf_ek/solve_dfa.hpp"

#include <set>
#include <stdexcept>
#include <string>

#include <bddx.h>
#include <spot/twa/twagraph.hh>
#include <spot/twaalgos/game.hh>
#include <spot/twaalgos/synthesis.hh>

namespace ltlf_ek {
namespace {

// Positive-literal variable cube of `names`, resolved on the product's dict
// (register_ap is idempotent: an already-declared AP keeps its variable).
bdd cube_of(const std::set<std::string>& names,
            const spot::twa_graph_ptr& aut) {
  bdd cube = bddtrue;
  for (const auto& n : names) cube &= bdd_ithvar(aut->register_ap(n));
  return cube;
}

}  // namespace

std::optional<Controller> solve_dfa(const spot::twa_graph_ptr& product,
                                    const VariablePartition& vars) {
  const spot::bdd_dict_ptr dict = product->get_dict();

  // Ofree = synthesis outputs (system); Iknown ∪ Oknown = the pinned governed
  // variables projected out of the arena (lem:sink_skip / §fulldfa \cl note).
  const bdd ofree_cube = cube_of(vars.output_free, product);
  const bdd known_cube = cube_of(vars.known(), product);

  // The product builder records kSink here; without it solve_dfa cannot drop the
  // ⊥-edges and would silently re-admit the sink into the arena, so fail loudly.
  const unsigned* sink = product->get_named_prop<unsigned>(kSinkProperty);
  if (!sink)
    throw std::invalid_argument(
        "solve_dfa: product is missing the \"" + std::string(kSinkProperty) +
        "\" named property (the kSink state index recorded by the DfaProduct "
        "product construction)");

  // Rebuild the product as the free-only game arena: project the pinned Iknown,
  // Oknown out of every guard, drop the kSink transitions (unreachable in real
  // play, lem:sink_skip), and turn the F_P-reachability objective into a Büchi
  // one by making every final state an absorbing accepting self-loop.
  spot::twa_graph_ptr game = spot::make_twa_graph(dict);
  // The arena's alphabet is the free variables only; everything not an output
  // (i.e. Ifree) is an environment input for split_2step.
  for (const auto& n : vars.input_free) game->register_ap(n);
  for (const auto& n : vars.output_free) game->register_ap(n);
  game->set_buchi();
  game->prop_state_acc(true);

  const unsigned n = product->num_states();
  game->new_states(n);
  game->set_init_state(product->get_init_state_number());

  const spot::acc_cond::mark_t kFinal = {0};
  const spot::acc_cond::mark_t kNone = {};

  for (unsigned st = 0; st < n; ++st) {
    if (product->state_is_accepting(st)) {
      // Reachability target: reaching a final state wins, so absorb into an
      // accepting self-loop (reachability -> Büchi reduction).
      game->new_edge(st, st, bddtrue, kFinal);
      continue;
    }
    if (st == *sink) {
      game->new_edge(st, st, bddtrue, kNone);  // kept, but unreachable in play.
      continue;
    }
    for (const auto& e : product->out(st)) {
      if (e.dst == *sink) continue;  // drop non-cons -> sink (lem:sink_skip).
      const bdd guard = bdd_exist(e.cond, known_cube);
      if (guard != bddfalse) game->new_edge(st, e.dst, guard, kNone);
    }
  }

  // Turn-ordered game over Ifree (env, plays first) vs Ofree (system), then
  // solve reachability-as-Büchi and read off the strategy.
  spot::set_synthesis_outputs(game, ofree_cube);
  spot::twa_graph_ptr arena = spot::split_2step(game, /*complete_env=*/true);
  if (!spot::solve_game(arena)) return std::nullopt;  // unrealizable.

  // solved_game_to_mealy returns the strategy graph Spot calls a "Mealy
  // machine"; our domain term is Controller (glossary).  Its edges mention only
  // Ifree/Ofree, so it already ignores the redundant Iknown bits --- i.e. it is
  // the lift to the 2^{I} interface of def:probDefTransducer.
  Controller controller;
  controller.strategy = spot::solved_game_to_mealy(arena);
  return controller;
}

}  // namespace ltlf_ek
