#include <stdexcept>
#include <string>
#include <vector>

#include <gtest/gtest.h>
#include <bddx.h>
#include <spot/tl/parse.hh>
#include <spot/twa/bdddict.hh>
#include <spot/twa/twagraph.hh>
#include <spot/twaalgos/ltlf2dfa.hh>

#include "ltlf_ek/dfa_product.hpp"
#include "ltlf_ek/output_labeled_transducer.hpp"
#include "ltlf_ek/solve_dfa.hpp"
#include "ltlf_ek/variables.hpp"

// End-to-end fixtures for DfaProduct (docs/GLOSSARY.md: "DFA product", Method 2).
// synthesize builds A = LtlfToDfa(phi), the product with T_in, T_out routing
// non-cons letters to the sink, and solves the game --- returning a Controller
// (realizable) or nullopt (unrealizable).
namespace {

using ltlf_ek::Controller;
using ltlf_ek::DfaProduct;
using ltlf_ek::OutputLabeledTransducer;
using ltlf_ek::Transducer;
using ltlf_ek::VariablePartition;

spot::formula Phi(const std::string& s) { return spot::parse_formula(s); }

// The empty-knowledge (empty V) transducer over {i, o}: single state, delta
// self-loops, lambda commits the empty cube --- so cons is trivially true.
OutputLabeledTransducer Trivial(const spot::bdd_dict_ptr& dict) {
  auto g = spot::make_twa_graph(dict);
  g->register_ap("i");
  g->register_ap("o");
  g->new_states(1);
  g->set_init_state(0);
  g->new_edge(0, 0, bddtrue);
  return OutputLabeledTransducer(g, {bddtrue}, /*sigma0=*/bddtrue,
                                 /*sigma1=*/bddtrue);
}

// An input-knowledge transducer committing G(i) (always i): Sigma0 = Ifree = ∅,
// Sigma1 = Iknown = {i}, lambda = i at its single state.
OutputLabeledTransducer TinAlwaysI(const spot::bdd_dict_ptr& dict) {
  auto g = spot::make_twa_graph(dict);
  int iv = g->register_ap("i");
  g->register_ap("o");
  g->new_states(1);
  g->set_init_state(0);
  g->new_edge(0, 0, bddtrue);
  return OutputLabeledTransducer(g, {bdd_ithvar(iv)}, /*sigma0=*/bddtrue,
                                 /*sigma1=*/bdd_ithvar(iv));
}

bool Realizable(const std::string& phi, const VariablePartition& vars,
                const Transducer& t_in, const Transducer& t_out) {
  DfaProduct method;
  return method.synthesize(Phi(phi), vars, t_in, t_out).has_value();
}

// Spot's own LTLf synthesis, used as the monolithic realizability baseline
// (single-state mtdfa: bddtrue = realizable).
bool SpotRealizable(const std::string& phi,
                    const std::vector<std::string>& outs) {
  auto dict = spot::make_bdd_dict();
  auto m = spot::ltlf_to_mtdfa_for_synthesis(
      Phi(phi), dict, outs, spot::ltlf_synthesis_backprop::dfs_node_backprop,
      /*one_step_preprocess=*/false, /*realizability=*/true);
  return m->states.size() == 1 && m->states[0] == bddtrue;
}

// --- Realizability verdicts on empty knowledge (hand-computed) -------------

TEST(DfaProduct, RealizableWhenSystemControlsTheOutput) {
  auto dict = spot::make_bdd_dict();
  auto t_in = Trivial(dict), t_out = Trivial(dict);
  auto vars = VariablePartition::split({"i"}, {"o"}, /*governed=*/{});
  // phi = o: system sets o at length 1 --- realizable.
  EXPECT_TRUE(Realizable("o", vars, t_in, t_out));
}

TEST(DfaProduct, UnrealizableWhenFormulaIsFalse) {
  auto dict = spot::make_bdd_dict();
  auto t_in = Trivial(dict), t_out = Trivial(dict);
  auto vars = VariablePartition::split({"i"}, {"o"}, {});
  EXPECT_FALSE(Realizable("0", vars, t_in, t_out));
}

TEST(DfaProduct, UnrealizableWhenGoalDependsOnFreeInput) {
  auto dict = spot::make_bdd_dict();
  auto t_in = Trivial(dict), t_out = Trivial(dict);
  auto vars = VariablePartition::split({"i"}, {"o"}, {});
  // phi = i, i free: the environment sets i false --- unrealizable.
  EXPECT_FALSE(Realizable("i", vars, t_in, t_out));
}

TEST(DfaProduct, RealizableWhenSystemCanReactToInput) {
  auto dict = spot::make_bdd_dict();
  auto t_in = Trivial(dict), t_out = Trivial(dict);
  auto vars = VariablePartition::split({"i"}, {"o"}, {});
  // phi = G(i -> o): the controller reads i and sets o accordingly.
  EXPECT_TRUE(Realizable("G(i -> o)", vars, t_in, t_out));
}

// Strong next (X[!]) forbids the early-stop escape: predicting a free future
// input is genuinely impossible, controlling a future output is fine.
TEST(DfaProduct, StrongNextOnFreeInputIsUnrealizable) {
  auto dict = spot::make_bdd_dict();
  auto t_in = Trivial(dict), t_out = Trivial(dict);
  auto vars = VariablePartition::split({"i"}, {"o"}, {});
  EXPECT_FALSE(Realizable("X[!] i", vars, t_in, t_out));
}

TEST(DfaProduct, StrongNextOnOutputIsRealizable) {
  auto dict = spot::make_bdd_dict();
  auto t_in = Trivial(dict), t_out = Trivial(dict);
  auto vars = VariablePartition::split({"i"}, {"o"}, {});
  EXPECT_TRUE(Realizable("X[!] o", vars, t_in, t_out));
}

// Empty Ofree: the controller controls nothing; realizability then depends only
// on the (here trivial) pinned strategies and phi.
TEST(DfaProduct, EmptyOutputFreeTriviallyTruePhiIsRealizable) {
  auto dict = spot::make_bdd_dict();
  auto t_in = Trivial(dict), t_out = Trivial(dict);
  auto vars = VariablePartition::split({"i"}, /*outputs=*/{}, {});
  EXPECT_TRUE(Realizable("1", vars, t_in, t_out));
}

TEST(DfaProduct, EmptyOutputFreeInputGoalIsUnrealizable) {
  auto dict = spot::make_bdd_dict();
  auto t_in = Trivial(dict), t_out = Trivial(dict);
  auto vars = VariablePartition::split({"i"}, /*outputs=*/{}, {});
  EXPECT_FALSE(Realizable("i", vars, t_in, t_out));
}

// --- Controller shape ------------------------------------------------------

TEST(DfaProduct, RealizableControllerCarriesAStrategy) {
  auto dict = spot::make_bdd_dict();
  auto t_in = Trivial(dict), t_out = Trivial(dict);
  auto vars = VariablePartition::split({"i"}, {"o"}, {});
  DfaProduct method;
  auto controller = method.synthesize(Phi("G(i -> o)"), vars, t_in, t_out);
  ASSERT_TRUE(controller.has_value());
  ASSERT_NE(controller->strategy, nullptr);
  EXPECT_GE(controller->strategy->num_states(), 1u);
}

// --- Monolithic baseline (empty V): agree with plain Spot LTLf synthesis ----
//
// Coarse cross-check only, on *turn-order-invariant* formulas.  Our controller
// is Mealy (main.tex §86: S_C sees v_t ∩ I, the current input), but Spot's
// ltlf_to_mtdfa_for_synthesis baseline is Moore (it commits the output before
// the current input), so the two disagree on Mealy-sensitive formulas like
// `o <-> i` --- those are asserted separately in ControllerMayReadCurrentInput,
// and deliberately excluded here.
TEST(DfaProduct, EmptyKnowledgeMatchesMonolithicBaseline) {
  const std::vector<std::string> phis = {
      "o",      "0",       "1",   "i",   "G(i -> o)", "X[!] i",
      "X[!] o", "F o",     "G i", "o U i", "i U o",   "G(o) | i"};
  for (const auto& phi : phis) {
    SCOPED_TRACE(phi);
    auto dict = spot::make_bdd_dict();
    auto t_in = Trivial(dict), t_out = Trivial(dict);
    auto vars = VariablePartition::split({"i"}, {"o"}, {});
    EXPECT_EQ(Realizable(phi, vars, t_in, t_out), SpotRealizable(phi, {"o"}));
  }
}

// The controller is Mealy: it may read the current input v_t ∩ I when producing
// v_t ∩ Ofree (main.tex §86, S_C's observed slice Σ0 = I).  So o <-> i is
// realizable --- the system copies the current input --- even though Spot's Moore
// mtdfa baseline calls it unrealizable (see EmptyKnowledgeMatchesMonolithicBaseline).
TEST(DfaProduct, ControllerMayReadCurrentInput) {
  auto dict = spot::make_bdd_dict();
  auto t_in = Trivial(dict), t_out = Trivial(dict);
  auto vars = VariablePartition::split({"i"}, {"o"}, {});
  EXPECT_TRUE(Realizable("o <-> i", vars, t_in, t_out));
  EXPECT_TRUE(Realizable("G(o <-> i)", vars, t_in, t_out));
}

// --- Knowledge sensitivity (metamorphic): knowledge can only add power ------
//
// phi forces a next step (X[!] 1, no early-stop escape) and ties o at step 0 to
// the input at step 1 (o <-> X i).  With i free the environment falsifies it;
// with i known and T_in committing G(i), the controller knows the next i and
// wins.  (Corrected from the PRD's oracle #4, whose weak-X example does not
// flip --- see the PRD "Developer comments" and
// memory ltlf-weak-x-and-termination-semantics.)
TEST(DfaProduct, KnowledgeTurnsUnrealizableIntoRealizable) {
  const std::string phi = "X[!] 1 & (o <-> X i)";
  {
    auto dict = spot::make_bdd_dict();
    auto t_in = Trivial(dict), t_out = Trivial(dict);
    auto free = VariablePartition::split({"i"}, {"o"}, /*governed=*/{});
    EXPECT_FALSE(Realizable(phi, free, t_in, t_out)) << "i free must lose";
  }
  {
    auto dict = spot::make_bdd_dict();
    auto t_in = TinAlwaysI(dict), t_out = Trivial(dict);
    auto known = VariablePartition::split({"i"}, {"o"}, /*governed=*/{"i"});
    EXPECT_TRUE(Realizable(phi, known, t_in, t_out)) << "i known must win";
  }
}

// --- Validation policy (PRD "Validation policy") ---------------------------

TEST(DfaProduct, ThrowsOnFormulaApOutsideInputsOutputs) {
  auto dict = spot::make_bdd_dict();
  auto t_in = Trivial(dict), t_out = Trivial(dict);
  auto vars = VariablePartition::split({"i"}, {"o"}, {});
  DfaProduct method;
  // z is neither an input nor an output.
  EXPECT_THROW(method.synthesize(Phi("z"), vars, t_in, t_out),
               std::invalid_argument);
}

TEST(DfaProduct, ThrowsWhenTransducersDoNotShareOneDict) {
  auto dict_a = spot::make_bdd_dict();
  auto dict_b = spot::make_bdd_dict();
  auto t_in = Trivial(dict_a), t_out = Trivial(dict_b);
  auto vars = VariablePartition::split({"i"}, {"o"}, {});
  DfaProduct method;
  EXPECT_THROW(method.synthesize(Phi("o"), vars, t_in, t_out),
               std::invalid_argument);
}

// solve_dfa relies on the kSinkProperty (recorded by the product builder) to
// drop the ⊥-edges; a product missing it would silently re-admit the sink, so
// solve_dfa must fail loudly rather than return a wrong verdict.
TEST(SolveDfa, ThrowsWhenProductLacksSinkProperty) {
  auto dict = spot::make_bdd_dict();
  auto product = spot::make_twa_graph(dict);
  product->register_ap("i");
  product->register_ap("o");
  product->set_buchi();
  product->prop_state_acc(true);
  product->new_states(1);
  product->set_init_state(0);
  product->new_edge(0, 0, bddtrue, {});  // no ltlf-ek-sink named property set.
  auto vars = VariablePartition::split({"i"}, {"o"}, {});
  EXPECT_THROW(ltlf_ek::solve_dfa(product, vars), std::invalid_argument);
}

}  // namespace
