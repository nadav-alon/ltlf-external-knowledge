#pragma once

#include <optional>

#include <spot/twa/twagraph.hh>

#include "ltlf_ek/synthesis.hpp"
#include "ltlf_ek/variables.hpp"

namespace ltlf_ek {

// SolveDfa(P) --- main.tex Method 2 (alg:dfa_product, line
// alg:dfa_product:solve), black-boxed there as \algname{SolveDfa}.
//
// Solve the product game `product` --- the system tries to *reach* a final
// product state F_P --- and extract the Controller, or report unrealizable
// (nullopt).  `product` is the explicit state-based-Büchi twa_graph built by
// DfaProduct: accepting states are F_P.  `vars` supplies the free/known split.
//
// Only the *free* moves are played: the governed variables Iknown, Oknown are
// pinned by the transducers, so solve_dfa existentially projects them out of
// the product guards and solves the game over Ifree (environment) vs Ofree
// (system) --- non-cons letters contribute no product transition in the first
// place (def:enabled, skipped as in Methods 1/3), so there is nothing to drop
// here.  The resulting Ifree-controller already ignores the redundant Iknown
// bits, so it *is* the lifted 2^{I}-interface controller of
// def:probDefTransducer (a faithful T_C).
//
// Thin wrapper over Spot's synthesis pipeline: set_synthesis_outputs (Ofree) ->
// split_2step -> solve_game -> solved_game_to_mealy.  nullopt = unrealizable.
std::optional<Controller> solve_dfa(const spot::twa_graph_ptr& product,
                                    const VariablePartition& vars);

}  // namespace ltlf_ek
