#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include <gtest/gtest.h>
#include <spot/tl/parse.hh>
#include <spot/twa/bdddict.hh>
#include <spot/twaalgos/hoa.hh>

#include "ltlf_ek/bench.hpp"
#include "ltlf_ek/dfa_product.hpp"
#include "ltlf_ek/mtdfa_product.hpp"
#include "ltlf_ek/mtnfa_product.hpp"
#include "ltlf_ek/nfa_product.hpp"
#include "ltlf_ek/otf_mtdfa_product.hpp"
#include "ltlf_ek/output_labeled_transducer.hpp"
#include "ltlf_ek/role.hpp"
#include "ltlf_ek/synthesis.hpp"
#include "ltlf_ek/turn_order.hpp"
#include "ltlf_ek/variables.hpp"

#include "support/fixtures.hpp"

// Tests for docs/prd/benchmark-suite.md Phase 1 -- the metric sink
// (SizeMetric / size_metric_name / record_size_metric / BenchSizeMetric,
// BenchReport::metrics), bound to the PRD's frozen "Interfaces & types ->
// Phase 1" block and "Test oracles" #1 (size-metric emission, O1) and #4
// (zero-perturbation, O4), plus the "Edge cases" section.  Written on the
// concurrent-workflow test-writer branch *before* the sink exists in
// include/ltlf_ek/bench.hpp / the five method .cpp files necessarily do --
// so this file will not compile/link until the developer branch lands them
// (expected; the launcher integrates the two).  This file's territory is
// disjoint from tests/bench_test.cpp (Stage/span coverage, unaffected by
// this PRD's charge-table concern) and tests/mtdfa_bench_test.cpp /
// tests/nfa_bench_test.cpp (Stage-shape coverage for those two methods).
namespace {

using ltlf_ek::BenchReport;
using ltlf_ek::BenchScope;
using ltlf_ek::BenchSizeMetric;
using ltlf_ek::Controller;
using ltlf_ek::DfaProduct;
using ltlf_ek::MtdfaProduct;
using ltlf_ek::MtnfaProduct;
using ltlf_ek::NfaProduct;
using ltlf_ek::OtfMtdfaProduct;
using ltlf_ek::OutputLabeledTransducer;
using ltlf_ek::record_size_metric;
using ltlf_ek::register_turn_order_aps;
using ltlf_ek::Role;
using ltlf_ek::SizeMetric;
using ltlf_ek::size_metric_name;
using ltlf_ek::Synthesis;
using ltlf_ek::Transducer;
using ltlf_ek::trivial_transducer;
using ltlf_ek::VariablePartition;

using ltlf_ek::test_support::IoFreeVars;
using ltlf_ek::test_support::Phi;

// ---------------------------------------------------------------------------
// Shared helpers.
// ---------------------------------------------------------------------------

// The set of CANONICAL metric labels emitted into `report` -- i.e. entries
// with canonical == true, keyed by their size_metric_name(SizeMetric)
// string.  Free-form (canonical == false) entries are deliberately excluded:
// the B2 charge table is a claim about the canonical registry only.
std::set<std::string> CanonicalMetricLabels(const BenchReport& report) {
  std::set<std::string> labels;
  for (const BenchSizeMetric& m : report.metrics)
    if (m.canonical) labels.insert(m.label);
  return labels;
}

std::set<std::string> NamesOf(const std::vector<SizeMetric>& metrics) {
  std::set<std::string> names;
  for (SizeMetric m : metrics) names.insert(std::string(size_metric_name(m)));
  return names;
}

// ---------------------------------------------------------------------------
// Unit fixtures on the frozen sink itself (record_size_metric /
// BenchSizeMetric), independent of any of the five methods.
// ---------------------------------------------------------------------------

TEST(RecordSizeMetric, NoOpWithoutActiveScopeRecordsNothingAndDoesNotCrash) {
  // No BenchScope is constructed anywhere in this test: both overloads must
  // take the no-op path (the BenchTimer rule the PRD extends to metrics) and
  // must not crash.
  record_size_metric(SizeMetric::product_states, 7);
  record_size_metric(std::string("scratch"), 7);
  SUCCEED();
}

TEST(RecordSizeMetric, CanonicalCallAppendsACanonicalTrueEntryWithTheRegistryNameAndValue) {
  BenchScope scope;
  record_size_metric(SizeMetric::controller_states, 42);
  const BenchReport report = scope.report();
  ASSERT_EQ(report.metrics.size(), 1u);
  EXPECT_EQ(report.metrics[0].label,
           std::string(size_metric_name(SizeMetric::controller_states)));
  EXPECT_TRUE(report.metrics[0].canonical);
  EXPECT_EQ(report.metrics[0].value, 42u);
}

TEST(RecordSizeMetric, FreeFormCallAppendsACanonicalFalseEntryWithTheGivenLabelAndValue) {
  BenchScope scope;
  record_size_metric(std::string("some_free_form_count"), 9);
  const BenchReport report = scope.report();
  ASSERT_EQ(report.metrics.size(), 1u);
  EXPECT_EQ(report.metrics[0].label, "some_free_form_count");
  EXPECT_FALSE(report.metrics[0].canonical);
  EXPECT_EQ(report.metrics[0].value, 9u);
}

TEST(SizeMetricName, EveryRegistryValueHasADistinctNonEmptyName) {
  const std::vector<SizeMetric> all = {
      SizeMetric::goal_dfa_states,   SizeMetric::goal_nfa_states,
      SizeMetric::nfa_product_states, SizeMetric::product_states,
      SizeMetric::product_bdd_nodes,  SizeMetric::controller_states};
  std::set<std::string> names;
  for (SizeMetric m : all) {
    const std::string name(size_metric_name(m));
    EXPECT_FALSE(name.empty());
    names.insert(name);
  }
  EXPECT_EQ(names.size(), all.size())
      << "size_metric_name must be injective over the closed registry (PRD "
        "B2: 'a closed canonical registry, exactly like Stage')";
}

// ---------------------------------------------------------------------------
// O1 -- size-metric emission oracle (PRD "Test oracles" #1): for each of the
// five methods, the set of CANONICAL metrics emitted under a live BenchScope
// equals EXACTLY that method's row in the B2 charge table.  Set equality
// both directions: EXPECT_EQ on two std::set<std::string> fails on either a
// missing metric or an extra one, which is the whole point of the oracle.
// ---------------------------------------------------------------------------

TEST(BenchScopeSizeMetricEmission, DfaProductEmitsExactlyItsChargeTableRow) {
  const spot::bdd_dict_ptr dict = spot::make_bdd_dict();
  const VariablePartition vars = IoFreeVars();
  const OutputLabeledTransducer t_in =
      trivial_transducer(vars, Role::t_in, dict);
  const OutputLabeledTransducer t_out =
      trivial_transducer(vars, Role::t_out, dict);
  DfaProduct method;

  BenchReport report;
  {
    BenchScope scope;
    const std::optional<Controller> controller =
        method.synthesize(Phi("G(i -> o)"), vars, t_in, t_out);
    ASSERT_TRUE(controller.has_value())
        << "fixture must stay realizable so controller_states is charged too";
    report = scope.report();
  }

  const std::set<std::string> expected = NamesOf(
      {SizeMetric::goal_dfa_states, SizeMetric::product_states,
       SizeMetric::controller_states});
  EXPECT_EQ(CanonicalMetricLabels(report), expected)
      << "PRD B2 charge table, DfaProduct row: exactly {goal_dfa_states, "
        "product_states, controller_states} -- no extras, no omissions";
}

TEST(BenchScopeSizeMetricEmission, NfaProductEmitsExactlyItsChargeTableRow) {
#ifndef MONA_FOUND
  GTEST_SKIP() << "mona not found (CMake find_program(mona)); NfaProduct "
                  "needs it via ltlf_to_nfa";
#endif
  const spot::bdd_dict_ptr dict = spot::make_bdd_dict();
  const VariablePartition vars = IoFreeVars();
  const OutputLabeledTransducer t_in =
      trivial_transducer(vars, Role::t_in, dict);
  const OutputLabeledTransducer t_out =
      trivial_transducer(vars, Role::t_out, dict);
  NfaProduct method;

  BenchReport report;
  {
    BenchScope scope;
    const std::optional<Controller> controller =
        method.synthesize(Phi("G(i -> o)"), vars, t_in, t_out);
    ASSERT_TRUE(controller.has_value())
        << "fixture must stay realizable so controller_states is charged too";
    report = scope.report();
  }

  const std::set<std::string> expected =
      NamesOf({SizeMetric::goal_nfa_states, SizeMetric::nfa_product_states,
               SizeMetric::product_states, SizeMetric::controller_states});
  EXPECT_EQ(CanonicalMetricLabels(report), expected)
      << "PRD B2 charge table, NfaProduct row: exactly {goal_nfa_states, "
        "nfa_product_states, product_states, controller_states}";
}

TEST(BenchScopeSizeMetricEmission, MtdfaProductEmitsExactlyItsChargeTableRow) {
  const spot::bdd_dict_ptr dict = spot::make_bdd_dict();
  const VariablePartition vars = IoFreeVars();
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
        << "fixture must stay realizable so controller_states is charged too";
    report = scope.report();
  }

