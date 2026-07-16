#include "ltlf_ek/ltlf_to_nfa.hpp"

#include "ltlf_ek/detail/past_ltlf_to_dfa.hpp"
#include "ltlf_ek/detail/reverse_dfa_to_nfa.hpp"

namespace ltlf_ek {

spot::twa_graph_ptr ltlf_to_nfa(const spot::formula& phi,
                                const spot::bdd_dict_ptr& dict) {
  const spot::twa_graph_ptr d = detail::past_ltlf_to_dfa(phi, dict);
  return detail::reverse_dfa_to_nfa(d);
}

}  // namespace ltlf_ek
