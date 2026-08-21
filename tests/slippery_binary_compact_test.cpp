#include <cstdint>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include <spot/tl/formula.hh>
#include <spot/twa/bdddict.hh>
#include <spot/twa/twagraph.hh>

#include "ltlf_ek/bench.hpp"
#include "ltlf_ek/bench_suite.hpp"
#include "ltlf_ek/ltlf_to_dfa.hpp"
#include "ltlf_ek/transducer_io.hpp"
#include "support/fixtures.hpp"

// Phase 3 of docs/prd/engineered-domain-families.md ("Registry additions",
// "Test oracles" T5, T7, T8, T9): the compact arm `slippery-binary-compact`
// (D5's bespoke ripple-carry A_N), reached ONLY by registry name lookup on
// the frozen include/ltlf_ek/bench_suite.hpp contract (no concrete class
// name is part of that contract, and this file never names one).
//
// CONCURRENT WORKFLOW: written before the registration lands (a /developer
// agent lands it on its own branch, D5 reusing Phase 1's t_in verbatim).
// This file cannot compile or link until that integration happens --
// expected, not a defect of this file (test-writer skill, "Before writing" /
// "Definition of done").
//
// Out of scope here, deliberately: T1 -- the registry-enumerated
// tests/produced_trace_equivalence_oracles_test.cpp already covers n = 2, 3
// automatically once slippery-binary-compact is registered (it iterates
// bench_families()); its SlipperyN4Params() has been extended in this same
// pass to add the compact arm's n = 4 cases, so T1's "both goals, n = 2, 3,
// 4" is complete across the two files, not duplicated here. T2/T3/T4/T6 are
// Phase 1/2 territory and already generic over the registry (T2/T4 in
// tests/slippery_world_test.cpp iterate a hard-coded family list that does
// not include this arm by design -- Phase 3's own green checkpoint is D8's
// structural bounds and T8's shared-t_in check, not a re-run of T2/T4, which
// stay scoped to the two Phase 1 families per that file's own header
// comment).
namespace {

using ltlf_ek::BenchCase;
using ltlf_ek::BenchFamily;
using ltlf_ek::BenchParams;
using ltlf_ek::BenchRow;
using ltlf_ek::bench_families;
using ltlf_ek::bench_subjects;
using ltlf_ek::BenchSubject;
using ltlf_ek::ltlf_to_dfa;
using ltlf_ek::print_transducer;
using ltlf_ek::run_bench_case;
using ltlf_ek::SizeMetric;
using ltlf_ek::size_metric_name;

const char kCompactFamily[] = "slippery-binary-compact";
const char kEnumeratedFamily[] = "slippery-binary";

const BenchFamily& FamilyNamed(const std::string& name) {
  for (const auto& f : bench_families())
    if (f->name() == name) return *f;
  throw std::runtime_error("slippery_binary_compact_test: no family named '" +
                           name + "'");
}

const BenchSubject& SubjectNamed(const std::string& name) {
  for (const auto& s : bench_subjects())
    if (s->name() == name) return *s;
  throw std::runtime_error(
      "slippery_binary_compact_test: no subject named '" + name + "'");
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

std::uint64_t IntPow(std::uint64_t base, std::uint64_t exp) {
  std::uint64_t r = 1;
  for (std::uint64_t i = 0; i < exp; ++i) r *= base;
  return r;
}

// Same idiom as tests/slippery_world_test.cpp's HasReachableAccepting:
// ltlf_to_dfa's result carries only reachable states, so a plain scan is a
// satisfiability check.
bool HasReachableAccepting(const spot::twa_graph_ptr& dfa) {
  for (unsigned s = 0; s < dfa->num_states(); ++s)
    if (dfa->state_is_accepting(s)) return true;
  return false;
}

}  // namespace

// ---------------------------------------------------------------------------
// T5 -- structural, exact: |T_in| = 4^n; A_N has exactly 15 top-level
// conjuncts for EVERY n (D3/D5's headline claim -- independent of N, unlike
// the enumerated arms' 14N+1); EK goal_mtdfa_roots = 1. All three are
// derived from the construction, so asserted, not measured (D8).
// ---------------------------------------------------------------------------

TEST(SlipperyBinaryCompactStructural, TinStateCountIsFourToTheN) {
  for (std::int64_t n : {2, 3, 4}) {
    for (bool realizable : {true, false}) {
      SCOPED_TRACE("n=" + std::to_string(n) +
                  " realizable=" + std::to_string(realizable));
      const BenchCase c =
          FamilyNamed(kCompactFamily).instantiate(Params(n, realizable));
      EXPECT_EQ(c.t_in.delta_dfa()->num_states(),
               static_cast<unsigned>(IntPow(4, n)))
          << "|T_in| must be 4^n = N^2 regardless of the realizable flag "
            "(D8: 'all three arms')";
    }
  }
}

TEST(SlipperyBinaryCompactStructural,
    ANHasExactlyFifteenTopLevelConjunctsIndependentOfN) {
  // n swept past the T1 certificate's own range (2..4) on purpose -- D5's
  // claim is that the conjunct count is a CONSTANT, and a constant checked
  // at only one n is indistinguishable from one that happens to equal 15
  // there. This is pure formula parsing (no automaton built), so it stays
  // cheap even at n = 6.
  for (std::int64_t n : {2, 3, 4, 5, 6}) {
    SCOPED_TRACE("n=" + std::to_string(n));
    const BenchCase c =
        FamilyNamed(kCompactFamily).instantiate(Params(n, /*realizable=*/true));
    ASSERT_TRUE(c.psi_in.has_value()) << "T1 family must carry psi_in (D4)";
    const spot::formula an = ltlf_ek::test_support::Phi(*c.psi_in);
    ASSERT_EQ(an.kind(), spot::op::And)
        << "D5: A_N is 1 init conjunct + 2x7 implications, a top-level And; "
          "got a different top-level operator instead";
    EXPECT_EQ(an.size(), 15u)
        << "D5/T5: the compact A_N must carry exactly 15 top-level "
          "conjuncts -- 1 init literal-conjunct + 2 axes x 7 rules -- for "
          "EVERY n, unlike the enumerated arms' 14N+1 (n=" << n << ")";
  }
}

TEST(SlipperyBinaryCompactStructural, EkGoalMtdfaRootsIsExactlyOne) {
  for (std::int64_t n : {2, 3}) {
    for (bool realizable : {true, false}) {
      SCOPED_TRACE("n=" + std::to_string(n) +
                  " realizable=" + std::to_string(realizable));
      const BenchCase c =
          FamilyNamed(kCompactFamily).instantiate(Params(n, realizable));
      const std::vector<BenchRow> rows =
          run_bench_case(c, SubjectNamed("mtdfa-product"));
      const std::optional<std::uint64_t> roots =
          RowValue(rows, MetricKey(SizeMetric::goal_mtdfa_roots));
      ASSERT_TRUE(roots.has_value())
          << "mtdfa-product must charge goal_mtdfa_roots for a T1 case";
      EXPECT_EQ(*roots, 1u)
          << "D8/T5: EK goal_mtdfa_roots must be 1 -- the EK side "
            "translates gamma_N alone, never A_N";
    }
  }
}

// ---------------------------------------------------------------------------
// T7 -- A_N satisfiable. Guards D6's silent X[!] collapse: with X[!] instead
// of weak X, EVERY guard in the seven-rule case-split fires at the last
// position (the guards are total), so A_N would collapse to `false`, and
// `false -> gamma` is vacuously valid -- every verdict would come back
// REALIZABLE looking fine. This test fails loudly on that regression.
// ---------------------------------------------------------------------------

TEST(SlipperyBinaryCompactSatisfiability, ANIsSatisfiableGuardingTheWeakXCollapse) {
  for (std::int64_t n : {2, 3, 4}) {
    for (bool realizable : {true, false}) {
      SCOPED_TRACE("n=" + std::to_string(n) +
                  " realizable=" + std::to_string(realizable));
      const BenchCase c =
          FamilyNamed(kCompactFamily).instantiate(Params(n, realizable));
      ASSERT_TRUE(c.psi_in.has_value());
      const spot::formula an = ltlf_ek::test_support::Phi(*c.psi_in);
      const spot::bdd_dict_ptr dict = spot::make_bdd_dict();
      const spot::twa_graph_ptr dfa = ltlf_to_dfa(an, dict);
      EXPECT_TRUE(HasReachableAccepting(dfa))
          << "the compact A_N must be satisfiable (D6/T7); an unsatisfiable "
            "A_N here means weak X silently collapsed to X[!] (or an "
            "equivalent totality bug), and 'false -> gamma' would make "
            "every verdict come back REALIZABLE unnoticed";
    }
  }
}

// ---------------------------------------------------------------------------
// T8 -- shared T_in. `slippery-binary` and `slippery-binary-compact` must
// produce the IDENTICAL transducer at every n (D3: "Arms 1 and 3 share a
// byte-identical T_in. Only psi_in differs."). This is what makes the
// compactness contrast a controlled one -- the single most important test
// in this file.
// ---------------------------------------------------------------------------

TEST(SlipperyBinaryCompactSharedTin, ProducesTheIdenticalTransducerToTheEnumeratedArm) {
  for (std::int64_t n : {2, 3, 4}) {
    for (bool realizable : {true, false}) {
      SCOPED_TRACE("n=" + std::to_string(n) +
                  " realizable=" + std::to_string(realizable));
      const BenchCase enumerated =
          FamilyNamed(kEnumeratedFamily).instantiate(Params(n, realizable));
      const BenchCase compact =
          FamilyNamed(kCompactFamily).instantiate(Params(n, realizable));

      std::ostringstream enumerated_printed, compact_printed;
      print_transducer(enumerated_printed, enumerated.t_in);
      print_transducer(compact_printed, compact.t_in);

      EXPECT_EQ(compact_printed.str(), enumerated_printed.str())
          << "D3: slippery-binary and slippery-binary-compact must share a "
            "byte-identical T_in at every n (only psi_in differs) -- a "
            "mismatch here breaks the compactness contrast's control";
    }
  }
}

// ---------------------------------------------------------------------------
// T9 -- structural, bounded (D8). |DFA(A_N)| >= 4^n for the compact arm,
// against its CONSTANT (15) conjunct count -- the superpolynomial-in-formula-
// size half of the separation claim. A `>=` bound, not an equality: the
// exact minimized size is Spot's to decide, and a minimization change of a
// few states must never turn this suite red (D8's own "bounded, not exact"
// distinction).
// ---------------------------------------------------------------------------

TEST(SlipperyBinaryCompactStructural,
    DfaOfANIsAtLeastFourToTheNAgainstAConstantConjunctCount) {
  for (std::int64_t n : {2, 3, 4}) {
    SCOPED_TRACE("n=" + std::to_string(n));
    const BenchCase c =
        FamilyNamed(kCompactFamily).instantiate(Params(n, /*realizable=*/true));
    ASSERT_TRUE(c.psi_in.has_value());
    const spot::formula an = ltlf_ek::test_support::Phi(*c.psi_in);

    // Re-assert the constant conjunct count right beside the bound it is
    // contrasted with, so this test is self-contained even if
    // ANHasExactlyFifteenTopLevelConjunctsIndependentOfN above is ever run
    // in isolation.
    ASSERT_EQ(an.kind(), spot::op::And);
    ASSERT_EQ(an.size(), 15u)
        << "sanity: the conjunct count this bound is contrasted against "
          "must stay constant (D8)";

    const spot::bdd_dict_ptr dict = spot::make_bdd_dict();
    const spot::twa_graph_ptr dfa = ltlf_to_dfa(an, dict);
    EXPECT_GE(dfa->num_states(), IntPow(4, n))
        << "D8/T9: |DFA(A_N)| must be >= 4^n for the compact arm (it must "
          "track position) -- against a constant 15-conjunct formula, this "
          "is the superpolynomial half of the separation claim (n=" << n
        << ")";
  }
}