  const std::set<std::string> expected =
      NamesOf({SizeMetric::goal_dfa_states, SizeMetric::product_states,
               SizeMetric::product_bdd_nodes, SizeMetric::controller_states});
  EXPECT_EQ(CanonicalMetricLabels(report), expected)
      << "PRD B2 charge table, MtdfaProduct row: exactly {goal_dfa_states, "
        "product_states, product_bdd_nodes, controller_states}";
}

TEST(BenchScopeSizeMetricEmission, MtnfaProductEmitsExactlyItsChargeTableRow) {
#ifndef MONA_FOUND
  GTEST_SKIP() << "mona not found (CMake find_program(mona)); MtnfaProduct "
                  "needs it via ltlf_to_mtnfa";
#endif
  const spot::bdd_dict_ptr dict = spot::make_bdd_dict();
  const VariablePartition vars = IoFreeVars();
  register_turn_order_aps(vars, dict);
  const OutputLabeledTransducer t_in =
      trivial_transducer(vars, Role::t_in, dict);
  const OutputLabeledTransducer t_out =
      trivial_transducer(vars, Role::t_out, dict);
  MtnfaProduct method;

  BenchReport report;
  {
    BenchScope scope;
    const std::optional<Controller> controller =
        method.synthesize(Phi("G(i -> o)"), vars, t_in, t_out);
    ASSERT_TRUE(controller.has_value())
        << "fixture must stay realizable so controller_states is charged too";
    report = scope.report();
  }

  const std::set<std::string> expected =
      NamesOf({SizeMetric::goal_nfa_states, SizeMetric::product_states,
               SizeMetric::product_bdd_nodes, SizeMetric::controller_states});
  EXPECT_EQ(CanonicalMetricLabels(report), expected)
      << "PRD B2 charge table, MtnfaProduct row: exactly {goal_nfa_states, "
        "product_states, product_bdd_nodes, controller_states} -- no "
        "nfa_product_states (product and determinization are fused)";
}

