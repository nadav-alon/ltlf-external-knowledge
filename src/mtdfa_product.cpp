#include "ltlf_ek/mtdfa_product.hpp"

#include <vector>

#include <spot/twaalgos/ltlf2dfa.hh>

#include "ltlf_ek/bench.hpp"
#include "ltlf_ek/emits_dfa.hpp"
#include "ltlf_ek/product.hpp"
#include "ltlf_ek/solve_mtdfa.hpp"
#include "ltlf_ek/turn_order.hpp"

namespace ltlf_ek {

MtdfaProduct::MtdfaProduct(bool minimize_mtdfa)
    : minimize_mtdfa_(minimize_mtdfa) {}

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
  //
  // Benchmarking (docs/prd/mtdfa-product.md "Benchmarking"): automaton_construction
  // is ltlf_to_mtdfa ALONE --- the measured win over DfaProduct's ltlf_to_dfa,
  // which also pays as_twa + complete_here.  emits_dfa / twadfa_to_mtdfa /
  // product are EK-crossing work and belong to product_construction, not this
  // span --- letting them in would muddy the one comparison this PRD exists
  // for.
  spot::mtdfa_ptr goal;
  {
    BenchTimer timer(Stage::automaton_construction);
    goal = spot::ltlf_to_mtdfa(phi, dict);
  }

  spot::mtdfa_ptr product;
  {
    BenchTimer timer(Stage::product_construction);
    const spot::mtdfa_ptr in_agreement =
        spot::twadfa_to_mtdfa(emits_dfa(t_in, dict));
    const spot::mtdfa_ptr out_agreement =
        spot::twadfa_to_mtdfa(emits_dfa(t_out, dict));
    product = spot::product(spot::product(goal, in_agreement), out_agreement);
  }

  // minimize_mtdfa knob (Phase 2, default off): Moore minimisation of the
  // product mtdfa, timed in its own free-form span so its cost is
  // attributable separately from the two canonical stages either side of it.
  if (minimize_mtdfa_) {
    BenchTimer timer(std::string("minimize_mtdfa"));
    product = spot::minimize_mtdfa(product);
  }

  // SolveDfa (mtdfa sibling): decision 2 pins Iknown, Oknown as forced
  // system moves; nullopt = unrealizable.
  BenchTimer timer(Stage::game_solving);
  return solve_mtdfa(product, vars);
}

}  // namespace ltlf_ek
