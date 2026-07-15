#include "ltlf_ek/emits_dfa.hpp"

#include <map>
#include <optional>
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

  // The rejecting sink: created lazily, only the first time some letter is
  // ever non-agreeing at a reachable state (Phase 0/Q1: an unreachable sink
  // becomes a real, wasted root --- twadfa_to_mtdfa's absorption needs a
  // non-empty mark, which the sink's self-loop never carries).
  std::optional<unsigned> sink;
  auto sink_state = [&]() {
    if (!sink) {
      sink = g->new_state();
      g->new_edge(*sink, *sink, bddtrue, {});  // rejecting: no mark.
    }
    return *sink;
  };

  const spot::acc_cond::mark_t kAccepting = {0};

  while (!worklist.empty()) {
    const unsigned q = worklist.front();
    worklist.pop();
    const unsigned src = index.at(q);

    // emits_region(q): the region of every letter whose Sigma1 slice agrees
    // with lambda at q (docs/GLOSSARY.md "Output agreement (emits)", region
    // form).  Every out-edge of q --- including the sink edge below --- marks
    // q itself accepting (state-based acceptance, Phase 0/Q1).
    const bdd er = tau.emits_region(q);
    bdd covered = bddfalse;
    for (const auto& [guard, d] : tau.delta_edges(q)) {
      const bdd eg = guard & er;
      if (eg == bddfalse) continue;  // non-agreeing: no dead edges.
      covered |= eg;
      g->new_edge(src, discover(d), eg, kAccepting);
    }

    const bdd uncovered = !covered;
    if (uncovered != bddfalse)
      g->new_edge(src, sink_state(), uncovered, kAccepting);
  }

  return g;
}

}  // namespace ltlf_ek
