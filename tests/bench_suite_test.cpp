#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "ltlf_ek/bench.hpp"
#include "ltlf_ek/bench_suite.hpp"
#include "ltlf_ek/dependent_inputs.hpp"
#include "ltlf_ek/output_labeled_transducer.hpp"
#include "ltlf_ek/variables.hpp"

// Tests for docs/prd/benchmark-suite.md Phase 2 -- the registry
// (bench_suite.hpp/.cpp): ComparabilityTier / BenchParams / BenchCase /
// BenchFamily / BenchSubject / bench_families() / bench_subjects() /
// BenchRow / run_bench_case, bound to the code as it exists on this branch
// (not the PRD's tentative "Interfaces & types" block -- Phase 2's freeze
// confidence is "tentative", and the developer's "Developer comments" record
// the one sharpening that matters here: BenchCase::t_in/t_out are the
// concrete OutputLabeledTransducer).
//
// "Test oracles" covered: O2 (discrimination -- the family validity check,
// asserted here on structural SHAPE across a small sweep, not the exact
// committed integers Phase 3 owns), O5 (cross-method agreement), O6
// (determinism / B5), O7 (tier declaration). Plus small unit fixtures:
// comparability_tier_name round-trip, the n-floor rejection ("Edge cases"),
// run_bench_case's one-BenchScope-per-case invariant, and the absent-never-
// zero rule re-pinned through this registry (Phase 1's tests/
// bench_size_metric_test.cpp already pins it through the five methods
// directly; this file pins it through bench_families()/bench_subjects()
// instead, since that is the surface Phase 2 actually adds).
//
// No test asserts a timing ratio (PRD B1/"Green checkpoint": "No test
// asserts a timing ratio") -- assertions below on a Stage/span row (e.g. the
// free-form `dependent_inputs` span the BenchSuiteExtractorColumn tests read)
// check only that the row is PRESENT, never its duration; every other
// assertion reads BenchRow entries keyed by a canonical SizeMetric name.
namespace {

using ltlf_ek::BenchCase;
using ltlf_ek::BenchFamily;
using ltlf_ek::BenchParams;
using ltlf_ek::BenchRow;
using ltlf_ek::BenchScope;
using ltlf_ek::BenchSubject;
using ltlf_ek::bench_families;
using ltlf_ek::bench_scope_active;
using ltlf_ek::bench_subjects;
using ltlf_ek::comparability_tier_name;
using ltlf_ek::ComparabilityTier;
using ltlf_ek::run_bench_case;
using ltlf_ek::SizeMetric;
using ltlf_ek::size_metric_name;

// ---------------------------------------------------------------------------
// Shared lookup / measurement helpers.
// ---------------------------------------------------------------------------

const BenchFamily& FamilyNamed(const std::string& name) {
  for (const auto& f : bench_families())
    if (f->name() == name) return *f;
  throw std::runtime_error("bench_suite_test: no family named '" + name +
                           "'");
}

const BenchSubject& SubjectNamed(const std::string& name) {
  for (const auto& s : bench_subjects())
    if (s->name() == name) return *s;
  throw std::runtime_error("bench_suite_test: no subject named '" + name +
                           "'");
}

BenchParams Params(std::int64_t n, bool realizable) {
  return BenchParams{{"n", n}, {"realizable", realizable ? 1 : 0}};
}

std::optional<std::uint64_t> RowValue(const std::vector<BenchRow>& rows,
                                      const std::string& key) {
  for (const BenchRow& r : rows)
    if (r.key == key) return r.value;
  return std::nullopt;
}

std::string MetricKey(SizeMetric m) { return std::string(size_metric_name(m)); }

// The closed SizeMetric registry's name set -- used to filter
// run_bench_case's rows down to the structural (size) rows, excluding the
// Stage/span duration rows that share the same BenchRow shape (PRD "the row
// key is generic") but are inherently nondeterministic nanosecond counts,
// never a legitimate discrimination or determinism target.
std::set<std::string> CanonicalSizeMetricNames() {
  const std::vector<SizeMetric> all = {
      SizeMetric::goal_dfa_states,   SizeMetric::goal_nfa_states,
      SizeMetric::goal_mtdfa_roots,  SizeMetric::nfa_product_states,
      SizeMetric::product_states,    SizeMetric::product_mtdfa_roots,
      SizeMetric::product_bdd_nodes, SizeMetric::controller_states};
  std::set<std::string> names;
  for (SizeMetric m : all) names.insert(MetricKey(m));
  return names;
}

std::map<std::string, std::uint64_t> StructuralRows(
    const std::vector<BenchRow>& rows) {
  static const std::set<std::string> kStructural = CanonicalSizeMetricNames();
  std::map<std::string, std::uint64_t> out;
  for (const BenchRow& r : rows)
    if (kStructural.count(r.key)) out.emplace(r.key, r.value);
  return out;
}

// nullopt => the subject skipped this case entirely (rows empty -- e.g. a
// MONA-dependent subject with mona absent, PRD "Edge cases"); otherwise the
// realizability verdict read off controller_states' presence (B2 rule 1 /
// charge table: every method charges controller_states iff synthesize()
// actually returned a Controller).
std::optional<bool> RealizabilityVerdict(const std::vector<BenchRow>& rows) {
  if (rows.empty()) return std::nullopt;
  return RowValue(rows, MetricKey(SizeMetric::controller_states)).has_value();
}

// Collects metric_key's value across `ns` for (family_name, subject_name),
// realizable held fixed. A missing row is a fatal assertion (ASSERT_TRUE in
// a void-returning helper), not a silent 0 -- a hole here means either
// Stop-list item 2 territory (the family stopped emitting the metric it
// claims) or a genuine test-authoring mistake, and either way the sweep must
// not continue computing a shape claim over a partially-missing series.
void CollectMetricSeries(const std::string& family_name,
                         const std::string& subject_name,
                         SizeMetric metric, const std::vector<std::int64_t>& ns,
                         bool realizable, std::vector<std::uint64_t>* out) {
  const BenchFamily& family = FamilyNamed(family_name);
  const BenchSubject& subject = SubjectNamed(subject_name);
  const std::string key = MetricKey(metric);
  out->clear();
  for (std::int64_t n : ns) {
    const BenchCase c = family.instantiate(Params(n, realizable));
    const std::vector<BenchRow> rows = run_bench_case(c, subject);
    const std::optional<std::uint64_t> v = RowValue(rows, key);
    ASSERT_TRUE(v.has_value())
        << "family=" << family_name << " subject=" << subject_name
        << " n=" << n << " metric=" << key << ": row missing";
    out->push_back(*v);
  }
}

// Linear-growth check: first differences constant across the whole series
// (formula-agnostic -- does not assume a particular closed form like n+1).
void ExpectFirstDifferencesConstant(const std::vector<std::uint64_t>& v,
                                    const std::string& what) {
  ASSERT_GE(v.size(), 3u)
      << what << ": need >=3 points to characterize linear growth";
  const std::int64_t d0 =
      static_cast<std::int64_t>(v[1]) - static_cast<std::int64_t>(v[0]);
  for (std::size_t i = 2; i < v.size(); ++i) {
    const std::int64_t di =
        static_cast<std::int64_t>(v[i]) - static_cast<std::int64_t>(v[i - 1]);
    EXPECT_EQ(di, d0) << what << ": expected constant first differences "
                                "(linear growth in n); diffs[0]=" << d0
                      << " diffs[" << (i - 1) << "]=" << di;
  }
}

double RangeRatio(const std::vector<std::uint64_t>& v) {
  return v.front() == 0 ? 0.0 : double(v.back()) / double(v.front());
}

}  // namespace

