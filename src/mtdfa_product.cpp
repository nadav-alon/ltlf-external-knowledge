#include "ltlf_ek/mtdfa_product.hpp"

#include <vector>

#include <spot/twaalgos/ltlf2dfa.hh>

#include "ltlf_ek/emits_dfa.hpp"
#include "ltlf_ek/product.hpp"
#include "ltlf_ek/solve_mtdfa.hpp"
#include "ltlf_ek/turn_order.hpp"

namespace ltlf_ek {

std::optional<Controller> MtdfaProduct::synthesize(const spot::formula& phi,
                                                    const VariablePartition& vars,
                                                    const Transducer& t_in,
                                                    const Transducer& t_out) {
  const std::vector<const Transducer*> taus{&t_in, &t_out};

  // Same validation preamble as DfaProduct (product.hpp "Shared validation
  // preamble"): phi's APs subseteq I∪O, one shared bdd_dict.
  validate_product_inputs(phi, vars, taus);

  const spot::bdd_dict_ptr dict = t_in.dict();

  // Phase 0/Q2: guard the AP order BEFORE spot::ltlf_to_mtdfa is ever called
  // --- there is no ltlf_ek::ltlf_to_mtdfa wrapper (Interfaces & types).
  require_turn_order_aps(vars, dict);

  // Decision 1: the product is a language intersection of the Goal's mtdfa
  // and each transducer's Output-agreement automaton (emits_dfa), lifted to
  // mtdfa via twadfa_to_mtdfa.  Route (a): only SPOT_API surface
  // (ltlf_to_mtdfa, twadfa_to_mtdfa, product) --- no bespoke terminal
  // rewriting.
  const spot::mtdfa_ptr goal = spot::ltlf_to_mtdfa(phi, dict);
  const spot::mtdfa_ptr in_agreement =
      spot::twadfa_to_mtdfa(emits_dfa(t_in, dict));
  const spot::mtdfa_ptr out_agreement =
      spot::twadfa_to_mtdfa(emits_dfa(t_out, dict));

  const spot::mtdfa_ptr product =
      spot::product(spot::product(goal, in_agreement), out_agreement);

  // SolveDfa (mtdfa sibling): decision 2 pins Iknown, Oknown as forced
  // system moves; nullopt = unrealizable.
  return solve_mtdfa(product, vars);
}

}  // namespace ltlf_ek