TEST(BenchScopeSizeMetricEmission, OtfMtdfaProductEmitsExactlyItsChargeTableRowWithNoGoalRowAtAll) {
  const spot::bdd_dict_ptr dict = spot::make_bdd_dict();
  const VariablePartition vars = IoFreeVars();
  register_turn_order_aps(vars, dict);
  const OutputLabeledTransducer t_in =
      trivial_transducer(vars, Role::t_in, dict);
  const OutputLabeledTransducer t_out =
      trivial_transducer(vars, Role::t_out, dict);
  OtfMtdfaProduct method;

  BenchReport report;
  {
    BenchScope scope;
    const std::optional<Controller> controller =
        method.synthesize(Phi("G(i -> o)"), vars, t_in, t_out);
    ASSERT_TRUE(controller.has_value())
        << "fixture must stay realizable so controller_states is charged too";
    report = scope.report();
  }

  const std::set<std::string> labels = CanonicalMetricLabels(report);
  const std::set<std::string> expected =
      NamesOf({SizeMetric::product_states, SizeMetric::product_bdd_nodes,
               SizeMetric::controller_states});
  EXPECT_EQ(labels, expected)
      << "PRD B2 charge table, OtfMtdfaProduct row: exactly {product_states, "
        "product_bdd_nodes, controller_states}";

  // Edge case (PRD "Edge cases": "OtfMtdfaProduct has no Goal automaton...
  // it emits no goal_* metric at all"), asserted explicitly and not just via
  // the set-equality check above.
  EXPECT_FALSE(labels.count(std::string(size_metric_name(SizeMetric::goal_dfa_states))));
  EXPECT_FALSE(labels.count(std::string(size_metric_name(SizeMetric::goal_nfa_states))));
}

// ---------------------------------------------------------------------------
// Edge case: an unrealizable run emits no controller_states row at all --
// absent, never zero (PRD "Edge cases" / "B2 rule 1").  The metrics that ARE
// charged before the game is solved (goal_*, product_states, ...) stay
// present; only controller_states, which depends on synthesize() actually
// returning a Controller, is missing.
// ---------------------------------------------------------------------------