// ---------------------------------------------------------------------------
// Unit: comparability_tier_name round-trip.
// ---------------------------------------------------------------------------

TEST(ComparabilityTierName, RoundTripsForAllThreeTiers) {
  EXPECT_EQ(comparability_tier_name(ComparabilityTier::t1), "t1");
  EXPECT_EQ(comparability_tier_name(ComparabilityTier::t2), "t2");
  EXPECT_EQ(comparability_tier_name(ComparabilityTier::t3), "t3");
}

TEST(ComparabilityTierName, RejectsAnOutOfRangeValue) {
  EXPECT_THROW(comparability_tier_name(static_cast<ComparabilityTier>(99)),
              std::invalid_argument);
}

// ---------------------------------------------------------------------------
// Unit: the sweep floor ("Edge cases": "n=0/n=1 degenerate ... a family must
// reject a parameter below its own floor rather than emit a malformed
// formula").
// ---------------------------------------------------------------------------

TEST(BenchFamilyNFloor, EveryFamilyRejectsNBelowTheSweepFloorRatherThanInstantiating) {
  for (const auto& family : bench_families()) {
    SCOPED_TRACE("family=" + family->name());
    EXPECT_THROW(family->instantiate(Params(0, true)), std::invalid_argument)
        << "n=0 degenerates: X[!]^0 == phi itself";
    EXPECT_THROW(family->instantiate(Params(0, false)), std::invalid_argument);
    EXPECT_THROW(family->instantiate(Params(1, true)), std::invalid_argument);
    EXPECT_THROW(family->sweep(1, 3), std::invalid_argument)
        << "sweep(n_min below the floor) must also reject, not silently "
          "clamp to a valid range";
  }
}

