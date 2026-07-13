#include "ltlf_ek/dfa_product.hpp"

#include <vector>

#include <bddx.h>
#include <spot/twa/twagraph.hh>

#include "ltlf_ek/ltlf_to_dfa.hpp"
#include "ltlf_ek/product.hpp"
#include "ltlf_ek/solve_dfa.hpp"
#include "ltlf_ek/variables.hpp"

namespace ltlf_ek {

std::optional<Controller> DfaProduct::synthesize(const spot::formula& phi,
                                                 const VariablePartition& vars,
                                                 const Transducer& t_in,
                                                 const Transducer& t_out) {
  const std::vector<const Transducer*> taus{&t_in, &t_out};

  // --- Validation (PRD "Validation policy"): phi's APs ⊆ I∪O, one shared dict.
  validate_product_inputs(phi, vars, taus);

  const spot::bdd_dict_ptr dict = t_in.dict();

  // --- LtlfToDfa: A on the shared dict (accepting states = F_D). ---
  const spot::twa_graph_ptr dfa = ltlf_to_dfa(phi, dict);

  // --- Product P (alg:dfa_product) via the symbolic build (docs/prd/
  //     symbolic-dfa-product.md): per-dst guards computed directly from
  //     delta_edges/emits_region + Goal out-edges --- no LetterAlphabet, no
  //     minterm loop.
  const ProductState init{dfa->get_init_state_number(),
                          {t_in.initial_state(), t_out.initial_state()}};
  const ProductGuards pg = build_product_symbolic(dfa, taus, init);

  // --- Materialize: one twa_graph state per reachable ProductState, one
  //     guarded edge per destination.
  const spot::twa_graph_ptr product = materialize_product(pg, init, dict);

  // --- SolveDfa: solve the product game and lift the controller. ---
  return solve_dfa(product, vars);
}

}  // namespace ltlf_ek
