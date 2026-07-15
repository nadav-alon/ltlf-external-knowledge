#include "ltlf_ek/emits_dfa.hpp"

#include <map>
#include <queue>
#include <utility>

#include <bddx.h>

namespace ltlf_ek {

spot::twa_graph_ptr emits_dfa(const Transducer& tau,
                              const spot::bdd_dict_ptr& dict) {
  spot::twa_graph_ptr g = spot::make_twa_graph(dict);
  g->set_buchi();
  g->prop_state_acc(true);

  // tau's state -> g's state, discovered by BFS from tau.initial_state()
  // (Transducer exposes no state count; see the header comment).
  std::map<unsigned, unsigned> index;
  std::queue<unsigned> worklist;
  auto discover = [&](unsigned q) {
    auto [it, inserted] = index.emplace(q, 0);
    if (inserted) {
      it->second = g->new_state();
      worklist.push(q);
    }
    return it->second;
  };

  g->set_init_state(discover(tau.initial_state()));

  const spot::acc_cond::mark_t kAccepting = {0};

  while (!worklist.empty()) {
    const unsigned q = worklist.front();
    worklist.pop();
    const unsigned src = index.at(q);

    // emits_region(q): the region of every letter whose Sigma1 slice agrees
    // with lambda at q (docs/GLOSSARY.md "Output agreement (emits)", region
    // form).  Every out-edge of q marks q itself accepting (state-based
    // acceptance, Phase 0/Q1).  No sink: a letter not covered by any edge
    // below is simply a missing edge, an implicit reject (Phase 1 blocker
    // fix, docs/prd/mtdfa-product.md "Phase 1 blocker" --- a materialised
    // rejecting sink segfaults Spot's mtdfa_winning_strategy(backprop=true)).
    const bdd er = tau.emits_region(q);
    for (const auto& [guard, d] : tau.delta_edges(q)) {
      const bdd eg = guard & er;
      if (eg == bddfalse) continue;  // non-agreeing: no dead edges.
      g->new_edge(src, discover(d), eg, kAccepting);
    }
  }

  return g;
}

}  // namespace ltlf_ek