// ---------------------------------------------------------------------------
// Unit: run_bench_case opens exactly one BenchScope per case.
// ---------------------------------------------------------------------------

TEST(BenchSuiteRunBenchCase, OpensExactlyOneBenchScopeAndClosesItBeforeReturning) {
  EXPECT_FALSE(bench_scope_active());
  const BenchCase c = FamilyNamed("cons-inert").instantiate(Params(2, true));
  const std::vector<BenchRow> rows =
      run_bench_case(c, SubjectNamed("dfa-product"));
  EXPECT_FALSE(rows.empty());
  EXPECT_FALSE(bench_scope_active())
      << "run_bench_case must not leave its BenchScope installed after "
        "returning";
}

TEST(BenchSuiteRunBenchCaseDeathTest, UnderAnAlreadyActiveScopeAsserts) {
  // "Nested BenchScope. Already forbidden (asserts) by the shipped
  // infrastructure; the runner must open exactly one per case" (PRD "Edge
  // cases"): run_bench_case's own `BenchScope scope;` hits the shipped
  // nested-install assert when a BenchScope is already active on this
  // thread -- the same mechanism tests/bench_test.cpp's BenchScopeDeathTest
  // pins directly on BenchScope; this pins it through run_bench_case.
  const BenchCase c = FamilyNamed("cons-inert").instantiate(Params(2, true));
  const BenchSubject& subject = SubjectNamed("dfa-product");
  BenchScope outer;
  EXPECT_DEATH({ run_bench_case(c, subject); }, "");
}

// ---------------------------------------------------------------------------
// Edge case: absent-never-zero, re-pinned through the Phase 2 registry
// (Phase 1's tests/bench_size_metric_test.cpp pins the same rule directly
// against the five methods; this pins it through bench_families()/
// bench_subjects()/run_bench_case, the surface Phase 2 actually adds).
// ---------------------------------------------------------------------------

TEST(BenchSuiteAbsentNeverZero, OtfMtdfaProductEmitsNoGoalMetricAtAllViaTheRegistry) {
  const BenchCase c = FamilyNamed("cons-prunes").instantiate(Params(2, true));
  ASSERT_TRUE(c.expected_realizable);
  const std::vector<BenchRow> rows =
      run_bench_case(c, SubjectNamed("otf-mtdfa-product"));

  EXPECT_FALSE(RowValue(rows, MetricKey(SizeMetric::goal_dfa_states)))
      << "OtfMtdfaProduct builds no Goal automaton at all (PRD 'Edge cases')";
  EXPECT_FALSE(RowValue(rows, MetricKey(SizeMetric::goal_nfa_states)));
  EXPECT_FALSE(RowValue(rows, MetricKey(SizeMetric::goal_mtdfa_roots)));
  EXPECT_TRUE(RowValue(rows, MetricKey(SizeMetric::product_mtdfa_roots)));
  EXPECT_TRUE(RowValue(rows, MetricKey(SizeMetric::product_bdd_nodes)));
  EXPECT_TRUE(RowValue(rows, MetricKey(SizeMetric::controller_states)))
      << "case is realizable, so controller_states must still be charged";
}

TEST(BenchSuiteAbsentNeverZero, UnrealizableCaseEmitsNoControllerStatesRowViaTheRegistry) {
  const BenchCase c = FamilyNamed("cons-prunes").instantiate(Params(2, false));
  ASSERT_FALSE(c.expected_realizable);
  const std::vector<BenchRow> rows =
      run_bench_case(c, SubjectNamed("dfa-product"));

  EXPECT_FALSE(RowValue(rows, MetricKey(SizeMetric::controller_states)))
      << "unrealizable: controller_states must be ABSENT, never a zero value";
  EXPECT_TRUE(RowValue(rows, MetricKey(SizeMetric::goal_dfa_states)))
      << "goal_dfa_states is charged before the game is solved, independent "
        "of the verdict";
  EXPECT_TRUE(RowValue(rows, MetricKey(SizeMetric::product_states)));
}

// ---------------------------------------------------------------------------
// Edge case: the two MONA-dependent subjects skip cleanly when mona is
// absent, never fail, never emit a zero row -- the MONA_FOUND ctest gate's
// precedent, applied to the Phase 2 registry. Mona IS present on this box
// (task context), so the #ifdef branch below is the one actually exercised
// here; the #else branch documents and would exercise the required skip
// behaviour on a clean box, matching tests/bench_size_metric_test.cpp's
// #ifndef MONA_FOUND / GTEST_SKIP() pattern for the same two methods.
// ---------------------------------------------------------------------------

