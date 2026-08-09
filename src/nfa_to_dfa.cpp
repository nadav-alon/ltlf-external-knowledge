#include "ltlf_ek/nfa_to_dfa.hpp"

#include <algorithm>
#include <cstddef>
#include <map>
#include <queue>
#include <vector>

#include <bddx.h>
#include <spot/twa/acc.hh>

#include "ltlf_ek/detail/acceptance.hpp"

namespace ltlf_ek {

namespace {

// Every full minterm v in 2^{ap_vars}, LSB-first in ap_vars order --- the
// same enumeration idiom as product.cpp's file-local all_letters, but over
// `nfa`'s OWN registered APs rather than a VariablePartition (nfa_to_dfa is
// partition-agnostic).  Empty ap_vars => {bddtrue} (size 1).
std::vector<bdd> all_minterms(const std::vector<int>& ap_vars) {
  const std::size_t n = ap_vars.size();
  std::vector<bdd> minterms;
  minterms.reserve(std::size_t{1} << n);
  for (std::size_t k = 0; k < (std::size_t{1} << n); ++k) {
    bdd v = bddtrue;
    for (std::size_t i = 0; i < n; ++i)
      v &= (k >> i & 1) ? bdd_ithvar(ap_vars[i]) : bdd_nithvar(ap_vars[i]);
    minterms.push_back(v);
  }
  return minterms;
}

}  // namespace

spot::twa_graph_ptr nfa_to_dfa(const spot::twa_graph_ptr& nfa) {
  spot::twa_graph_ptr dfa = spot::make_twa_graph(nfa->get_dict());
  std::vector<int> ap_vars;
  ap_vars.reserve(nfa->ap().size());
  for (const spot::formula& ap : nfa->ap())
    ap_vars.push_back(dfa->register_ap(ap.ap_name()));
  dfa->set_buchi();
  dfa->prop_state_acc(true);

  const std::vector<bdd> minterms = all_minterms(ap_vars);
  const spot::acc_cond::mark_t kFinal = {0};
  const spot::acc_cond::mark_t kNone = {};

  // Subset states: sorted, de-duplicated vectors of nfa state ids, interned
  // to an output DFA state id via this map.  R0 = {nfa's init state} at
  // output id 0, so dfa's states()[0] is the initial state (solve_dfa's
  // expectation).
  std::map<std::vector<unsigned>, unsigned> interned;
  std::queue<std::vector<unsigned>> worklist;

  const std::vector<unsigned> r0{nfa->get_init_state_number()};
  interned.emplace(r0, dfa->new_state());
  dfa->set_init_state(0);
  worklist.push(r0);

  auto is_accepting_subset = [&](const std::vector<unsigned>& r) {
    for (unsigned s : r)
      if (nfa->state_is_accepting(s)) return true;
    return false;
  };

  while (!worklist.empty()) {
    const std::vector<unsigned> cur = worklist.front();
    worklist.pop();
    const unsigned src = interned.at(cur);
    const bool acc = is_accepting_subset(cur);
    const spot::acc_cond::mark_t mark = acc ? kFinal : kNone;

    for (const bdd& v : minterms) {
      // R' = union over s in R of nfa's out-edges from s whose guard v
      // satisfies --- sorted + de-duplicated via std::set-like insertion
      // (small subsets, so a linear scan + std::map key is simplest).
      std::vector<unsigned> next;
      for (unsigned s : cur)
        for (const auto& e : nfa->out(s))
          if ((v & e.cond) != bddfalse) next.push_back(e.dst);
      if (next.empty()) continue;  // ∅-skip: no edge for this letter.
      std::sort(next.begin(), next.end());
      next.erase(std::unique(next.begin(), next.end()), next.end());

      auto it = interned.find(next);
      unsigned dst;
      if (it == interned.end()) {
        dst = dfa->new_state();
        interned.emplace(next, dst);
        worklist.push(next);
      } else {
        dst = it->second;
      }
      dfa->new_edge(src, dst, v, mark);
    }

    // Defensive self-loop (same pattern as detail::reverse_dfa_to_nfa's
    // accepting-dead-end fix): if R is accepting but every letter was
    // ∅-skipped, R has zero real out-edges, and twa_graph::state_is_accepting
    // reads its mark off the FIRST out-edge --- with none, it would silently
    // read back false.  See docs/prd/acceptance-mark-on-edgeless-states.md.
    detail::ensure_acceptance_readable(dfa, src, mark);
  }
  return dfa;
}

}  // namespace ltlf_ek
