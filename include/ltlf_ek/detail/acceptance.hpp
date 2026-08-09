#pragma once

#include <spot/twa/acc.hh>
#include <spot/twa/twagraph.hh>

// docs/prd/acceptance-mark-on-edgeless-states.md "Interfaces & types": the
// one named helper for the bddfalse-guarded-self-loop idiom, adopted at all
// four DFA/NFA-builder call sites that need a state's acceptance mark to
// survive a `state_is_accepting` read.  BuDDy/Spot representation mechanics,
// not a domain concept --- deliberately no docs/GLOSSARY.md entry (same
// policy as `include/ltlf_ek/transducer.hpp:44`'s `dict()`).
namespace ltlf_ek::detail {

// Spot carries state-based acceptance ON a state's out-edges, and
// twa_graph::state_is_accepting reads the mark off the state's FIRST out-edge.
// A state with zero out-edges therefore cannot carry a mark: the flag is
// silently read back as false.  Give such a state a bddfalse-guarded
// self-loop --- never taken by any real letter, so the language is unchanged,
// but it is an edge whose mark Spot can read.
//
// No-op when `g` already has at least one out-edge from `state`, and a no-op
// when `mark` is empty (a non-accepting edgeless state needs no carrier).
// Precondition: `g` uses state-based acceptance (prop_state_acc(true)).
void ensure_acceptance_readable(const spot::twa_graph_ptr& g, unsigned state,
                                spot::acc_cond::mark_t mark);

}  // namespace ltlf_ek::detail