TEST(BenchSuiteMonaGate, NfaProductSubjectSkipsCleanlyWhenMonaAbsentAndRunsWhenPresent) {
  const BenchCase c = FamilyNamed("cons-inert").instantiate(Params(2, true));
  const std::vector<BenchRow> rows =
      run_bench_case(c, SubjectNamed("nfa-product"));
#ifdef MONA_FOUND
  EXPECT_FALSE(rows.empty())
      << "mona is present on this build (MONA_FOUND); nfa-product must "
        "actually run and emit rows, not skip";
  EXPECT_TRUE(RowValue(rows, MetricKey(SizeMetric::controller_states)));
#else
  EXPECT_TRUE(rows.empty())
      << "mona absent (no MONA_FOUND): nfa-product must skip cleanly -- "
        "empty rows, never a failure, never a zero";
#endif
}

TEST(BenchSuiteMonaGate, MtnfaProductSubjectSkipsCleanlyWhenMonaAbsentAndRunsWhenPresent) {
  const BenchCase c = FamilyNamed("cons-inert").instantiate(Params(2, true));
  const std::vector<BenchRow> rows =
      run_bench_case(c, SubjectNamed("mtnfa-product"));
#ifdef MONA_FOUND
  EXPECT_FALSE(rows.empty())
      << "mona is present on this build (MONA_FOUND); mtnfa-product must "
        "actually run and emit rows, not skip";
  EXPECT_TRUE(RowValue(rows, MetricKey(SizeMetric::controller_states)));
#else
  EXPECT_TRUE(rows.empty())
      << "mona absent (no MONA_FOUND): mtnfa-product must skip cleanly -- "
        "empty rows, never a failure, never a zero";
#endif
}

// ---------------------------------------------------------------------------
// O2 -- discrimination oracle (PRD "Test oracles" #2), the primary family
// validity check: structural SHAPE, not the exact committed integers (that
// exactness is Phase 3's BenchStructural, against a committed baseline).
// Kept to a small n range (cheap): the shape is already visible by n=6.
// ---------------------------------------------------------------------------

TEST(BenchSuiteDiscrimination, ConsPrunesGoalIsExponentialWhileProductIsLinear) {
  const std::vector<std::int64_t> ns = {2, 3, 4, 5, 6};
  std::vector<std::uint64_t> goal, product;
  ASSERT_NO_FATAL_FAILURE(CollectMetricSeries(
      "cons-prunes", "dfa-product", SizeMetric::goal_dfa_states, ns, true,
      &goal));
  ASSERT_NO_FATAL_FAILURE(CollectMetricSeries(
      "cons-prunes", "dfa-product", SizeMetric::product_states, ns, true,
      &product));

  ExpectFirstDifferencesConstant(product, "cons-prunes product_states");

  const double goal_ratio = RangeRatio(goal);
  const double product_ratio = RangeRatio(product);
  EXPECT_GT(goal_ratio, 3.0)
      << "cons-prunes goal_dfa_states must grow far faster than linear over "
        "n=2..6; measured range ratio=" << goal_ratio;
  EXPECT_GT(goal_ratio, product_ratio * 2.0)
      << "goal_dfa_states' growth must dominate product_states' (B4: 'Goal "
        "is 2^n, product is n+1'); goal_ratio=" << goal_ratio
      << " product_ratio=" << product_ratio;
}

TEST(BenchSuiteDiscrimination, ConsInertProductStatesAtLeastGoalDfaStatesAtEveryN) {
  const std::vector<std::int64_t> ns = {2, 3, 4, 5, 6};
  std::vector<std::uint64_t> goal, product;
  ASSERT_NO_FATAL_FAILURE(CollectMetricSeries(
      "cons-inert", "dfa-product", SizeMetric::goal_dfa_states, ns, true,
      &goal));
  ASSERT_NO_FATAL_FAILURE(CollectMetricSeries(
      "cons-inert", "dfa-product", SizeMetric::product_states, ns, true,
      &product));

  for (std::size_t i = 0; i < ns.size(); ++i) {
    SCOPED_TRACE("n=" + std::to_string(ns[i]));
    EXPECT_GE(product[i], goal[i])
        << "cons-inert: cons prunes nothing, so product_states must be >= "
          "goal_dfa_states (B4/'Test oracles' #2)";
  }
}

