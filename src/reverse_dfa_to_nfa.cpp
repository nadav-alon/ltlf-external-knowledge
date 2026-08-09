#include "ltlf_ek/detail/reverse_dfa_to_nfa.hpp"

#include <bddx.h>
#include <spot/twa/acc.hh>

#include "ltlf_ek/detail/acceptance.hpp"

namespace ltlf_ek::detail {

spot::twa_graph_ptr reverse_dfa_to_nfa(const spot::twa_graph_ptr& d) {
  spot::twa_graph_ptr n = spot::make_twa_graph(d->get_dict());
  for (const spot::formula& ap : d->ap()) n->register_ap(ap.ap_name());
  n->set_buchi();
  n->prop_state_acc(true);

  const unsigned num_d_states = d->num_states();
  n->new_states(num_d_states + 1);
  const unsigned fresh_init = num_d_states;
  n->set_init_state(fresh_init);

  const spot::acc_cond::mark_t kFinal = {0};
  const spot::acc_cond::mark_t kNone = {};
  const unsigned s0 = d->get_init_state_number();

  for (unsigned s = 0; s < num_d_states; ++s) {
    for (const auto& e : d->out(s)) {
      // Reversed edge e.dst --v--> s; mark-on-out-edge convention: the new
      // edge's SOURCE (e.dst, post-reversal) carries the final mark iff it
      // is F_N = {s0}.
      n->new_edge(e.dst, s, e.cond, e.dst == s0 ? kFinal : kNone);
      // s_{N,0}'s out-edges reach the v-predecessors of D's accepting
      // states.
      if (d->state_is_accepting(e.dst))
        n->new_edge(fresh_init, s, e.cond, kNone);
    }
  }

  // Defensive self-loop on s0 (see header doc-comment): s0 is unconditionally
  // in F_N, so it must read as accepting even if the loop above gave it no
  // real out-edges. ensure_acceptance_readable no-ops when s0 already has an
  // out-edge, which is exactly the condition under which the previously
  // unconditional self-loop was itself erased by purge_dead_states() below:
  // that purge is a NO-SUCCESSOR purge (marks play no part in it), and its
  // documented exception keeps a bddfalse self-loop only when it is the
  // state's sole outgoing edge. Same final graph either way -- pinned by
  // ReverseDfaToNfaSelfLoopEquivalence.* in tests/reverse_dfa_to_nfa_test.cpp,
  // which compares the two constructions edge for edge on the shapes where
  // they could differ (docs/prd/acceptance-mark-on-edgeless-states.md).
  ensure_acceptance_readable(n, s0, kFinal);

  n->purge_unreachable_states();
  n->purge_dead_states();
  return n;
}

}  // namespace ltlf_ek::detail
