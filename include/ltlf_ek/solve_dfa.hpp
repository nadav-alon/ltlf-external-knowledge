#pragma once

#include <optional>

#include <spot/twa/twagraph.hh>

#include "ltlf_ek/synthesis.hpp"
#include "ltlf_ek/variables.hpp"

namespace ltlf_ek {

// Named property under which the product builder records the reserved kSink
// state index on the product twa_graph, so solve_dfa can drop its ⊥-edges
// (lem:sink_skip).  This is the plumbing contract between the DfaProduct product
// construction and solve_dfa --- an infrastructure key, not a domain term.
inline constexpr char kSinkProperty[] = "ltlf-ek-sink";

// SolveDfa(P) --- main.tex Method 2 (alg:dfa_product, line
// alg:dfa_product:solve), black-boxed there as \algname{SolveDfa}.
//
// Solve the product game `product` --- the system tries to *reach* a final
// product state F_P --- and extract the Controller, or report unrealizable
// (nullopt).  `product` is the explicit state-based-Büchi twa_graph built by
// DfaProduct: accepting states are F_P, and the sink kSink is carried in the
// named property "ltlf-ek-sink".  `vars` supplies the free/known split.
//
// Per the pinning of lem:sink_skip and the \cl note in main.tex §fulldfa, only
// the *free* moves are played: the governed variables Iknown, Oknown are pinned
// by the transducers, so solve_dfa existentially projects them out of the
// product guards, drops the (unreachable, lem:sink_skip) kSink transitions, and
// solves the game over Ifree (environment) vs Ofree (system).  The resulting
// Ifree-controller already ignores the redundant Iknown bits, so it *is* the
// lifted 2^{I}-interface controller of def:probDefTransducer (a faithful T_C).
//
// Thin wrapper over Spot's synthesis pipeline: set_synthesis_outputs (Ofree) ->
// split_2step -> solve_game -> solved_game_to_mealy.  nullopt = unrealizable.
std::optional<Controller> solve_dfa(const spot::twa_graph_ptr& product,
                                    const VariablePartition& vars);

}  // namespace ltlf_ek