// The knowledge-size axis. Every other family holds |T_in| at 1, so
// product_states <= goal_dfa_states by construction and nothing here ever
// measures what large knowledge costs. These two vary |T_in| = n instead.
TEST(BenchSuiteDiscrimination, KnowledgeChainInertProductIsExactlyNTimesTheGoal) {
  const std::vector<std::int64_t> ns = {2, 3, 4, 5, 6};
  std::vector<std::uint64_t> goal, product;
  ASSERT_NO_FATAL_FAILURE(CollectMetricSeries(
      "knowledge-chain-inert", "dfa-product", SizeMetric::goal_dfa_states, ns,
      true, &goal));
  ASSERT_NO_FATAL_FAILURE(CollectMetricSeries(
      "knowledge-chain-inert", "dfa-product", SizeMetric::product_states, ns,
      true, &product));

  for (std::size_t i = 0; i < ns.size(); ++i) {
    SCOPED_TRACE("n=" + std::to_string(ns[i]));
    // phi never mentions the known variable the chain constrains, so cons has
    // nothing to cut and the product is the plain |T_in| x |goal|.
    EXPECT_EQ(product[i], goal[i] * static_cast<std::uint64_t>(ns[i]))
        << "knowledge-chain-inert must multiply: product_states should be "
           "exactly n * goal_dfa_states with n-state, non-pruning knowledge";
  }
}

TEST(BenchSuiteDiscrimination, KnowledgeChainProductStaysLinearBecauseConsPrunes) {
  const std::vector<std::int64_t> ns = {2, 3, 4, 5, 6};
  std::vector<std::uint64_t> goal, product;
  ASSERT_NO_FATAL_FAILURE(CollectMetricSeries(
      "knowledge-chain", "dfa-product", SizeMetric::goal_dfa_states, ns, true,
      &goal));
  ASSERT_NO_FATAL_FAILURE(CollectMetricSeries(
      "knowledge-chain", "dfa-product", SizeMetric::product_states, ns, true,
      &product));

  // Matched with knowledge-chain-inert: same n-state knowledge, but here phi
  // is about the known variable, so cons collapses the goal's 2^n instead.
  ExpectFirstDifferencesConstant(product, "knowledge-chain product_states");
  EXPECT_GT(RangeRatio(goal), RangeRatio(product) * 2.0)
      << "knowledge-chain: cons must still dominate |T_in| growth";
}

TEST(BenchSuiteDiscrimination, MirrorSmallGoalNfaStatesIsLinearWhileMtdfaRootsAreExponential) {
#ifndef MONA_FOUND
  GTEST_SKIP() << "mona not found (CMake find_program(mona)); mirror-small's "
                  "goal_nfa_states side needs it via ltlf_to_nfa";
#endif
  const std::vector<std::int64_t> ns = {2, 3, 4, 5, 6};
  std::vector<std::uint64_t> nfa_states, mtdfa_roots;
  ASSERT_NO_FATAL_FAILURE(CollectMetricSeries(
      "mirror-small", "nfa-product", SizeMetric::goal_nfa_states, ns, true,
      &nfa_states));
  ASSERT_NO_FATAL_FAILURE(CollectMetricSeries(
      "mirror-small", "mtdfa-product", SizeMetric::goal_mtdfa_roots, ns, true,
      &mtdfa_roots));

  ExpectFirstDifferencesConstant(nfa_states, "mirror-small goal_nfa_states");

  const double nfa_ratio = RangeRatio(nfa_states);
  const double mtdfa_ratio = RangeRatio(mtdfa_roots);
  EXPECT_GT(mtdfa_ratio, 3.0)
      << "mirror-small's mtdfa route must grow far faster than linear over "
        "n=2..6; measured range ratio=" << mtdfa_ratio;
  EXPECT_GT(mtdfa_ratio, nfa_ratio * 3.0)
      << "the mtdfa route's growth must dominate the NFA route's (B4: "
        "'Goal NFA is n+3 while the mtdfa is 2^n'); mtdfa_ratio="
      << mtdfa_ratio << " nfa_ratio=" << nfa_ratio;
}

