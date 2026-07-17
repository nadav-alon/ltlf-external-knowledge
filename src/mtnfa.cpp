#include "ltlf_ek/mtnfa.hpp"

#include <algorithm>
#include <cassert>
#include <deque>
#include <map>
#include <unordered_map>

#include "ltlf_ek/ltlf_to_nfa.hpp"

namespace ltlf_ek {

namespace {

// Sort nfa->ap() by formula id, mirroring spot::mtdfa's `aps` convention
// ("This vector is sorted by formula ID, to make it easy to merge with
// another sorted vector.", ltlf2dfa.hh).
std::vector<spot::formula> SortedAps(const spot::twa_graph_ptr& nfa) {
  std::vector<spot::formula> aps(nfa->ap().begin(), nfa->ap().end());
  std::sort(aps.begin(), aps.end());
  return aps;
}

// The relabeling recursion behind mtnfa_to_mtdfa ("Novel mechanisms (c)"):
// a unary, memoized MTBDD map from a StateSetPool-terminated `rowSet` MTBDD
// (successor subsets, from set_union) to a spot::mtdfa row (leaves are
// bddfalse or bdd_terminalpp(2*j+b)).  `subset_index` / `pending` are the
// BFS bookkeeping shared across the whole determinization (a
// map<vector<unsigned>, unsigned> DISTINCT from the StateSetPool); `memo`
// is local to one top-level call, mirroring StateSetPool::set_union's
// per-call memo scope (docs/prd/mtnfa.md "Developer comments" on Phase 1).
bdd RelabelRec(const bdd& node, const Mtnfa& nfa,
               std::map<std::vector<unsigned>, unsigned>& subset_index,
               std::deque<std::vector<unsigned>>& pending,
               std::unordered_map<int, bdd>& memo) {
  if (auto it = memo.find(node.id()); it != memo.end()) return it->second;

  bdd result;
  if (bdd_is_terminal(node)) {
    const std::vector<unsigned>& members = nfa.pool.set_of(bdd_get_terminal(node));
    if (members.empty()) {
      result = bddfalse;  // empty successor subset: the rejecting sink
    } else {
      unsigned j;
      if (auto found = subset_index.find(members); found != subset_index.end()) {
        j = found->second;
      } else {
        j = static_cast<unsigned>(subset_index.size());
        subset_index.emplace(members, j);
        pending.push_back(members);
      }
      const bool b = std::any_of(members.begin(), members.end(),
                                  [&](unsigned s) { return nfa.accepting[s]; });
      result = bdd_terminalpp(static_cast<int>(2 * j + (b ? 1 : 0)));
    }
  } else {
    const int v = bdd_var(node);
    result = bdd_ite(bdd_ithvarpp(v),
                     RelabelRec(bdd_high(node), nfa, subset_index, pending, memo),
                     RelabelRec(bdd_low(node), nfa, subset_index, pending, memo));
  }
  memo.emplace(node.id(), result);
  return result;
}

}  // namespace

Mtnfa nfa_to_mtnfa(const spot::twa_graph_ptr& nfa) {
  Mtnfa result;
  result.dict = nfa->get_dict();
  result.aps = SortedAps(nfa);

  const unsigned n = nfa->num_states();
  result.states.reserve(n);
  result.accepting.reserve(n);
  for (unsigned s = 0; s < n; ++s) {
    // Fold every out-edge via set_union of guarded_singleton; the identity
    // (empty-set terminal) both seeds the fold and is what a state with no
    // out-edges --- or an uncovered letter --- keeps ("Interfaces & types",
    // "Edge cases": no sink state, N stays partial).
    bdd row = bdd_terminalpp(0);
    for (const auto& e : nfa->out(s))
      row = result.pool.set_union(row, result.pool.guarded_singleton(e.cond, e.dst));
    result.states.push_back(row);
    result.accepting.push_back(nfa->state_is_accepting(s));
  }
  result.initial = nfa->get_init_state_number();
  // Keep `nfa` alive for as long as `result` is (see mtnfa.hpp's
  // `source_nfa` comment): `states`' BDDs reference AP variables `nfa`
  // registered on `dict`, and the dict frees an owner's variables when the
  // owner's last reference dies.
  result.source_nfa = nfa;
  return result;
}

Mtnfa ltlf_to_mtnfa(const spot::formula& phi, const spot::bdd_dict_ptr& dict) {
  return nfa_to_mtnfa(ltlf_to_nfa(phi, dict));
}

spot::mtdfa_ptr mtnfa_to_mtdfa(const Mtnfa& nfa) {
  // Precondition (theory-review F2): the initial state must NOT be accepting.
  // R0 = {nfa.initial} is seeded directly at output index 0 and is never
  // discovered as a *destination* subset, so RelabelRec never reads its
  // acceptance bit --- an accepting initial state (i.e. an NFA that accepts
  // the empty word) would be silently dropped.  ltlf_to_mtnfa always satisfies
  // this: ltlf_to_nfa gives a fresh non-accepting s_{N,0}.  Callers lifting an
  // arbitrary twa_graph must ensure it too (or split the initial state first).
  assert(!nfa.accepting[nfa.initial] &&
         "mtnfa_to_mtdfa: accepting initial state unsupported (see F2)");

  auto out = std::make_shared<spot::mtdfa>(nfa.dict);
  out->aps = nfa.aps;

  // The returned mtdfa must own its OWN registration stake in `dict`.  Its
  // rows are built by bdd_ite over the AP variable numbers `nfa.states` uses,
  // but nothing else ties those numbers to `out`'s lifetime: Mtnfa::source_nfa
  // keeps them alive only while the *Mtnfa* lives, so the natural calling
  // pattern `mtnfa_to_mtdfa(ltlf_to_mtnfa(phi, dict))` --- which discards the
  // Mtnfa temporary and keeps only this mtdfa --- would leave `out` holding
  // BDDs over variable numbers `dict` is free to recycle on the next
  // register_ap (e.g. by spot::ltlf_to_mtdfa).  Observed as a spurious
  // non-empty product_xor.  register_proposition returns the ALREADY-assigned
  // variable number and just adds `out.get()` to that variable's owner list,
  // so the numbers already baked into the rows below stay valid; the pairing
  // unregister is spot::mtdfa's own destructor
  // (`dict_->unregister_all_my_variables(this)`, ltlf2dfa.hh:130).  This is
  // the same ownership contract spot::twadfa_to_mtdfa honours.
  for (const spot::formula& ap : out->aps)
    nfa.dict->register_proposition(ap, out.get());

  // BFS over reachable subsets ("Novel mechanisms (c)"): R0 = {nfa.initial}
  // seeded at output index 0, so states[0] is the mtdfa's initial state, as
  // solve_mtdfa / Spot expect.
  std::map<std::vector<unsigned>, unsigned> subset_index;
  std::deque<std::vector<unsigned>> pending;
  const std::vector<unsigned> r0{nfa.initial};
  subset_index.emplace(r0, 0u);
  pending.push_back(r0);

  while (!pending.empty()) {
    const std::vector<unsigned> R = std::move(pending.front());
    pending.pop_front();

    // rowSet = fold(set_union, {nfa.states[s] : s in R}); R is always
    // non-empty (only non-empty subsets are ever enqueued, see RelabelRec).
    bdd row_set = nfa.states[R.front()];
    for (std::size_t k = 1; k < R.size(); ++k)
      row_set = nfa.pool.set_union(row_set, nfa.states[R[k]]);

    std::unordered_map<int, bdd> memo;  // fresh per row, per-call memo scope
    const bdd row = RelabelRec(row_set, nfa, subset_index, pending, memo);

    // BFS discovery order assigns output indices 0,1,2,... in dequeue
    // order, so R's index always equals out->states.size() here.  Inlined
    // into the assert so the lookup vanishes entirely under NDEBUG (no
    // -Wunused-variable, theory-review "consider").
    assert(subset_index.at(R) == out->states.size());
    out->states.push_back(row);
  }

  return out;
}

}  // namespace ltlf_ek
