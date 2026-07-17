#include <optional>
#include <string>
#include <vector>

#include <gtest/gtest.h>
#include <spot/tl/parse.hh>
#include <spot/twa/bdddict.hh>

#include "ltlf_ek/bench.hpp"
#include "ltlf_ek/nfa_product.hpp"
#include "ltlf_ek/output_labeled_transducer.hpp"
#include "ltlf_ek/role.hpp"
#include "ltlf_ek/variables.hpp"

#include "support/fixtures.hpp"

// Benchmark-report shape for NfaProduct (docs/prd/nfa-product.md "Test
// oracles" #4 / "Assembled synthesize body"): automaton_construction,
// product_construction (with a nested free-form "determinize" sub-span),
// game_solving -- mirrors tests/mtdfa_bench_test.cpp's
// BenchScopeIntegration.MtdfaProductEmitsCanonicalStagesOnceEachInOrder for
// the bench-shape pattern, but the extra assertion here is that
// "determinize" nests INSIDE product_construction (src/nfa_product.cpp's
// BenchTimer sub("determinize") is opened while product_construction's own
// BenchTimer is still alive), unlike MtdfaProduct's minimize_mtdfa span
// (its own top-level root).
//
// Timing is non-deterministic (bench_test.cpp precedent): every assertion
// below is on span structure/labels/nesting/ordering, never absolute
// duration.
//
// NfaProduct::synthesize always shells out to mona (via ltlf_to_nfa ->
// detail::past_ltlf_to_dfa), so every TEST here is MONA_FOUND-gated
// (docs/prd/nfa-product.md "Edge cases" "MONA absent").
namespace {

using ltlf_ek::BenchReport;
using ltlf_ek::BenchScope;
using ltlf_ek::Controller;
using ltlf_ek::NfaProduct;
using ltlf_ek::OutputLabeledTransducer;
using ltlf_ek::Role;
using ltlf_ek::Stage;
using ltlf_ek::stage_name;
using ltlf_ek::trivial_transducer;
using ltlf_ek::VariablePartition;

using ltlf_ek::test_support::IoFreeVars;
using ltlf_ek::test_support::Phi;

TEST(BenchScopeIntegration,
    NfaProductEmitsCanonicalStagesOnceEachInOrderWithNestedDeterminizeSpan) {
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
        << "fixture must stay realizable for the integration oracle to mean "
          "anything";
    report = scope.report();
  }

  ASSERT_EQ(report.roots.size(), 3u)
      << "expected exactly automaton_construction, product_construction, "
        "game_solving -- nfa_to_dfa's determinize work must land inside "
        "product_construction (as a nested sub-span), not spill a fourth "
        "root";
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

  // The "determinize" sub-span nests INSIDE product_construction (PRD
  // "Assembled synthesize body": "the 'determinize' sub-span nests inside
  // product_construction") -- a single free-form (canonical:false) child,
  // not a fourth root.
  const auto& product_span = report.roots[1];
  ASSERT_EQ(product_span.children.size(), 1u)
      << "product_construction must have exactly one child span "
        "(\"determinize\")";
  EXPECT_EQ(product_span.children[0].label, "determinize");
  EXPECT_FALSE(product_span.children[0].canonical);
  // automaton_construction and game_solving open no sub-spans of their own.
  EXPECT_TRUE(report.roots[0].children.empty());
  EXPECT_TRUE(report.roots[2].children.empty());
}

}  // namespace