TEST(BenchSuiteDiscrimination, MirrorDegenerateGoalNfaStatesIsSameOrderAsGoalDfaStates) {
#ifndef MONA_FOUND
  GTEST_SKIP() << "mona not found (CMake find_program(mona)); "
                  "mirror-degenerate's NFA side needs it via ltlf_to_nfa";
#endif
  const std::vector<std::int64_t> ns = {2, 3, 4, 5, 6};
  std::vector<std::uint64_t> nfa_states, dfa_states;
  ASSERT_NO_FATAL_FAILURE(CollectMetricSeries(
      "mirror-degenerate", "nfa-product", SizeMetric::goal_nfa_states, ns,
      true, &nfa_states));
  ASSERT_NO_FATAL_FAILURE(CollectMetricSeries(
      "mirror-degenerate", "dfa-product", SizeMetric::goal_dfa_states, ns,
      true, &dfa_states));

  // The documented trap (B4 "mirror-degenerate"): ltlf_to_nfa is
  // mirror-based, so this family's NFA is the SAME ORDER as its DFA --
  // deliberately asserted, not treated as a defect. "Same order": both grow
  // well past linear over the sweep, and the two range ratios sit within a
  // generous constant factor of each other (unlike mirror-small, whose NFA
  // route stays flat/linear against the same n range -- see
  // MirrorSmallGoalNfaStatesIsLinearWhileMtdfaRootsAreExponential above).
  const double nfa_ratio = RangeRatio(nfa_states);
  const double dfa_ratio = RangeRatio(dfa_states);
  EXPECT_GT(nfa_ratio, 3.0)
      << "mirror-degenerate's goal_nfa_states must NOT stay flat/linear like "
        "mirror-small's -- that is exactly the trap this family exists to "
        "preserve; measured range ratio=" << nfa_ratio;
  EXPECT_GE(nfa_ratio, dfa_ratio / 5.0)
      << "nfa_ratio=" << nfa_ratio << " dfa_ratio=" << dfa_ratio
      << ": expected the same order of growth (within a factor of 5)";
  EXPECT_LE(nfa_ratio, dfa_ratio * 5.0)
      << "nfa_ratio=" << nfa_ratio << " dfa_ratio=" << dfa_ratio
      << ": expected the same order of growth (within a factor of 5)";
}

// ---------------------------------------------------------------------------
// O5 -- cross-method agreement (PRD "Test oracles" #5): all five methods
// agree on realizability for every case, and each agrees with the family's
// declared expected_realizable. Reuses the existing technique (compare
// Synthesis::synthesize(...).has_value() across methods, as tests/
// mtnfa_product_test.cpp / bench_size_metric_test.cpp already do) rather
// than inventing a new one -- here read off run_bench_case's
// controller_states presence, which Phase 1's charge-table oracle already
// established is exactly synthesize()'s has_value() (RealizabilityVerdict).
// ---------------------------------------------------------------------------

TEST(BenchSuiteCrossMethodAgreement, AllFiveMethodsAgreeWithEachOtherAndTheDeclaredVerdict) {
  const std::vector<std::int64_t> ns = {2, 3};
  int total_cases = 0;
  int total_subject_runs = 0;
  for (const auto& family : bench_families()) {
    for (std::int64_t n : ns) {
      for (bool realizable : {true, false}) {
        SCOPED_TRACE("family=" + family->name() + " n=" + std::to_string(n) +
                    " realizable=" + std::to_string(realizable));
        const BenchCase c = family->instantiate(Params(n, realizable));
        ++total_cases;
        std::optional<bool> reference;
        for (const auto& subject : bench_subjects()) {
          // One measured-infeasible cell, excluded by construction (the same
          // exclusion tests/slippery_world_test.cpp documents at length):
          // NfaProduct on `slippery-onehot` at n = 3 needs ~13 min per goal
          // against < 1 s for every other (subject, family, n) cell here.
          // PRD engineered-domain-families.md Stop-list 8 -- a reportable
          // data point about the one-hot arm, not a bug to tune around.
          if ((subject->name() == "nfa-product" ||
               subject->name() == "nfa-product-nk") &&
              family->name() == "slippery-onehot" && n >= 3) {
            continue;
          }
          // dependent-inputs-extraction is a cost-measuring subject (it times
          // dependent_inputs and never synthesizes), so it never emits a
          // controller_states row and has no realizability verdict at all.
          // Feeding it to this cross-method *verdict*-agreement oracle is a
          // category error, not a disagreement -- exclude it here.
          if (subject->name() == "dependent-inputs-extraction") {
            continue;
          }
          const std::vector<BenchRow> rows = run_bench_case(c, *subject);
          const std::optional<bool> verdict = RealizabilityVerdict(rows);
          if (!verdict.has_value()) continue;  // subject skipped (mona absent).
          ++total_subject_runs;
          if (!reference.has_value()) {
            reference = verdict;
          } else {
            EXPECT_EQ(*verdict, *reference)
                << "subject=" << subject->name()
                << " disagrees with an earlier method on this case (O5)";
          }
        }
        ASSERT_TRUE(reference.has_value())
            << "every case must have at least one subject that actually ran";
        EXPECT_EQ(*reference, c.expected_realizable)
            << "the agreed verdict disagrees with the family's declared "
              "expected_realizable";
      }
    }
  }
  RecordProperty("cross_method_agreement_cases", total_cases);
  RecordProperty("cross_method_agreement_subject_runs", total_subject_runs);
}

