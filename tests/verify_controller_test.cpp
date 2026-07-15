#include <optional>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

#include <gtest/gtest.h>
#include <bddx.h>
#include <spot/tl/parse.hh>
#include <spot/twa/bdddict.hh>
#include <spot/twa/twagraph.hh>
#include <spot/twaalgos/synthesis.hh>

#include "ltlf_ek/cli.hpp"
#include "ltlf_ek/consistency.hpp"
#include "ltlf_ek/dfa_product.hpp"
#include "ltlf_ek/ltlf_to_dfa.hpp"
#include "ltlf_ek/output_labeled_transducer.hpp"
#include "ltlf_ek/synthesis.hpp"
#include "ltlf_ek/transducer_io.hpp"
#include "ltlf_ek/variables.hpp"
#include "ltlf_ek/verify_controller.hpp"

#include "support/fixtures.hpp"

// Test oracles for verify_controller / controller_as_transducer
// (docs/prd/controller-verifier.md "Test oracles" #1-#5; #6, the CLI
// end-to-end oracle, lives in tests/ltlf_ek_synth_test.cpp instead since it
// drives the ltlf-ek-synth binary).  verify_controller decides the
// def:probDefTransducer postcondition as a one-player (env) reachability
// fixpoint on A_phi x T_in x T_out x T_C --- NEVER via language inclusion ---
// so an accepting product state is *exempt* from Bad regardless of anything
// that happens afterward (the system may always stop there); every fixture
// below is built with that in mind (see oracle #4).
namespace {

using ltlf_ek::consistent;
using ltlf_ek::Controller;
using ltlf_ek::controller_as_transducer;
using ltlf_ek::DfaProduct;
using ltlf_ek::ltlf_to_dfa;
using ltlf_ek::OutputLabeledTransducer;
using ltlf_ek::Role;
using ltlf_ek::trivial_transducer;
using ltlf_ek::Transducer;
using ltlf_ek::VariablePartition;
using ltlf_ek::verify_controller;
using ltlf_ek::VerifyResult;
using ltlf_ek::Witness;

// Shared fixtures (tests/support/fixtures.hpp): Phi, IoFreeVars, TinAlwaysI,
// ConstantOutputTc.  The ok/lasso pairs below flip ConstantOutputTc's `out`
// between `o` and `!o` to turn the good controller into its mutated
// (lambda-flip) twin, on the SAME delta graph (oracles #1, #3).
using ltlf_ek::test_support::ConstantOutputTc;
using ltlf_ek::test_support::IoFreeVars;
using ltlf_ek::test_support::Phi;
using ltlf_ek::test_support::TinAlwaysI;

// ---------------------------------------------------------------------------
// Oracle #1: unit fixtures with a known verdict and known witness *shape*.
// ---------------------------------------------------------------------------

// Good: commits o := true at every step --- F(o) is satisfied by the very
// first letter, so the first post-virtual-start state is already Acc and the
// verdict is `ok` with no counterexample.
TEST(VerifyController, ConstantTrueOutputSatisfiesEventuallyO) {
  auto dict = spot::make_bdd_dict();
  auto vars = IoFreeVars();
  auto t_in = trivial_transducer(vars, Role::t_in, dict);
  auto t_out = trivial_transducer(vars, Role::t_out, dict);
  auto probe = spot::make_twa_graph(dict);
  const bdd ov = bdd_ithvar(probe->register_ap("o"));
  const bdd iv = bdd_ithvar(probe->register_ap("i"));

  auto g = spot::make_twa_graph(dict);
  g->new_states(1);
  g->set_init_state(0);
  g->new_edge(0, 0, bddtrue);
  auto t_c = ConstantOutputTc(g, ov, /*sigma0=*/iv, /*sigma1=*/ov);

  const VerifyResult r = verify_controller(Phi("F o"), vars, t_in, t_out, t_c);
  EXPECT_TRUE(r.ok);
  EXPECT_FALSE(r.counterexample.has_value());
}

// Bad (infinite-avoidance lasso): commits o := false at every step, on the
// SAME delta graph --- F(o) is never satisfied, so the product never
// accepts; every reachable state self-loops, giving a length-1 lasso.
TEST(VerifyController, ConstantFalseOutputNeverSatisfiesEventuallyO) {
  auto dict = spot::make_bdd_dict();
  auto vars = IoFreeVars();
  auto t_in = trivial_transducer(vars, Role::t_in, dict);
  auto t_out = trivial_transducer(vars, Role::t_out, dict);
  auto probe = spot::make_twa_graph(dict);
  const bdd ov = bdd_ithvar(probe->register_ap("o"));
  const bdd iv = bdd_ithvar(probe->register_ap("i"));

  auto g = spot::make_twa_graph(dict);
  g->new_states(1);
  g->set_init_state(0);
  g->new_edge(0, 0, bddtrue);
  auto t_c = ConstantOutputTc(g, !ov, /*sigma0=*/iv, /*sigma1=*/ov);

  const VerifyResult r = verify_controller(Phi("F o"), vars, t_in, t_out, t_c);
  EXPECT_FALSE(r.ok);
  ASSERT_TRUE(r.counterexample.has_value());
  EXPECT_FALSE(r.counterexample->cycle.empty())
      << "an always-false o loop is an infinite-avoidance lasso, not a "
         "dead-end";
}

// Bad (dead-end, empty cycle): state 0 is total (commits o := false always,
// F(o) never yet true), then moves to state 1 whose lambda_C is defined only
// when `i` holds --- so the environment choosing i=false at step 2 is a
// forced dead-end (main.tex \cref{def:enabled}; PRD "Edge cases": an
// undefined lambda_C on a reachable Ifree is a dead-end).
TEST(VerifyController, PartialControllerDeadEndsOnUndefinedLambda) {
  auto dict = spot::make_bdd_dict();
  auto vars = IoFreeVars();
  auto t_in = trivial_transducer(vars, Role::t_in, dict);
  auto t_out = trivial_transducer(vars, Role::t_out, dict);
  auto probe = spot::make_twa_graph(dict);
  const bdd ov = bdd_ithvar(probe->register_ap("o"));
  const bdd iv = bdd_ithvar(probe->register_ap("i"));

  auto g = spot::make_twa_graph(dict);
  g->new_states(2);
  g->set_init_state(0);
  g->new_edge(0, 1, bddtrue);  // delta(0, *) = 1, unconditionally.
  g->new_edge(1, 1, bddtrue);  // delta(1, *) = 1 (only matters when defined).
  // state 0: lambda commits o := false for ANY i (constant, i not mentioned).
  // state 1: lambda commits o := false only when i holds; undefined (no
  // completion) when i is false --- the forced dead-end.
  const std::vector<bdd> lambda = {!ov, iv & !ov};
  OutputLabeledTransducer t_c(g, lambda, /*sigma0=*/iv, /*sigma1=*/ov);

  const VerifyResult r = verify_controller(Phi("F o"), vars, t_in, t_out, t_c);
  EXPECT_FALSE(r.ok);
  ASSERT_TRUE(r.counterexample.has_value());
  EXPECT_TRUE(r.counterexample->cycle.empty())
      << "a forced dead-end is a lasso with an empty cycle";
  EXPECT_EQ(r.counterexample->prefix.size(), 1u)
      << "one step (into state 1) before the dead-end on the next Ifree";
}

// ---------------------------------------------------------------------------
// Oracle #4: reachability-vs-inclusion discriminator --- X[!] o forces a
// non-accepting length-1 prefix (strong next is unsatisfiable at length 1),
// so a naive language-inclusion checker would wrongly flag the length-1
// stopping point; the correct reachability semantics say `ok` because the
// system never actually stops there --- it is forced (by X[!]) to continue
// and sets `o` at the second letter.  Mirrors
// tests/dfa_product_test.cpp StrongNextOnOutputIsRealizable, this time
// through verify_controller directly (a hand-built T_C, not through
// DfaProduct/solve_dfa) --- confirming the two independent implementations
// share one termination semantics (PRD "Open theory questions").
TEST(VerifyController, StrongNextControllerIsSafeDespiteNonAcceptingFirstStep) {
  auto dict = spot::make_bdd_dict();
  auto vars = IoFreeVars();
  auto t_in = trivial_transducer(vars, Role::t_in, dict);
  auto t_out = trivial_transducer(vars, Role::t_out, dict);
  auto probe = spot::make_twa_graph(dict);
  const bdd ov = bdd_ithvar(probe->register_ap("o"));
  const bdd iv = bdd_ithvar(probe->register_ap("i"));

  auto g = spot::make_twa_graph(dict);
  g->new_states(2);
  g->set_init_state(0);
  g->new_edge(0, 1, bddtrue);  // delta(0, *) = 1.
  g->new_edge(1, 1, bddtrue);  // delta(1, *) = 1 (self loop).
  const std::vector<bdd> lambda = {!ov, ov};  // step 0: o=false; step >=1: o=true.
  OutputLabeledTransducer t_c(g, lambda, /*sigma0=*/iv, /*sigma1=*/ov);

  const VerifyResult r =
      verify_controller(Phi("X[!] o"), vars, t_in, t_out, t_c);
  EXPECT_TRUE(r.ok) << "reachability semantics: the system is forced past "
                       "the non-accepting length-1 prefix, then wins";
}

// ---------------------------------------------------------------------------
// Oracle #2 (positive metamorphic): every controller DfaProduct::synthesize
// returns must pass verify_controller --- ties the independent oracle to
// solve_dfa's verdict.  Corpus mirrors tests/dfa_product_test.cpp's
// realizable formulas (incl. the knowledge-sensitivity flip).
// ---------------------------------------------------------------------------

TEST(VerifyControllerMetamorphic, EveryDfaProductControllerIsSafe) {
  const std::vector<std::string> phis = {"o", "G(i -> o)", "X[!] o",
                                         "o <-> i", "G(o <-> i)"};
  for (const auto& phi : phis) {
    SCOPED_TRACE(phi);
    auto dict = spot::make_bdd_dict();
    auto vars = IoFreeVars();
    auto t_in = trivial_transducer(vars, Role::t_in, dict);
    auto t_out = trivial_transducer(vars, Role::t_out, dict);
    DfaProduct method;
    const std::optional<Controller> controller =
        method.synthesize(Phi(phi), vars, t_in, t_out);
    ASSERT_TRUE(controller.has_value()) << "expected " << phi << " realizable";
    EXPECT_TRUE(
        verify_controller(Phi(phi), vars, t_in, t_out, *controller).ok);
  }
}

// Empty Ofree corpus member: the controller controls nothing (PRD "Edge
// cases"); phi = 1 is trivially realizable and must still verify safe.
TEST(VerifyControllerMetamorphic, EmptyOutputFreeControllerIsSafe) {
  auto dict = spot::make_bdd_dict();
  auto vars = VariablePartition::split({"i"}, /*outputs=*/{}, {});
  auto t_in = trivial_transducer(vars, Role::t_in, dict);
  auto t_out = trivial_transducer(vars, Role::t_out, dict);
  DfaProduct method;
  const std::optional<Controller> controller =
      method.synthesize(Phi("1"), vars, t_in, t_out);
  ASSERT_TRUE(controller.has_value());
  EXPECT_TRUE(verify_controller(Phi("1"), vars, t_in, t_out, *controller).ok);
}

// Knowledge-sensitivity flip (dfa_product_test.cpp
// KnowledgeTurnsUnrealizableIntoRealizable's known-i branch): with i known
// (T_in always committing i), the synthesized controller must verify safe.
TEST(VerifyControllerMetamorphic, KnowledgeSensitiveControllerIsSafe) {
  const std::string phi = "X[!] 1 & (o <-> X i)";
  auto dict = spot::make_bdd_dict();
  auto t_in = TinAlwaysI(dict);
  auto t_out = trivial_transducer(
      VariablePartition::split({"i"}, {"o"}, /*governed=*/{"i"}), Role::t_out,
      dict);
  auto vars = VariablePartition::split({"i"}, {"o"}, /*governed=*/{"i"});
  DfaProduct method;
  const std::optional<Controller> controller =
      method.synthesize(Phi(phi), vars, t_in, t_out);
  ASSERT_TRUE(controller.has_value());
  EXPECT_TRUE(verify_controller(Phi(phi), vars, t_in, t_out, *controller).ok);
}

// ---------------------------------------------------------------------------
// Oracle #3 (discriminating negative): mutate a verified-good controller and
// confirm verify_controller flips to !ok with a self-consistent witness.
// ---------------------------------------------------------------------------

using ProdState = std::tuple<unsigned, unsigned, unsigned, unsigned>;

ProdState InitState(const spot::twa_graph_ptr& dfa, const Transducer& t_in,
                    const Transducer& t_out, const Transducer& t_c) {
  return {dfa->get_init_state_number(), t_in.initial_state(),
          t_out.initial_state(), t_c.initial_state()};
}

// Replays one letter through A_phi x T_in x T_out x T_C, asserting it is a
// legitimate *agreeing* transition (mirrors verify_controller's own agree()
// predicate --- docs/prd/controller-verifier.md) and that the resulting
// state is never F_phi --- the counterexample's self-consistency check
// (PRD "Test oracles" #3).
ProdState StepAgreeingAndNonAccepting(const spot::twa_graph_ptr& dfa,
                                      const Transducer& t_in,
                                      const Transducer& t_out,
                                      const Transducer& t_c,
                                      const ProdState& s, bdd v) {
  const auto [s_phi, q_in, q_out, q_c] = s;
  EXPECT_TRUE(consistent(t_in, q_in, t_out, q_out, v));

  const std::optional<bdd> lambda_c = t_c.lambda(q_c, v);
  EXPECT_TRUE(lambda_c.has_value());
  EXPECT_NE(v & *lambda_c, bddfalse);

  std::optional<unsigned> d_phi;
  for (const auto& e : dfa->out(s_phi))
    if ((v & e.cond) != bddfalse) { d_phi = e.dst; break; }
  const std::optional<unsigned> d_in = t_in.delta(q_in, v);
  const std::optional<unsigned> d_out = t_out.delta(q_out, v);
  const std::optional<unsigned> d_c = t_c.delta(q_c, v);
  EXPECT_TRUE(d_phi.has_value());
  EXPECT_TRUE(d_in.has_value());
  EXPECT_TRUE(d_out.has_value());
  EXPECT_TRUE(d_c.has_value());

  EXPECT_FALSE(dfa->state_is_accepting(*d_phi))
      << "witness self-consistency: must never enter F_phi";
  return {*d_phi, *d_in, *d_out, *d_c};
}

// Mutation: flip the single lambda_C output bit of the OK fixture above
// (o := true -> o := false, same delta graph) --- the "always-ok stub"
// failure mode this oracle guards against.  Then replay the witness's
// letters and confirm they form a genuine self-consistent lasso.
TEST(VerifyControllerDiscriminator, FlippingLambdaOutputBitTurnsSafeIntoUnsafe) {
  auto dict = spot::make_bdd_dict();
  auto vars = IoFreeVars();
  auto t_in = trivial_transducer(vars, Role::t_in, dict);
  auto t_out = trivial_transducer(vars, Role::t_out, dict);
  auto probe = spot::make_twa_graph(dict);
  const bdd ov = bdd_ithvar(probe->register_ap("o"));
  const bdd iv = bdd_ithvar(probe->register_ap("i"));

  auto g = spot::make_twa_graph(dict);
  g->new_states(1);
  g->set_init_state(0);
  g->new_edge(0, 0, bddtrue);

  OutputLabeledTransducer good(g, {ov}, /*sigma0=*/iv, /*sigma1=*/ov);
  ASSERT_TRUE(verify_controller(Phi("F o"), vars, t_in, t_out, good).ok);

  OutputLabeledTransducer mutated(g, {!ov}, /*sigma0=*/iv, /*sigma1=*/ov);
  const VerifyResult r =
      verify_controller(Phi("F o"), vars, t_in, t_out, mutated);
  ASSERT_FALSE(r.ok);
  ASSERT_TRUE(r.counterexample.has_value());
  const Witness& w = *r.counterexample;
  EXPECT_FALSE(w.cycle.empty());

  const spot::twa_graph_ptr dfa = ltlf_to_dfa(Phi("F o"), dict);
  ProdState s = InitState(dfa, t_in, t_out, mutated);
  for (const bdd& v : w.prefix)
    s = StepAgreeingAndNonAccepting(dfa, t_in, t_out, mutated, s, v);
  const ProdState cycle_head = s;
  for (const bdd& v : w.cycle)
    s = StepAgreeingAndNonAccepting(dfa, t_in, t_out, mutated, s, v);
  EXPECT_EQ(s, cycle_head) << "the cycle letters must return to their own "
                              "head, closing a genuine lasso";
}

// Mutation: redirect state 0's edge to a NEW bad successor instead of the
// original (correct) one --- the other mutation kind oracle #3 names.  Good:
// state 0 (o=false, not yet F(o)) -> state 1 (o=true forever, satisfies
// F(o) one step later).  Mutated: SAME state 0 lambda, but its edge is
// redirected into a new state 2 that commits o=false forever (never F(o)).
TEST(VerifyControllerDiscriminator, RedirectingAnEdgeTurnsSafeIntoUnsafe) {
  auto dict = spot::make_bdd_dict();
  auto vars = IoFreeVars();
  auto t_in = trivial_transducer(vars, Role::t_in, dict);
  auto t_out = trivial_transducer(vars, Role::t_out, dict);
  auto probe = spot::make_twa_graph(dict);
  const bdd ov = bdd_ithvar(probe->register_ap("o"));
  const bdd iv = bdd_ithvar(probe->register_ap("i"));

  spot::twa_graph_ptr good_g = spot::make_twa_graph(dict);
  good_g->new_states(2);
  good_g->set_init_state(0);
  good_g->new_edge(0, 1, bddtrue);  // delta(0,*) = 1 (correct successor).
  good_g->new_edge(1, 1, bddtrue);  // delta(1,*) = 1 (self loop).
  const OutputLabeledTransducer good(good_g, {!ov, ov}, /*sigma0=*/iv,
                                     /*sigma1=*/ov);
  ASSERT_TRUE(verify_controller(Phi("F o"), vars, t_in, t_out, good).ok);

  spot::twa_graph_ptr bad_g = spot::make_twa_graph(dict);
  bad_g->new_states(3);
  bad_g->set_init_state(0);
  bad_g->new_edge(0, 2, bddtrue);  // REDIRECTED: was 1, now the bad sink 2.
  bad_g->new_edge(1, 1, bddtrue);  // orphaned, unchanged, unreachable.
  bad_g->new_edge(2, 2, bddtrue);  // bad sink: self loop, always o=false.
  const OutputLabeledTransducer mutated(bad_g, {!ov, ov, !ov}, /*sigma0=*/iv,
                                        /*sigma1=*/ov);
  const VerifyResult r =
      verify_controller(Phi("F o"), vars, t_in, t_out, mutated);
  EXPECT_FALSE(r.ok);
  EXPECT_TRUE(r.counterexample.has_value());
}

// ---------------------------------------------------------------------------
// Oracle #5: controller_as_transducer round-trip --- the materialized
// Role::t_c transducer's lambda_C/delta_C reproduce the strategy graph's
// outputs on every state.
// ---------------------------------------------------------------------------

TEST(ControllerAsTransducer, ReproducesStrategyGraphOutputsOnEveryState) {
  auto dict = spot::make_bdd_dict();
  auto vars = IoFreeVars();
  auto t_in = trivial_transducer(vars, Role::t_in, dict);
  auto t_out = trivial_transducer(vars, Role::t_out, dict);
  DfaProduct method;
  const std::optional<Controller> controller =
      method.synthesize(Phi("G(i -> o)"), vars, t_in, t_out);
  ASSERT_TRUE(controller.has_value());

  const OutputLabeledTransducer t_c =
      controller_as_transducer(*controller, vars);

  // Independent re-collapse of the SAME split strategy graph
  // controller_as_transducer built `t_c` from --- deterministic, so this
  // must produce an isomorphic (here: identical) graph to check the
  // materializer's own wiring against, without re-deriving Spot's algorithm.
  const spot::twa_graph_ptr collapsed = spot::unsplit_2step(controller->strategy);
  ASSERT_EQ(t_c.initial_state(), collapsed->get_init_state_number());

  const int iv = collapsed->register_ap("i");
  for (unsigned q = 0; q < collapsed->num_states(); ++q) {
    SCOPED_TRACE(q);
    bdd union_out = bddfalse;
    for (const auto& e : collapsed->out(q)) union_out |= e.cond;

    for (bool i_val : {false, true}) {
      SCOPED_TRACE(i_val);
      const bdd obs = i_val ? bdd_ithvar(iv) : bdd_nithvar(iv);
      const bdd expected_completion = bdd_restrict(union_out, obs);
      const std::optional<bdd> got = t_c.lambda(q, obs);
      if (expected_completion == bddfalse) {
        EXPECT_FALSE(got.has_value());
        continue;
      }
      ASSERT_TRUE(got.has_value());
      const bdd expected_o = bdd_exist(expected_completion, t_c.sigma0_cube());
      EXPECT_EQ(*got, expected_o);

      const bdd letter = obs & *got;
      std::optional<unsigned> expected_dst;
      for (const auto& e : collapsed->out(q))
        if ((letter & e.cond) != bddfalse) { expected_dst = e.dst; break; }
      EXPECT_EQ(t_c.delta(q, letter), expected_dst);
    }
  }
}

// ---------------------------------------------------------------------------
// Phase 0/Q4 follow-up (docs/prd/mtdfa-product.md "Interfaces & types"
// src/synthesis.cpp): controller_as_transducer must unsplit ONLY when the
// "state-player" named property is present.  DfaProduct's Controller always
// carries it (solved_game_to_mealy, exercised above); solve_mtdfa's does
// NOT (mtdfa_strategy_to_mealy's output is already unsplit) --- calling
// spot::unsplit_2step unconditionally on such a Controller used to throw
// "get_state_players(): state-player property not defined, not a game?".
// This fixture stands in for that shape directly (a hand-built, already-
// unsplit Mealy machine with NO "state-player" property) so the regression
// is covered without needing solve_mtdfa/MtdfaProduct to exist yet
// (concurrent workflow).
// ---------------------------------------------------------------------------

TEST(ControllerAsTransducer, AcceptsAnAlreadyUnsplitMealyLackingTheStatePlayerProperty) {
  auto dict = spot::make_bdd_dict();
  auto vars = IoFreeVars();
  auto g = spot::make_twa_graph(dict);
  const int iv = g->register_ap("i");
  const int ov = g->register_ap("o");
  g->new_states(1);
  g->set_init_state(0);
  g->new_edge(0, 0, bdd_ithvar(ov));  // commits o := true regardless of i.

  ASSERT_EQ(g->get_named_prop<std::vector<bool>>("state-player"), nullptr)
      << "sanity: this graph must NOT carry the split-arena property --- "
         "mirroring mtdfa_strategy_to_mealy's (already-unsplit) output";

  const Controller controller{g};
  std::optional<OutputLabeledTransducer> t_c;
  EXPECT_NO_THROW(t_c = controller_as_transducer(controller, vars))
      << "controller_as_transducer must not call spot::unsplit_2step "
         "unconditionally --- only when \"state-player\" is present";
  ASSERT_TRUE(t_c.has_value());
  EXPECT_EQ(t_c->initial_state(), 0u);
  EXPECT_EQ(t_c->lambda(0, bdd_ithvar(iv)), std::optional<bdd>(bdd_ithvar(ov)));
  EXPECT_EQ(t_c->lambda(0, bdd_nithvar(iv)), std::optional<bdd>(bdd_ithvar(ov)));
  EXPECT_EQ(t_c->delta(0, bdd_ithvar(iv) & bdd_ithvar(ov)),
           std::optional<unsigned>(0u));
}

// ---------------------------------------------------------------------------
// Validation policy (same policy as DfaProduct::synthesize).
// ---------------------------------------------------------------------------

TEST(VerifyController, ThrowsOnFormulaApOutsideInputsOutputs) {
  auto dict = spot::make_bdd_dict();
  auto vars = IoFreeVars();
  auto t_in = trivial_transducer(vars, Role::t_in, dict);
  auto t_out = trivial_transducer(vars, Role::t_out, dict);
  auto probe = spot::make_twa_graph(dict);
  const bdd ov = bdd_ithvar(probe->register_ap("o"));
  const bdd iv = bdd_ithvar(probe->register_ap("i"));
  auto g = spot::make_twa_graph(dict);
  g->new_states(1);
  g->set_init_state(0);
  g->new_edge(0, 0, bddtrue);
  OutputLabeledTransducer t_c(g, {ov}, /*sigma0=*/iv, /*sigma1=*/ov);

  EXPECT_THROW(verify_controller(Phi("z"), vars, t_in, t_out, t_c),
              std::invalid_argument);
}

TEST(VerifyController, ThrowsWhenTransducersDoNotShareOneDict) {
  auto dict_a = spot::make_bdd_dict();
  auto dict_b = spot::make_bdd_dict();
  auto vars = IoFreeVars();
  auto t_in = trivial_transducer(vars, Role::t_in, dict_a);
  auto t_out = trivial_transducer(vars, Role::t_out, dict_a);
  auto probe = spot::make_twa_graph(dict_b);
  const bdd ov = bdd_ithvar(probe->register_ap("o"));
  const bdd iv = bdd_ithvar(probe->register_ap("i"));
  auto g = spot::make_twa_graph(dict_b);
  g->new_states(1);
  g->set_init_state(0);
  g->new_edge(0, 0, bddtrue);
  OutputLabeledTransducer t_c(g, {ov}, /*sigma0=*/iv, /*sigma1=*/ov);

  EXPECT_THROW(verify_controller(Phi("o"), vars, t_in, t_out, t_c),
              std::invalid_argument);
}

}  // namespace
