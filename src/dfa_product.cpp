#include "ltlf_ek/dfa_product.hpp"

#include <stdexcept>

#include "ltlf_ek/consistency.hpp"

namespace ltlf_ek {

std::optional<Controller> DfaProduct::synthesize(const spot::formula& /*phi*/,
                                                 const VariablePartition& /*vars*/,
                                                 const Transducer& /*t_in*/,
                                                 const Transducer& /*t_out*/) {
  // TODO(developer): implement main.tex Algorithm "DFA Product":
  //   1. A = LtlfToDfa(phi)                     (Spot: translate to a DFA)
  //   2. build product states S_D × Q_in × Q_out ∪ {⊥}
  //   3. for each (s, v, q_out, q_in):
  //        if consistent(t_in, q_in, t_out, q_out, v):
  //            delta_prod(...) = (delta_D(s,v), delta_in(q_in,v), delta_out(q_out,v))
  //        else: delta_prod(...) = ⊥            (sink self-loops)
  //   4. C = SolveDfa(P)
  throw std::logic_error("DfaProduct::synthesize: not yet implemented");
}

}  // namespace ltlf_ek