// ---------------------------------------------------------------------------
// O6 -- determinism (PRD "Test oracles" #6, pins B5: "nothing in the suite
// draws random numbers"). Running the identical (BenchCase, BenchSubject)
// twice in one process must yield identical STRUCTURAL rows -- span/timing
// rows are excluded (StructuralRows), since a duration is never a legitimate
// determinism target (nondeterministic by construction, PRD B1).
// ---------------------------------------------------------------------------

TEST(BenchSuiteDeterminism, SameCaseRunTwiceInProcessYieldsIdenticalStructuralRows) {
  const std::vector<std::pair<std::string, std::string>> combos = {
      {"cons-prunes", "dfa-product"},
      {"cons-inert", "mtdfa-product"},
      {"mirror-small", "otf-mtdfa-product"},
      {"mirror-degenerate", "mtdfa-product"},
      {"parity-t3", "dfa-product"},
  };
  for (const auto& combo : combos) {
    const std::string& family_name = combo.first;
    const std::string& subject_name = combo.second;
    SCOPED_TRACE("family=" + family_name + " subject=" + subject_name);
    const BenchFamily& family = FamilyNamed(family_name);
    const BenchSubject& subject = SubjectNamed(subject_name);
    const BenchCase c = family.instantiate(Params(3, true));

    const std::map<std::string, std::uint64_t> first =
        StructuralRows(run_bench_case(c, subject));
    const std::map<std::string, std::uint64_t> second =
        StructuralRows(run_bench_case(c, subject));

    EXPECT_FALSE(first.empty())
        << "sanity: the case must actually emit structural rows";
    EXPECT_EQ(first, second)
        << "running the identical (BenchCase, BenchSubject) twice in one "
          "process must yield byte-identical structural rows (PRD B5)";
  }
}

// ---------------------------------------------------------------------------
// O7 -- tier declaration (PRD "Test oracles" #7): every T1 family carries a
// non-empty psi_in; no other tier does. Mechanical form of "the tier is
// declared, never sniffed" (B3).
// ---------------------------------------------------------------------------

TEST(BenchSuiteTierDeclaration, EveryT1FamilyDeclaresNonEmptyPsiInAndNoOtherTierDoes) {
  for (const auto& family : bench_families()) {
    SCOPED_TRACE("family=" + family->name());
    const BenchCase c = family->instantiate(Params(2, true));
    if (c.tier == ComparabilityTier::t1) {
      ASSERT_TRUE(c.psi_in.has_value())
          << "T1 family must carry psi_in as data (PRD B3)";
      EXPECT_FALSE(c.psi_in->empty());
    } else {
      EXPECT_FALSE(c.psi_in.has_value())
          << "a non-T1 family must not declare psi_in (PRD B3: only T1 is "
            "'the only tier where an external comparison is a legitimate "
            "claim')";
    }
  }
}

TEST(BenchSuiteTierDeclaration, ExactlyTheDeclaredT1FamiliesAreT1AndParityT3IsT3) {
  std::set<std::string> t1_names, t3_names, other_names;
  for (const auto& family : bench_families()) {
    const BenchCase c = family->instantiate(Params(2, true));
    if (c.tier == ComparabilityTier::t1) {
      t1_names.insert(family->name());
    } else if (c.tier == ComparabilityTier::t3) {
      t3_names.insert(family->name());
    } else {
      other_names.insert(family->name());
    }
  }
  // The four trivial-knowledge families, plus the three slippery-world arms
  // of docs/prd/engineered-domain-families.md -- the two enumerated ones from
  // Phase 1 and the compact one from Phase 3 (D4: psi_in IS A_N, so they are
  // T1 by construction). Unlike the enumerated arms, whose psi_in is
  // generated from the same slippery_step as their T_in, the compact arm's
  // ripple-carry A_N is written independently, so its T1 declaration rests on
  // the Produced-trace-equivalence certificate rather than on construction --
  // and that certificate is green at n = 2, 3, 4 on both goals.
  const std::set<std::string> expected_t1 = {"cons-prunes", "cons-inert",
                                             "mirror-small",
                                             "mirror-degenerate",
                                             "slippery-binary",
                                             "slippery-onehot",
                                             "slippery-binary-compact"};
  EXPECT_EQ(t1_names, expected_t1);
  EXPECT_EQ(t3_names, std::set<std::string>({"parity-t3"}));
  // The two knowledge-size families are t2: their T_in is aperiodic (a
  // saturating counter, not a mod-n one), so a psi_in exists and t3 would be
  // a false claim -- but none is supplied, so they must not enter the T1
  // ltlfsynt table either. That is exactly what t2 denotes.
  EXPECT_EQ(other_names, std::set<std::string>({"knowledge-chain",
                                                "knowledge-chain-inert"}));
}

