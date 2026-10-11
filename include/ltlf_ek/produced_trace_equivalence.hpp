#pragma once

#include <optional>
#include <vector>

#include <bddx.h>
#include <spot/tl/formula.hh>
#include <spot/twa/twagraph.hh>

#include "ltlf_ek/role.hpp"
#include "ltlf_ek/transducer.hpp"
#include "ltlf_ek/variables.hpp"

namespace ltlf_ek {

// Produced-trace equivalence (docs/GLOSSARY.md "Produced-trace equivalence",
// docs/prd/engineered-domain-families.md "New library API --- Produced-trace
// equivalence").  The COMPLETE counterpart of the *Faithfulness guard*
// (docs/prd/oracle-faithfulness-guard.md), which only samples single-bit
// Sigma1 mutations (sound, not complete) --- this decides whether tau's
// *Produced-trace language* L(tau) and a declared psi denote the SAME
// LTLf language over NON-EMPTY words.
struct EquivalenceResult {
  bool equivalent_on_nonempty;
  bool empty_word_agrees;                        // reported, NEVER folded in
  std::optional<std::vector<bdd>> counterexample; // shortest non-empty witness
  unsigned tau_dfa_states;                        // |emits_dfa(tau)|
  unsigned psi_dfa_states;                        // |ltlf_to_dfa(psi)|
  unsigned product_states;                        // reachable pairs explored
  // Keeps the counterexample's AP variables registered on the shared
  // bdd_dict for as long as the result lives (Spot releases an automaton's
  // variables when it dies, after which they may be recycled).
  std::vector<spot::twa_graph_ptr> witness_owners;
};

// Decide L(tau) == L(psi) on non-empty words, by walking a synchronous
// product of emits_dfa(tau) and ltlf_to_dfa(psi) built on ONE shared
// spot::bdd_dict (tau.dict()).  A missing edge on tau's side is an implicit
// rejecting sink, never a skipped letter; psi's side is total by
// construction (ltlf_to_dfa is always complete), so a missing edge there is
// a precondition violation and throws std::logic_error instead.
// `equivalent_on_nonempty` is false
// iff some REACHABLE, non-initial product state pair differs in finality;
// `counterexample`, when set, is the SHORTEST such word, found by a
// breadth-first walk that visits letters in a fixed, deterministic order ---
// so it is reproducible, not "some" witness.  The empty word is excluded
// from the verdict and reported on its own field instead (see
// `empty_word_agrees`'s doc below) --- it must never be folded into
// `equivalent_on_nonempty`.
//
// The walk never enumerates an alphabet: at each product state it splits the
// letter space into the blocks on which BOTH automata's first-matching edge
// is constant (the intersections of their edge guards, plus tau's uncovered
// remainder going to the sink), so its cost follows the automata, not
// 2^|universe|, and a psi that strays outside `vars` is covered by
// construction.  Edge order fixes the visit order, so the witness is
// reproducible; each witness letter is a cube (bdd_satone) of the block.
//
// `role` is NOT defaulted, for the same reason the Faithfulness guard's
// isn't: a defaulted Role is exactly how a T_out pair silently gets checked
// under T_in slices and passes vacuously.  `vars` and `role` are part of the
// frozen signature but no longer shape the walk: the whole I union O
// universe is decided regardless of role, because a declared psi (e.g. the
// engineered families' A_N) is free to reference variables outside
// Sigma0 union Sigma1 (docs/prd/engineered-domain-families.md D5's guards
// read Ofree `mv` literals under Role::t_in).
//
// `empty_word_agrees` = (emits_dfa's initial state is final) ==
// (ltlf_to_dfa's initial state is final).  In practice this is ALWAYS
// false --- a length-0 run vacuously agrees with lambda, so emits_dfa's
// initial state is final by construction, while the repo's LTLf convention
// rejects the empty word unconditionally.  That is a mismatch between two
// encodings of "language", not a domain fact; never treat it as a red test.
//
// Degenerate inputs are legal and decided normally: psi = tt / ff, and a
// tau whose delta is nowhere defined (tau_dfa_states >= 1, an empty
// non-empty-word language, compared exactly like any other).
EquivalenceResult produced_trace_equivalent(const Transducer& tau,
                                            spot::formula psi,
                                            const VariablePartition& vars,
                                            Role role);

}  // namespace ltlf_ek
