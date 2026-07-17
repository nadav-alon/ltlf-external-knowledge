#pragma once

#include "ltlf_ek/synthesis.hpp"

namespace ltlf_ek {

// Method 1 --- NFA product (main.tex §nfa, \cref{alg:nfa_product}), EXPLICIT
// representation (docs/prd/nfa-product.md).  Builds N via ltlf_to_nfa,
// completes it (spot::complete_here --- restores the non-cons vs cons-dead
// distinction an incomplete N would otherwise blur inside the product),
// forms the nondeterministic product with T_in/T_out skipping non-cons
// letters (def:consistency, build_product_nondet), subset-determinizes it
// (nfa_to_dfa), then solves the game (solve_dfa) --- unchanged from
// DfaProduct's last stage.
//
// NfaProduct is the reference/baseline route for the mtdfa MtnfaProduct
// (docs/prd/mtnfa.md): correctness-obvious because it is built entirely from
// already-landed pieces (ltlf_to_nfa, LetterAlphabet, build_product_nondet,
// nfa_to_dfa, solve_dfa), and every realizable/unrealizable verdict is
// cross-checked against DfaProduct.
class NfaProduct final : public Synthesis {
 public:
  std::optional<Controller> synthesize(const spot::formula& phi,
                                       const VariablePartition& vars,
                                       const Transducer& t_in,
                                       const Transducer& t_out) override;
};

}  // namespace ltlf_ek