// ---------------------------------------------------------------------------
// D9 -- the extractor column (docs/prd/engineered-domain-families.md Phase 4):
// "dependent-inputs-extraction" times dependent_inputs on the SAME whole-
// monolithic-task reduction the "-nk" subjects already build via
// build_nk_case (psi_in -> phi under Iknown/Oknown demoted to free). Kept to
// n=2, the sweep floor every family shares.
// ---------------------------------------------------------------------------

TEST(BenchSuiteExtractorColumn, SubjectIsRegisteredUnderItsExactName) {
  EXPECT_NO_THROW(SubjectNamed("dependent-inputs-extraction"));
}

TEST(BenchSuiteExtractorColumn, OnAPsiInBearingCaseYieldsANonEmptyDependentInputsRow) {
  const BenchCase c = FamilyNamed("slippery-binary").instantiate(Params(2, true));
  ASSERT_TRUE(c.psi_in.has_value())
      << "slippery-binary is T1 (PRD B3): a psi_in-bearing case is exactly "
        "what build_nk_case needs to produce a reduction";
  const std::vector<BenchRow> rows =
      run_bench_case(c, SubjectNamed("dependent-inputs-extraction"));
  EXPECT_FALSE(rows.empty());
  EXPECT_TRUE(RowValue(rows, "dependent_inputs"))
      << "D9: the extractor's own wall-clock must be charged under the "
        "free-form 'dependent_inputs' BenchTimer span";
}

TEST(BenchSuiteExtractorColumn, OnACaseWithoutPsiInYieldsEmptyRowsNeverAZeroRow) {
  // knowledge-chain is t2 (BenchSuiteTierDeclaration above): no psi_in, so
  // build_nk_case has no reduction to build and the subject must skip
  // cleanly -- B2 rule 1, "absent, never zero", extended to an absent case.
  const BenchCase c = FamilyNamed("knowledge-chain").instantiate(Params(2, true));
  ASSERT_FALSE(c.psi_in.has_value());
  const std::vector<BenchRow> rows =
      run_bench_case(c, SubjectNamed("dependent-inputs-extraction"));
  EXPECT_TRUE(rows.empty())
      << "no psi_in: build_nk_case returns nullopt, so the subject must "
        "record nothing, not a zero row";
}

TEST(BenchSuiteExtractorColumn,
    MeasuresUnderTheNoKnowledgeFrameWhereTheDomainPartitionWouldThrow) {
  // D9's no-knowledge frame is mandatory, not stylistic: dependent_inputs
  // throws std::invalid_argument on a partition with non-empty input_known
  // (I9, src/detail/dependency_core.cpp). Pin both halves on the SAME case:
  // the domain partition (which slippery-binary declares with non-empty
  // input_known, the position APs) throws when handed to dependent_inputs
  // directly, yet the subject succeeds on that exact case -- which is only
  // possible because build_nk_case demotes Iknown -> Ifree first (D7) before
  // the subject's own dependent_inputs call, i.e. the frame it actually
  // measures under has EMPTY input_known.
  const BenchCase c = FamilyNamed("slippery-binary").instantiate(Params(2, true));
  ASSERT_FALSE(c.vars.input_known.empty())
      << "sanity: the domain partition itself has known inputs, otherwise "
        "this test would not distinguish the two frames";

  EXPECT_THROW(dependent_inputs(c.phi, c.vars, c.t_in.dict()),
              std::invalid_argument)
      << "I9: dependent_inputs must refuse a partition whose input_known is "
        "non-empty";

  const std::vector<BenchRow> rows =
      run_bench_case(c, SubjectNamed("dependent-inputs-extraction"));
  EXPECT_FALSE(rows.empty())
      << "the subject succeeds on the identical case, so it cannot be "
        "calling dependent_inputs under the domain partition above -- it "
        "measures under build_nk_case's demoted, no-knowledge partition";
  EXPECT_TRUE(RowValue(rows, "dependent_inputs"));
}
