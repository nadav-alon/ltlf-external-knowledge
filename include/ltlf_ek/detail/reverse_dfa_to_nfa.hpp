#pragma once

#include <spot/twa/twagraph.hh>

// P3 of docs/prd/ltlf-to-nfa.md ("Reverse" / alg:ltlftonfa:reverse, S160-169):
// the production DFA->NFA edge-reversal (glossary: "Automaton reversal
// (Reverse)"). Internal `detail` helper, not the public ltlf_to_nfa contract
// -- exposed via this header, rather than kept file-local, so it is directly
// unit-testable (same precedent as detail/past_ltlf_to_dfa.hpp).
//
// NOTE: tests/ltlfsynt_oracle_test.cpp has its own test-local
// ReverseAutomaton/DeterminizeReversed pair (the P2 checkpoint's independent
// oracle machinery, reversing the *reference* ltlf_to_dfa(phi) rather than
// past_ltlf_to_dfa's D). That duplication is deliberate (PRD "Developer
// comments" S363): reverse_dfa_to_nfa is the *production* reversal, with
// different invariants (no completion, the two purges below) that the test
// oracle intentionally does not replicate -- do not modify or "unify" that
// helper against this one.
namespace ltlf_ek::detail {

// Reverse(D) (main.tex S160-169, alg:ltlftonfa:reverse): builds the NFA N
// with L(N) = L(phi) from the DFA D = PastLtlfToDfa(mirror(phi)) with
// L(D) = rev(L(phi)).
//
// N keeps D's states, adds a fresh initial state s_{N,0}, and makes D's own
// initial state s_{D,0} the SOLE accepting state:
//   S_N = S_D u {s_{N,0}},  F_N = {s_{D,0}}
//   delta_N(s_{N,0}, v) = { s : delta_D(s,v) in F_D }
//   delta_N(t, v)       = { s : delta_D(s,v) = t }         (t in S_D)
// i.e. every D-edge s --v--> t becomes the N-edge t --v--> s, plus s_{N,0}'s
// out-edges to the v-predecessors of D's accepting states. State-based Buchi
// with the mark-on-out-edge convention (as ltlf_to_dfa / past_ltlf_to_dfa):
// an edge is marked iff its SOURCE state is s_{D,0}.
//
// N is deliberately left NONDETERMINISTIC and NOT completed (PRD invariant
// 3): alg:nfa_product tolerates an empty delta_N(s,v), so no rejecting sink
// is added.
//
// PRECONDITION beyond determinism + completeness: every state of D must be
// reachable from s_{D,0}. An unreachable state t with an edge t --v--> s_{D,0}
// gives s_{D,0} a real out-edge in N that leads nowhere useful, and the purges
// below can then drop s_{D,0} -- N's only accepting state -- leaving
// L(N) = {} instead of rev(L(D)). This holds of past_ltlf_to_dfa's D (Spot
// builds only reachable states), it predates the acceptance-helper adoption
// below, and it is NOT repaired by adding the defensive self-loop
// unconditionally: purge_dead_states erases an appended bddfalse self-loop as
// soon as the state has any other edge. See
// ReverseDfaToNfaSelfLoopEquivalence.* in tests/reverse_dfa_to_nfa_test.cpp.
//
// Accepting dead-end: s_{D,0} may end up with zero out-edges in N (whenever
// nothing in D transitions into D's own initial state). A defensive
// guard=bddfalse self-loop is added on s_{D,0} before purging so the
// accepting mark has an edge to live on; it is never traversable (bddfalse),
// so it cannot affect L(N). This mirrors spot::twa_graph::purge_dead_states's
// own documented exception for exactly this pattern ("self-loops ... can be
// used to store colors on state without successor with state-based
// acceptance", spot/twa/twagraph.cc) -- the self-loop survives purging iff
// it really is s_{D,0}'s only outgoing edge, and is otherwise pruned away
// like any other bddfalse edge. Since the acceptance-helper adoption
// (docs/prd/acceptance-mark-on-edgeless-states.md) it is added only in that
// surviving case; adding it unconditionally instead is provably the same
// graph, not merely the same language.
//
// Ends with purge_unreachable_states() then purge_dead_states() (PRD
// S207-209) to drop the dead/unreachable states reversing D's completeness
// leaves behind (the old reject sink's reversed edges, etc) -- an
// optimization, not a correctness requirement; given the reachability
// precondition above, the self-loop ensures this never drops the accepting
// state itself.
spot::twa_graph_ptr reverse_dfa_to_nfa(const spot::twa_graph_ptr& d);

}  // namespace ltlf_ek::detail
