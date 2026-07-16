#include <optional>
#include <string>
#include <vector>

#include <gtest/gtest.h>
#include <spot/tl/parse.hh>
#include <spot/twa/bdddict.hh>

#include "ltlf_ek/bench.hpp"
#include "ltlf_ek/mtdfa_product.hpp"
#include "ltlf_ek/output_labeled_transducer.hpp"
#include "ltlf_ek/role.hpp"
#include "ltlf_ek/turn_order.hpp"
#include "ltlf_ek/variables.hpp"
#include "ltlf_ek/verify_controller.hpp"

#include "support/fixtures.hpp"

// Tests for docs/prd/mtdfa-product.md Phase 2 delta ONLY: the BenchTimer
// wiring in src/mtdfa_product.cpp and the minimize_mtdfa constructor knob.
// Phase 1's MtdfaProduct behaviour (realizability corpora, controller shape,
// edge cases) is covered by tests/mtdfa_product_test.cpp and is out of scope
// here. Mirrors tests/bench_test.cpp's
// BenchScopeIntegration.DfaProductEmitsCanonicalStagesOnceEachInOrder for the
// bench-shape pattern.
//
// Timing is non-deterministic (bench_test.cpp precedent): every assertion
// below is on span structure/labels/ordering, never absolute duration.
namespace {

using ltlf_ek::BenchReport;
using ltlf_ek::BenchScope;
using ltlf_ek::Controller;
using ltlf_ek::MtdfaProduct;
using ltlf_ek::OutputLabeledTransducer;
using ltlf_ek::register_turn_order_aps;
using ltlf_ek::Role;
using ltlf_ek::Stage;
using ltlf_ek::stage_name;
using ltlf_ek::trivial_transducer;
using ltlf_ek::VariablePartition;
using ltlf_ek::verify_controller;

using ltlf_ek::test_support::IoFreeVars;
using ltlf_ek::test_support::Phi;

// The realizable / unrealizable fixtures reused throughout: same partition
// (I={i} free, O={o} free, V=empty) and trivial_transducer's empty-knowledge
// t_in/t_out as tests/mtdfa_product_test.cpp's EmptyKnowledgeVars/Trivial and
// "G(i -> o)" / "i" rows -- not hand-rolled here.
VariablePartition BenchVars() { return IoFreeVars(); }

// ---------------------------------------------------------------------------
// 1. Bench-stage shape: MtdfaProduct::synthesize under a BenchScope emits
// exactly the three canonical stages, in order -- and, since emits_dfa /
// twadfa_to_mtdfa / product all run inside the product_construction
// BenchTimer (src/mtdfa_product.cpp:54-60) while automaton_construction
// brackets spot::ltlf_to_mtdfa alone (src/mtdfa_product.cpp:48-50), this
// three-root shape IS the assertion that emits_dfa's work is attributed to
// product_construction, not automaton_construction -- the PRD's "Benchmarking"
// table claim, encoded structurally (span duration is non-deterministic, so
// the attribution is checked by which BRACKET the work falls in, not by a
// timing comparison).
// ---------------------------------------------------------------------------

TEST(BenchScopeIntegration, MtdfaProductEmitsCanonicalStagesOnceEachInOrder) {
  const spot::bdd_dict_ptr dict = spot::make_bdd_dict();
  const VariablePartition vars = BenchVars();
  register_turn_order_aps(vars, dict);
  const OutputLabeledTransducer t_in =
      trivial_transducer(vars, Role::t_in, dict);
  const OutputLabeledTransducer t_out =
      trivial_transducer(vars, Role::t_out, dict);
  MtdfaProduct method;  // minimize_mtdfa defaults to off.

  BenchReport report;
  {
    BenchScope scope;
    const std::optional<Controller> controller =
        method.synthesize(Phi("G(i -> o)"), vars, t_in, t_out);
    ASSERT_TRUE(controller.has_value())
        << "fixture must stay realizable for the integration oracle to mean "
          "anything";
    report = scope.report();
  }

  ASSERT_EQ(report.roots.size(), 3u)
      << "expected exactly automaton_construction, product_construction, "
        "game_solving -- emits_dfa's work must land inside "
        "product_construction, not spill a fourth root";
  const std::vector<Stage> expected_order = {
      Stage::automaton_construction, Stage::product_construction,
      Stage::game_solving};
  for (std::size_t i = 0; i < expected_order.size(); ++i) {
    SCOPED_TRACE("root index " + std::to_string(i));
    EXPECT_EQ(report.roots[i].label,
             std::string(stage_name(expected_order[i])));
    EXPECT_TRUE(report.roots[i].canonical);
  }
  EXPECT_GT(report.total.count(), 0);
}

// ---------------------------------------------------------------------------
// 2. minimize_mtdfa knob (constructor MtdfaProduct(bool minimize_mtdfa =
// false), Spot call spot::minimize_mtdfa).
// ---------------------------------------------------------------------------

// Verdict identity -- realizable fixture: on vs off agree, and the on-run's
// controller still passes the universal post-condition verifier.
TEST(MtdfaProductMinimizeMtdfa,
    VerdictIdentityOnRealizableFixtureAndOnRunControllerPassesVerifier) {
  const spot::bdd_dict_ptr dict = spot::make_bdd_dict();
  const VariablePartition vars = BenchVars();
  register_turn_order_aps(vars, dict);
  const OutputLabeledTransducer t_in =
      trivial_transducer(vars, Role::t_in, dict);
  const OutputLabeledTransducer t_out =
      trivial_transducer(vars, Role::t_out, dict);
  const spot::formula phi = Phi("G(i -> o)");

  MtdfaProduct off(/*minimize_mtdfa=*/false);
  MtdfaProduct on(/*minimize_mtdfa=*/true);
  const std::optional<Controller> off_result =
      off.synthesize(phi, vars, t_in, t_out);
  const std::optional<Controller> on_result =
      on.synthesize(phi, vars, t_in, t_out);

  ASSERT_TRUE(off_result.has_value()) << "fixture must be realizable";
  ASSERT_TRUE(on_result.has_value())
      << "minimize_mtdfa must not change the realizability verdict";
  EXPECT_TRUE(verify_controller(phi, vars, t_in, t_out, *on_result).ok)
      << "the minimize_mtdfa=true controller must still satisfy "
        "def:probDefTransducer";
}

// Verdict identity -- unrealizable fixture: on vs off still agree.
TEST(MtdfaProductMinimizeMtdfa, VerdictIdentityOnUnrealizableFixture) {
  const spot::bdd_dict_ptr dict = spot::make_bdd_dict();
  const VariablePartition vars = BenchVars();
  register_turn_order_aps(vars, dict);
  const OutputLabeledTransducer t_in =
      trivial_transducer(vars, Role::t_in, dict);
  const OutputLabeledTransducer t_out =
      trivial_transducer(vars, Role::t_out, dict);
  const spot::formula phi = Phi("i");  // unrealizable: mtdfa_product_test.cpp
                                       // BuildEmptyKnowledgeCorpus's "i" row.

  MtdfaProduct off(/*minimize_mtdfa=*/false);
  MtdfaProduct on(/*minimize_mtdfa=*/true);
  EXPECT_FALSE(off.synthesize(phi, vars, t_in, t_out).has_value());
  EXPECT_FALSE(on.synthesize(phi, vars, t_in, t_out).has_value())
      << "minimize_mtdfa must not turn an unrealizable verdict realizable";
}

// Regression: default (off) still emits exactly the three canonical stages,
// byte-for-byte unchanged from Phase 1 -- and, since minimize_mtdfa_ is false,
// no "minimize_mtdfa" span is opened at all (src/mtdfa_product.cpp:65-68 is
// gated on the flag). Encodes the developer's Phase-2 smoke-test claim
// (PRD "Phase 2" green checkpoint) as an assertion.
TEST(MtdfaProductMinimizeMtdfa,
    OffRunRegressesToExactlyThreeCanonicalStagesWithNoMinimizeMtdfaSpan) {
  const spot::bdd_dict_ptr dict = spot::make_bdd_dict();
  const VariablePartition vars = BenchVars();
  register_turn_order_aps(vars, dict);
  const OutputLabeledTransducer t_in =
      trivial_transducer(vars, Role::t_in, dict);
  const OutputLabeledTransducer t_out =
      trivial_transducer(vars, Role::t_out, dict);
  MtdfaProduct off(/*minimize_mtdfa=*/false);

  BenchReport report;
  {
    BenchScope scope;
    const std::optional<Controller> controller =
        off.synthesize(Phi("G(i -> o)"), vars, t_in, t_out);
    ASSERT_TRUE(controller.has_value());
    report = scope.report();
  }

  ASSERT_EQ(report.roots.size(), 3u);
  const std::vector<Stage> expected_order = {
      Stage::automaton_construction, Stage::product_construction,
      Stage::game_solving};
  for (std::size_t i = 0; i < expected_order.size(); ++i) {
    SCOPED_TRACE("root index " + std::to_string(i));
    EXPECT_EQ(report.roots[i].label,
             std::string(stage_name(expected_order[i])));
    EXPECT_TRUE(report.roots[i].canonical);
    EXPECT_NE(report.roots[i].label, "minimize_mtdfa");
  }
}

// Span presence, on: exactly one free-form (canonical:false) "minimize_mtdfa"
// span, positioned between product_construction and game_solving --
// src/mtdfa_product.cpp opens it as its own top-level BenchTimer, not nested
// under product_construction, so it is the third of four roots.
TEST(MtdfaProductMinimizeMtdfa,
    OnRunEmitsExactlyOneFreeFormMinimizeMtdfaSpanBetweenProductConstructionAndGameSolving) {
  const spot::bdd_dict_ptr dict = spot::make_bdd_dict();
  const VariablePartition vars = BenchVars();
  register_turn_order_aps(vars, dict);
  const OutputLabeledTransducer t_in =
      trivial_transducer(vars, Role::t_in, dict);
  const OutputLabeledTransducer t_out =
      trivial_transducer(vars, Role::t_out, dict);
  MtdfaProduct on(/*minimize_mtdfa=*/true);

  BenchReport report;
  {
    BenchScope scope;
    const std::optional<Controller> controller =
        on.synthesize(Phi("G(i -> o)"), vars, t_in, t_out);
    ASSERT_TRUE(controller.has_value());
    report = scope.report();
  }

  ASSERT_EQ(report.roots.size(), 4u)
      << "expected automaton_construction, product_construction, "
        "minimize_mtdfa, game_solving";
  EXPECT_EQ(report.roots[0].label,
           std::string(stage_name(Stage::automaton_construction)));
  EXPECT_TRUE(report.roots[0].canonical);
  EXPECT_EQ(report.roots[1].label,
           std::string(stage_name(Stage::product_construction)));
  EXPECT_TRUE(report.roots[1].canonical);
  EXPECT_EQ(report.roots[2].label, "minimize_mtdfa");
  EXPECT_FALSE(report.roots[2].canonical);
  EXPECT_EQ(report.roots[3].label,
           std::string(stage_name(Stage::game_solving)));
  EXPECT_TRUE(report.roots[3].canonical);
}

}  // namespace
