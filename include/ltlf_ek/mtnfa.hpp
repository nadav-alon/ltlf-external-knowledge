#pragma once

#include <vector>

#include <spot/tl/formula.hh>
#include <spot/twa/bdddict.hh>
#include <spot/twa/twagraph.hh>
#include <spot/twaalgos/ltlf2dfa.hh>

#include "ltlf_ek/detail/state_set_pool.hpp"

// Phase 2 of docs/prd/mtnfa.md: the MTNFA representation
// (docs/GLOSSARY.md "MTNFA") --- the nondeterministic sibling of
// spot::mtdfa.  One MTBDD per NFA state over the letter alphabet
// 2^{I u O}; terminals name SETS of successor states via `pool`
// (detail::StateSetPool, Phase 1).  Built on the SAME spot::bdd_dict as
// the transducers and every letter.
namespace ltlf_ek {

struct Mtnfa {
  std::vector<bdd> states;              // states[s] : MTBDD, set-valued terminals
  std::vector<bool> accepting;          // accepting[s] == (s in F_N)
  unsigned initial = 0;                 // index of s_{N,0}
  // `pool` interprets the terminals; OWNED.  `mutable`: mtnfa_to_mtdfa takes
  // `const Mtnfa&` (frozen signature) but must intern newly-unioned
  // successor sets into THIS pool as it determinizes (the terminals in
  // `states` are only meaningful relative to it) --- logical constness, the
  // classic justification for `mutable` on an owned cache/interning table.
  // Recorded in "Developer comments / PRD disagreements" below.
  mutable detail::StateSetPool pool;
  spot::bdd_dict_ptr dict;
  std::vector<spot::formula> aps;       // APs registered (phi's support on dict)

  // NOT part of the PRD's frozen field list; forced addition, see
  // "Developer comments / PRD disagreements" in docs/prd/mtnfa.md.  `states`'
  // BDDs reference AP variables that were registered on `dict` by the
  // *source* twa_graph (nfa_to_mtnfa's argument), following the
  // OutputLabeledTransducer::delta_dfa_ pattern
  // (output_labeled_transducer.hpp): keeping a
  // shared_ptr to that graph alive for as long as this Mtnfa is alive is
  // what keeps those variables reserved (spot::bdd_dict unregisters an
  // owner's variables when the LAST reference to that owner dies).  Without
  // this, an Mtnfa built from a temporary twa_graph_ptr (e.g. inside
  // ltlf_to_mtnfa's `nfa_to_mtnfa(ltlf_to_nfa(phi, dict))`) would outlive
  // its own AP registration, and a later dict->register_ap by an unrelated
  // caller (e.g. spot::ltlf_to_mtdfa building the oracle afterward) could
  // silently reuse/alias those variable numbers --- observed empirically as
  // a spurious product_xor mismatch.
  spot::twa_graph_ptr source_nfa;
};

// Lift an explicit deterministic-or-not twa_graph NFA into an Mtnfa.  For
// each state s: fold every out-edge (cond, dst) via pool.set_union of
// pool.guarded_singleton(cond, dst) --- overlapping guards MERGE (that
// overlap is nondeterminism, main.tex delta_N : S_N x 2^{I u O} -> 2^{S_N},
// main.tex:198); an uncovered letter (or a state with no out-edges) stays
// the empty-set terminal (index 0).  accepting[s] = nfa->state_is_accepting(s);
// initial = nfa->get_init_state_number().  aps = nfa->ap() (sorted by
// formula id, mirroring spot::mtdfa's `aps` convention).  No sink state is
// added: N is legitimately partial.  The NFA analog of
// spot::twadfa_to_mtdfa (ours; Spot has no twanfa_to_mtnfa).
Mtnfa nfa_to_mtnfa(const spot::twa_graph_ptr& nfa);

// LtlfToNfa in the mtdfa representation: ltlf_to_nfa(phi, dict) then
// nfa_to_mtnfa.  Same (phi, dict) shape + shared-dict precondition as
// ltlf_to_nfa; APs come from phi's support (registered on the intermediate
// twa_graph by ltlf_to_nfa).  MONA-backed (via ltlf_to_nfa) --- runtime dep
// on `mona`.
Mtnfa ltlf_to_mtnfa(const spot::formula& phi, const spot::bdd_dict_ptr& dict);

// NfaToDfa (alg:nfa_product:determinize) at the mtdfa representation,
// applied to the Goal NFA alone: symbolic subset construction (BFS over
// reachable subsets R subseteq S_N).  Returns a spot::mtdfa (states =
// reachable subsets, states[0] = {nfa.initial}, terminal 2*j+b where
// b = any state in the subset j is accepting, bddfalse = the
// empty-set/rejecting sink) recognizing L(nfa).  nullptr is NEVER
// returned; an NFA whose language is empty yields the single
// rejecting-sink mtdfa.
spot::mtdfa_ptr mtnfa_to_mtdfa(const Mtnfa& nfa);

}  // namespace ltlf_ek
