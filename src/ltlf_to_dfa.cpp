#include "ltlf_ek/ltlf_to_dfa.hpp"

#include <spot/twaalgos/complete.hh>
#include <spot/twaalgos/ltlf2dfa.hh>

namespace ltlf_ek {

spot::twa_graph_ptr ltlf_to_dfa(const spot::formula& phi,
                                const spot::bdd_dict_ptr& dict) {
  // ltlf_to_mtdfa builds the multi-terminal DFA for phi on the shared dict;
  // as_twa(state_based=true) materialises it as an explicit deterministic
  // twa_graph whose *accepting states* are the final DFA states F_D (the
  // acceptance-mark encoding of finiteness --- no extra AP).
  spot::mtdfa_ptr mtdfa = spot::ltlf_to_mtdfa(phi, dict);
  spot::twa_graph_ptr dfa = mtdfa->as_twa(/*state_based=*/true);

  // alg:dfa_product's product loop reads delta_D(s, v) for every letter v, so
  // delta_D must be total; complete adds a non-accepting rejecting sink for the
  // letters an incomplete DFA would leave undefined.
  spot::complete_here(dfa);
  return dfa;
}

}  // namespace ltlf_ek
