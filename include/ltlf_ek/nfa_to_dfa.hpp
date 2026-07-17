#pragma once

#include <spot/twa/twagraph.hh>

namespace ltlf_ek {

// NfaToDfa(N) --- main.tex alg:nfa_product:determinize, black-boxed there as
// \algname{NfaToDfa} (docs/GLOSSARY.md "Goal automaton determinization",
// EXPLICIT representation).
//
// A GENERIC explicit subset construction, twa_graph NFA -> twa_graph DFA:
// partition- and transducer-agnostic, it enumerates full minterms over `nfa`'s
// OWN registered APs (nfa->ap()) and knows nothing of I/O/Ifree/Iknown/etc.
// A DFA state is a subset R of nfa's state ids; R0 = {nfa's init state}, at
// output state id 0 (so states()[0] is the initial state, as solve_dfa
// expects); delta_D(R, v) = union over s in R of nfa's delta(s, v).  The empty
// subset is SKIPPED --- no edge is emitted for it (a missing edge = reject;
// this is an incomplete output, it grows no sink of its own).  A subset R is
// accepting iff some s in R has nfa->state_is_accepting(s).  Output is
// state-based Buchi on nfa's own bdd_dict (the abused-DBA convention
// ltlf_to_dfa / mtdfa::as_twa use, so solve_dfa and the isolated determinize
// oracle read this the same way as ltlf_to_dfa's A).  Reachable-subset BFS
// from {init}, sorted+de-duplicated subset keys, no seed/randomness.
//
// This function does NOT complete `nfa` first --- that is the caller's job
// (NfaProduct completes N via spot::complete_here before building the product
// it feeds in here; the isolated determinize oracle calls nfa_to_dfa directly
// on the un-completed ltlf_to_nfa output, per docs/prd/nfa-product.md Phase 1).
//
// Never returns nullptr: phi=0 (ff) yields a single non-accepting initial
// state with no outgoing edges.
spot::twa_graph_ptr nfa_to_dfa(const spot::twa_graph_ptr& nfa);

}  // namespace ltlf_ek
