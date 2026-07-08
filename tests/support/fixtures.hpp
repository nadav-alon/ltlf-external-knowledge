#pragma once

#include <string>

#include <bddx.h>
#include <spot/tl/parse.hh>
#include <spot/twa/bdddict.hh>
#include <spot/twa/twagraph.hh>

#include "ltlf_ek/output_labeled_transducer.hpp"
#include "ltlf_ek/variables.hpp"

// Shared test-only fixtures (docs/prd/architecture-cleanup.md "New:
// tests/support/fixtures.hpp"): the small formula / partition / transducer
// building blocks that were duplicated file-locally across
// tests/verify_controller_test.cpp, tests/dfa_product_test.cpp and
// tests/ltlf_to_dfa_test.cpp before this header existed.
//
// Policy: prefer the library factory over a fixture --- e.g.
// ltlf_ek::trivial_transducer already covers the empty-knowledge (V = ∅)
// transducer, so it is not duplicated here.  This header holds only what the
// library does not provide.
namespace ltlf_ek::test_support {

// Parses `s` as an LTLf formula (spot::parse_formula, thin wrapper so callers
// do not repeat the include/spelling).
inline spot::formula Phi(const std::string& s) {
  return spot::parse_formula(s);
}

// The shared running partition used by most fixtures: I = {i} free,
// O = {o} free, V = ∅.
inline VariablePartition IoFreeVars() {
  return VariablePartition::split({"i"}, {"o"}, /*governed=*/{});
}

// An input-knowledge transducer committing G(i) (always i): Sigma0 = Ifree =
// ∅, Sigma1 = Iknown = {i}, lambda = i at its single state.
inline OutputLabeledTransducer TinAlwaysI(const spot::bdd_dict_ptr& dict) {
  auto g = spot::make_twa_graph(dict);
  int iv = g->register_ap("i");
  g->register_ap("o");
  g->new_states(1);
  g->set_init_state(0);
  g->new_edge(0, 0, bddtrue);
  return OutputLabeledTransducer(g, {bdd_ithvar(iv)}, /*sigma0=*/bddtrue,
                                 /*sigma1=*/bdd_ithvar(iv));
}

// A single-state, totally-defined Role::t_c controller (Sigma0 = observed
// cube, Sigma1 = produced cube) that commits a fixed output cube `out` at
// every step, regardless of the input --- the building block for an
// ok/mutated controller pair on the same delta graph.
inline OutputLabeledTransducer ConstantOutputTc(const spot::twa_graph_ptr& g,
                                                bdd out, bdd sigma0_cube,
                                                bdd sigma1_cube) {
  return OutputLabeledTransducer(g, {out}, sigma0_cube, sigma1_cube);
}

}  // namespace ltlf_ek::test_support
