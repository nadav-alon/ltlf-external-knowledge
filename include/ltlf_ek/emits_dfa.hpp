#pragma once

#include <spot/twa/bdddict.hh>
#include <spot/twa/twagraph.hh>

#include "ltlf_ek/transducer.hpp"

namespace ltlf_ek {

// The Output-agreement automaton for ONE transducer (docs/GLOSSARY.md "Output
// agreement (emits)", automaton form): the DFA accepting exactly those words
// whose every letter agrees with tau's lambda along tau's run.  This is the
// automaton form of emits_region --- NOT of cons, which is the two-transducer
// conjunction and has no automaton form (\cref{def:consistency} §203); the cons
// filter emerges from intersecting the two, see docs/prd/mtdfa-product.md
// "Decision 1".  Built on `dict` --- the SAME spot::bdd_dict as tau and the
// Goal automaton.
//
// Language: L(emits_dfa(tau)) = { w : tau's run on w is defined and every
// letter agrees with lambda at its state } (docs/prd/mtdfa-product.md
// "emits_dfa --- pinned specification").
//
// States: one per state reachable from tau.initial_state() via delta_edges.
// Transducer exposes no state count, so reachability (BFS from the initial
// state) is how this build discovers tau's state set --- recorded as a
// "Developer comments / PRD disagreements" entry in docs/prd/mtdfa-product.md
// (the PRD's "q in [0, num_states)" phrasing assumes an accessor the frozen
// Transducer interface does not have).
//
// NO rejecting sink: the automaton is deterministic but deliberately
// INCOMPLETE --- a letter not covered by any edge is a missing edge, an
// implicit reject, so the language above is unchanged.  A materialised
// rejecting sink used to be built here; it was removed as wasted work
// (Phase 0/Q1), and because main.tex SKIPS non-enabled letters --- which an
// incomplete automaton does literally.
//
// Phase 0/Q1: marks acceptance STATE-BASED and calls prop_state_acc(true)
// explicitly --- twadfa_to_mtdfa branches on that property and would otherwise
// read the marks as final transitions (ltlf2dfa.cc:3001).  Also genuinely
// deterministic by construction: twadfa_to_mtdfa THROWS otherwise
// (ltlf2dfa.cc:2958).
spot::twa_graph_ptr emits_dfa(const Transducer& tau,
                              const spot::bdd_dict_ptr& dict);

}  // namespace ltlf_ek
