#include "ltlf_ek/detail/acceptance.hpp"

#include <bddx.h>

namespace ltlf_ek::detail {

void ensure_acceptance_readable(const spot::twa_graph_ptr& g, unsigned state,
                                spot::acc_cond::mark_t mark) {
  const auto out = g->out(state);
  if (out.begin() != out.end()) return;  // already has an out-edge.
  if (mark == spot::acc_cond::mark_t{}) return;  // nothing to make readable.
  g->new_edge(state, state, bddfalse, mark);
}

}  // namespace ltlf_ek::detail