TEST(BenchScopeSizeMetricEmission, DfaProductUnrealizableRunOmitsControllerStatesRowEntirely) {
  const spot::bdd_dict_ptr dict = spot::make_bdd_dict();
  const VariablePartition vars = IoFreeVars();
  const OutputLabeledTransducer t_in =
      trivial_transducer(vars, Role::t_in, dict);
  const OutputLabeledTransducer t_out =
      trivial_transducer(vars, Role::t_out, dict);
  DfaProduct method;

  BenchReport report;
  {
    BenchScope scope;
    // "i" is unrealizable under IoFreeVars: i is a free input, the
    // environment can set it false (dfa_product_test.cpp
    // UnrealizableWhenGoalDependsOnFreeInput).
    const std::optional<Controller> controller =
        method.synthesize(Phi("i"), vars, t_in, t_out);
    ASSERT_FALSE(controller.has_value()) << "fixture must stay unrealizable";
    report = scope.report();
  }

  const std::set<std::string> labels = CanonicalMetricLabels(report);
  EXPECT_FALSE(
      labels.count(std::string(size_metric_name(SizeMetric::controller_states))))
      << "PRD 'Edge cases': an unrealizable run emits no controller_states "
        "row -- absent, never a zero value";
  const std::set<std::string> expected_without_controller =
      NamesOf({SizeMetric::goal_dfa_states, SizeMetric::product_states});
  EXPECT_EQ(labels, expected_without_controller)
      << "goal_dfa_states and product_states are charged before the game is "
        "solved, independent of the verdict";
}

TEST(BenchScopeSizeMetricEmission, OtfMtdfaProductUnrealizableRunOmitsControllerStatesRowEntirely) {
  const spot::bdd_dict_ptr dict = spot::make_bdd_dict();
  const VariablePartition vars = IoFreeVars();
  register_turn_order_aps(vars, dict);
  const OutputLabeledTransducer t_in =
      trivial_transducer(vars, Role::t_in, dict);
  const OutputLabeledTransducer t_out =
      trivial_transducer(vars, Role::t_out, dict);
  OtfMtdfaProduct method;

  BenchReport report;
  {
    BenchScope scope;
    // phi=0 collapses to the rejecting sink, unrealizable (precedent:
    // otf_mtdfa_product_test.cpp
    // FormulaZeroCollapsesToTheRejectingSinkAndIsUnrealizable).
    const std::optional<Controller> controller =
        method.synthesize(Phi("0"), vars, t_in, t_out);
    ASSERT_FALSE(controller.has_value()) << "fixture must stay unrealizable";
    report = scope.report();
  }

  const std::set<std::string> labels = CanonicalMetricLabels(report);
  EXPECT_FALSE(
      labels.count(std::string(size_metric_name(SizeMetric::controller_states))))
      << "PRD 'Edge cases': an unrealizable run emits no controller_states "
        "row -- absent, never a zero value";
  const std::set<std::string> expected_without_controller =
      NamesOf({SizeMetric::product_states, SizeMetric::product_bdd_nodes});
  EXPECT_EQ(labels, expected_without_controller)
      << "product_states / product_bdd_nodes are charged during the fused "
        "on-the-fly construction regardless of the verdict";
}

// ---------------------------------------------------------------------------
// O4 -- zero-perturbation oracle (PRD "Test oracles" #4): a run's
// realizability verdict and its emitted Controller are identical with and
// without an active BenchScope, now that a live scope also records size
// metrics.  Same comparison technique as tests/bench_test.cpp's
// ExpectZeroPerturbation (verdict has_value() equality, then byte-identical
// HOA serialization of the strategy) -- generalized to any Synthesis
// subject via a factory, rather than inventing a new controller-equality
// notion, per the PRD's "mirror the equivalent span-level invariant".
// ---------------------------------------------------------------------------

