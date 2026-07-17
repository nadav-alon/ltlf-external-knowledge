#pragma once

#include <cstdint>
#include <unordered_map>
#include <vector>

#include <bddx.h>

// Phase 1 of docs/prd/mtnfa.md: the set-terminal substrate for MTNFA
// (docs/GLOSSARY.md "MTNFA").  Owns the interning table mapping a terminal
// index <-> a sorted, de-duplicated set of NFA-state indices (index 0 ==
// the empty set --- "Novel mechanisms (a)"), and the memoized UNION APPLY
// over MTBDDs whose leaves are such terminals ("Novel mechanisms (b)").
// Internal `detail` machinery (mona_dfa.hpp precedent); no glossary
// commitment on the exact members, only on the class name itself.
namespace ltlf_ek::detail {

class StateSetPool {
 public:
  // Interns {} (the empty set) as index 0 up front, per "Novel mechanisms
  // (a)": idx == 0 is always the empty set, so an uncovered letter can
  // always name it without a special case.
  StateSetPool();

  // Interns a sorted, de-duplicated state-set and returns its stable
  // terminal index.  Equal sets intern to the same index (canonical), so
  // interning twice is idempotent and physically-equal MTBDD terminals
  // compare `==`.
  unsigned intern(std::vector<unsigned> sorted_states);

  // The set named by `terminal_index`, as produced by intern() (or by 0 for
  // the empty set).
  const std::vector<unsigned>& set_of(int terminal_index) const;

  // The bespoke core ("Novel mechanisms (b)"): memoized MTBDD union apply.
  // `a`, `b` must be MTBDDs whose every leaf is bdd_terminalpp(idx) for an
  // idx interned in *this* pool (never bddfalse/bddtrue).  Returns the
  // MTBDD whose leaf on every letter is the union of a's and b's leaf-sets
  // there.  Uses Spot's C++ `bdd` type throughout, so every intermediate is
  // RAII-refcounted --- no manual bdd_addref/bdd_delref.
  bdd set_union(const bdd& a, const bdd& b);

  // Convenience used by the lift (nfa_to_mtnfa): the MTBDD
  // ite(guard, {state}, {}) --- terminal {state} where `guard` holds, the
  // empty-set terminal elsewhere.
  bdd guarded_singleton(const bdd& guard, unsigned state);

 private:
  struct VectorHash {
    std::size_t operator()(const std::vector<unsigned>& v) const noexcept;
  };

  // Recursive worker behind set_union().  `memo` is local to one top-level
  // set_union() call, keyed on the order-normalized {a.id(), b.id()} pair
  // (union is commutative + idempotent), giving behaviour polynomial in the
  // input MTBDDs' node counts rather than exponential in the letters.
  bdd UnionRec(const bdd& a, const bdd& b,
               std::unordered_map<std::uint64_t, bdd>& memo);

  // Canonical storage: sets_[i] is the set interned as index i; index_of_
  // inverts that so intern() is O(1) average and canonicalizing.
  std::vector<std::vector<unsigned>> sets_;
  std::unordered_map<std::vector<unsigned>, unsigned, VectorHash> index_of_;
};

}  // namespace ltlf_ek::detail
