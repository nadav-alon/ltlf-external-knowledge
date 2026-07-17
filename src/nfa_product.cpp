#include "ltlf_ek/nfa_product.hpp"

#include <vector>

#include <bddx.h>
#include <spot/twa/twagraph.hh>
#include <spot/twaalgos/complete.hh>

#include "ltlf_ek/bench.hpp"
#include "ltlf_ek/ltlf_to_nfa.hpp"
#include "ltlf_ek/nfa_to_dfa.hpp"
#include "ltlf_ek/product.hpp"
#include "ltlf_ek/solve_dfa.hpp"
#include "ltlf_ek/variables.hpp"

namespace ltlf_ek {

std::optional<Controller> NfaProduct::synthesize(const spot::formula& phi,
                                                  const VariablePartition& vars,
                                                  const Transducer& t_in,
                                                  const Transducer& t_out) {
  const std::vector<const Transducer*> taus{&t_in, &t_out};

  // --- Validation (PRD "Validation policy"): phi's APs ⊆ I∪O, one shared dict.
  validate_product_inputs(phi, vars, taus);

  const spot::bdd_dict_ptr dict = t_in.dict();

  // --- LtlfToNfa: N on the shared dict --- nondeterministic, NOT completed,
  //     sole final state F_N = {s_{D,0}} (docs/GLOSSARY.md "Goal NFA
  //     construction"). ---
  spot::twa_graph_ptr nfa;
  {
    BenchTimer t(Stage::automaton_construction);
    nfa = ltlf_to_nfa(phi, dict);
  }

  // --- Product P (alg:nfa_product:cons) via the nondeterministic build
  //     (docs/prd/nfa-product.md): N is completed FIRST (N -> N_c, a fresh
  //     non-accepting sink, delta total) so that a cons-consistent letter on
  //     which the goal dies becomes a real successor {(sink, ...)} rather
  //     than being indistinguishable from a non-cons letter (Behaviour §1).
  //     Materialize P, then subset-determinize it into D (nfa_to_dfa) ---
  //     the "determinize" sub-span nests inside product_construction, per
  //     docs/prd/benchmarking.md. ---
  spot::twa_graph_ptr D;
  {
    BenchTimer t(Stage::product_construction);
    spot::complete_here(nfa);  // N -> N_c (rejecting sink, delta total)
    const LetterAlphabet alphabet(vars, nfa);
    const ProductState init{nfa->get_init_state_number(),
                            {t_in.initial_state(), t_out.initial_state()}};
    const ProductGuards pg = build_product_nondet(nfa, taus, init, alphabet);
    const spot::twa_graph_ptr P = materialize_product(pg, init, dict, vars);
    {
      BenchTimer sub("determinize");  // free-form nested sub-span
      D = nfa_to_dfa(P);
    }
  }

  // --- SolveDfa: solve the product game and lift the controller. ---
  BenchTimer t(Stage::game_solving);
  return solve_dfa(D, vars);
}

}  // namespace ltlf_ek