void ExpectZeroPerturbationOnMethod(
    const std::function<std::unique_ptr<Synthesis>()>& make_method,
    const spot::formula& phi, const VariablePartition& vars,
    const Transducer& t_in, const Transducer& t_out) {
  std::optional<Controller> without_scope;
  {
    std::unique_ptr<Synthesis> method = make_method();
    without_scope = method->synthesize(phi, vars, t_in, t_out);
  }

  std::optional<Controller> with_scope;
  {
    BenchScope scope;
    std::unique_ptr<Synthesis> method = make_method();
    with_scope = method->synthesize(phi, vars, t_in, t_out);
  }

  ASSERT_EQ(without_scope.has_value(), with_scope.has_value())
      << "an active BenchScope (now also recording size metrics) changed "
        "the realizability verdict";
  if (!without_scope) return;  // unrealizable: nothing further to compare.

  std::ostringstream a, b;
  spot::print_hoa(a, without_scope->strategy) << "\n";
  spot::print_hoa(b, with_scope->strategy) << "\n";
  EXPECT_EQ(a.str(), b.str())
      << "an active BenchScope changed the synthesized controller's HOA "
        "serialization (byte-identical invariant violated)";
}

TEST(BenchScopeZeroPerturbation, DfaProductVerdictAndControllerUnchangedByAnActiveScope) {
  const spot::bdd_dict_ptr dict = spot::make_bdd_dict();
  const VariablePartition vars = IoFreeVars();
  const OutputLabeledTransducer t_in =
      trivial_transducer(vars, Role::t_in, dict);
  const OutputLabeledTransducer t_out =
      trivial_transducer(vars, Role::t_out, dict);
  ExpectZeroPerturbationOnMethod([] { return std::make_unique<DfaProduct>(); },
                                Phi("G(i -> o)"), vars, t_in, t_out);
}

TEST(BenchScopeZeroPerturbation, NfaProductVerdictAndControllerUnchangedByAnActiveScope) {
#ifndef MONA_FOUND
  GTEST_SKIP() << "mona not found (CMake find_program(mona)); NfaProduct "
                  "needs it via ltlf_to_nfa";
#endif
  const spot::bdd_dict_ptr dict = spot::make_bdd_dict();
  const VariablePartition vars = IoFreeVars();
  const OutputLabeledTransducer t_in =
      trivial_transducer(vars, Role::t_in, dict);
  const OutputLabeledTransducer t_out =
      trivial_transducer(vars, Role::t_out, dict);
  ExpectZeroPerturbationOnMethod([] { return std::make_unique<NfaProduct>(); },
                                Phi("G(i -> o)"), vars, t_in, t_out);
}

TEST(BenchScopeZeroPerturbation, MtdfaProductVerdictAndControllerUnchangedByAnActiveScope) {
  const spot::bdd_dict_ptr dict = spot::make_bdd_dict();
  const VariablePartition vars = IoFreeVars();
  register_turn_order_aps(vars, dict);
  const OutputLabeledTransducer t_in =
      trivial_transducer(vars, Role::t_in, dict);
  const OutputLabeledTransducer t_out =
      trivial_transducer(vars, Role::t_out, dict);
  ExpectZeroPerturbationOnMethod(
      [] { return std::make_unique<MtdfaProduct>(); }, Phi("G(i -> o)"), vars,
      t_in, t_out);
}

TEST(BenchScopeZeroPerturbation, MtnfaProductVerdictAndControllerUnchangedByAnActiveScope) {
#ifndef MONA_FOUND
  GTEST_SKIP() << "mona not found (CMake find_program(mona)); MtnfaProduct "
                  "needs it via ltlf_to_mtnfa";
#endif
  const spot::bdd_dict_ptr dict = spot::make_bdd_dict();
  const VariablePartition vars = IoFreeVars();
  register_turn_order_aps(vars, dict);
  const OutputLabeledTransducer t_in =
      trivial_transducer(vars, Role::t_in, dict);
  const OutputLabeledTransducer t_out =
      trivial_transducer(vars, Role::t_out, dict);
  ExpectZeroPerturbationOnMethod(
      [] { return std::make_unique<MtnfaProduct>(); }, Phi("G(i -> o)"), vars,
      t_in, t_out);
}

TEST(BenchScopeZeroPerturbation, OtfMtdfaProductVerdictAndControllerUnchangedByAnActiveScope) {
  const spot::bdd_dict_ptr dict = spot::make_bdd_dict();
  const VariablePartition vars = IoFreeVars();
  register_turn_order_aps(vars, dict);
  const OutputLabeledTransducer t_in =
      trivial_transducer(vars, Role::t_in, dict);
  const OutputLabeledTransducer t_out =
      trivial_transducer(vars, Role::t_out, dict);
  ExpectZeroPerturbationOnMethod(
      [] { return std::make_unique<OtfMtdfaProduct>(); }, Phi("G(i -> o)"),
      vars, t_in, t_out);
}

}  // namespace
