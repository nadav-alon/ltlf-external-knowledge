#include "ltlf_ek/dfa_product.hpp"

#include <map>
#include <set>
#include <string>
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

  // --- Product P (alg:dfa_product) via the shared build_product core. ---
  std::set<std::string> universe = vars.inputs();
  const std::set<std::string> outs = vars.outputs();
  universe.insert(outs.begin(), outs.end());

  spot::twa_graph_ptr product = spot::make_twa_graph(dict);
  std::vector<int> io_vars;
  io_vars.reserve(universe.size());
  for (const auto& n : universe) io_vars.push_back(product->register_ap(n));
  product->set_buchi();
  product->prop_state_acc(true);

  const std::vector<bdd> letters = all_letters(io_vars);
  const spot::acc_cond::mark_t kFinalMark = {0};
  const spot::acc_cond::mark_t kNoMark = {};

  const ProductState init{dfa->get_init_state_number(),
                          {t_in.initial_state(), t_out.initial_state()}};
  const std::map<ProductState, ProductNode> graph =
      build_product(dfa, taus, init, letters, /*goal_must_be_complete=*/true);

  // --- Materialize: one twa_graph state per reachable ProductState, one
  //     guarded edge per destination (group-by-dst: guard |= letters[idx]).
  std::map<ProductState, unsigned> index;
  for (const auto& [state, node] : graph) index.emplace(state, product->new_state());
  product->set_init_state(index.at(init));

  for (const auto& [state, node] : graph) {
    const unsigned src = index.at(state);
    // F_P = F_D × Q_1 × ... (alg:dfa_product:final): state-based, so mark
    // every out-edge.
    const spot::acc_cond::mark_t mark = node.acc ? kFinalMark : kNoMark;

    std::map<unsigned, bdd> guards;  // dst state index -> accumulated guard.
    for (const auto& [idx, succ] : node.edges)
      guards[index.at(succ)] |= letters[idx];  // bdd default-constructs to bddfalse.

    for (const auto& [dst, guard] : guards)
      product->new_edge(src, dst, guard, mark);
  }

  // --- SolveDfa: solve the product game and lift the controller. ---
  return solve_dfa(product, vars);
}

}  // namespace ltlf_ek
