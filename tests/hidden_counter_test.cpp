#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include <gtest/gtest.h>
#include <spot/tl/parse.hh>
#include <spot/tl/print.hh>

#include "ltlf_ek/bench.hpp"
#include "ltlf_ek/bench_suite.hpp"
#include "ltlf_ek/produced_trace_equivalence.hpp"
#include "ltlf_ek/role.hpp"

// Tests for the `hidden-counter` family (src/bench_suite.cpp): an N-bit
// counter of a-steps whose top bit is the only visible part.  No test
// asserts a speedup or a size ratio.
namespace {

using ltlf_ek::BenchCase;
using ltlf_ek::BenchFamily;
using ltlf_ek::BenchParams;
using ltlf_ek::BenchRow;
using ltlf_ek::ComparabilityTier;
using ltlf_ek::Role;

const BenchFamily& HiddenCounter() {
  for (const auto& f : ltlf_ek::bench_families())
    if (f->name() == "hidden-counter") return *f;
  throw std::runtime_error("no hidden-counter family registered");
}

const ltlf_ek::BenchSubject& SubjectNamed(const std::string& name) {
  for (const auto& s : ltlf_ek::bench_subjects())
    if (s->name() == name) return *s;
  throw std::runtime_error("no subject named " + name);
}

BenchCase Cell(std::int64_t n, bool realizable) {
  return HiddenCounter().instantiate(
      BenchParams{{"n", n}, {"realizable", realizable ? 1 : 0}});
}

std::optional<std::uint64_t> RowValue(const std::vector<BenchRow>& rows,
                                      const std::string& key) {
  for (const BenchRow& r : rows)
    if (r.key == key) return r.value;
  return std::nullopt;
}

TEST(HiddenCounter, SweepsEveryBitWidthFromOneInBothPolarities) {
  EXPECT_EQ(HiddenCounter().sweep(1, 4).size(), 8u);
  EXPECT_THROW(HiddenCounter().sweep(0, 4), std::invalid_argument);
  EXPECT_THROW(Cell(0, true), std::invalid_argument);
}

TEST(HiddenCounter, DeclaresTierT1WithAPsiIn) {
  const BenchCase c = Cell(2, true);
  EXPECT_EQ(c.tier, ComparabilityTier::t1);
  EXPECT_TRUE(c.psi_in.has_value());
}

TEST(HiddenCounter, TinHasTwoToTheNStates) {
  for (std::int64_t n = 1; n <= 4; ++n)
    EXPECT_EQ(Cell(n, true).t_in.delta_dfa()->num_states(), 1u << n)
        << "N=" << n;
}

TEST(HiddenCounter, PsiInIsProducedTraceEquivalentToTinForNOneToFour) {
  for (std::int64_t n = 1; n <= 4; ++n) {
    const BenchCase c = Cell(n, true);
    const ltlf_ek::EquivalenceResult r = ltlf_ek::produced_trace_equivalent(
        c.t_in, spot::parse_infix_psl(*c.psi_in).f, c.vars, Role::t_in);
    EXPECT_TRUE(r.equivalent_on_nonempty) << "N=" << n;
  }
}

TEST(HiddenCounter, PhiFloorIsMaxOfTwoAndTwoToTheNMinusOneStrongNexts) {
  for (std::int64_t n = 1; n <= 4; ++n)
    for (bool realizable : {true, false}) {
      const BenchCase c = Cell(n, realizable);
      std::int64_t depth = 0;
      for (const spot::formula conjunct : c.phi) {
        if (conjunct.is(spot::op::G)) continue;
        spot::formula f = conjunct;
        while (f.is(spot::op::strong_X)) {
          ++depth;
          f = f[0];
        }
      }
      EXPECT_EQ(depth, std::max<std::int64_t>(2, std::int64_t{1} << (n - 1)))
          << "N=" << n << " realizable=" << realizable;
    }
}

TEST(HiddenCounter, EkDecidesTheDeclaredVerdictForNOneToFour) {
  for (std::int64_t n = 1; n <= 4; ++n)
    for (bool realizable : {true, false}) {
      const std::vector<BenchRow> rows = ltlf_ek::run_bench_case(
          Cell(n, realizable), SubjectNamed("dfa-product"));
      EXPECT_EQ(RowValue(rows, "controller_states").has_value(), realizable)
          << "N=" << n << " realizable=" << realizable;
    }
}

TEST(HiddenCounter, LtlfsyntDecidesTheDeclaredVerdictForNOneToFour) {
  if (std::system("command -v ltlfsynt >/dev/null 2>&1") != 0)
    GTEST_SKIP() << "no ltlfsynt on PATH";
  for (std::int64_t n = 1; n <= 4; ++n)
    for (bool realizable : {true, false}) {
      const BenchCase c = Cell(n, realizable);
      const std::string cmd =
          "ltlfsynt --ins=a,k --outs=o --semantics=Mealy --realizability -f '(" +
          *c.psi_in + ") -> (" + spot::str_psl(c.phi) + ")' 2>&1";
      std::string out;
      FILE* p = popen(cmd.c_str(), "r");
      ASSERT_NE(p, nullptr);
      char buf[256];
      while (std::fgets(buf, sizeof buf, p)) out += buf;
      pclose(p);
      EXPECT_EQ(out, realizable ? "REALIZABLE\n" : "UNREALIZABLE\n")
          << "N=" << n << " realizable=" << realizable;
    }
}

TEST(HiddenCounter, KnowledgeSizesRecordsTinStatesAndPsiInSizePerCell) {
  for (std::int64_t n = 1; n <= 4; ++n) {
    const BenchCase c = Cell(n, true);
    const std::vector<BenchRow> rows =
        ltlf_ek::run_bench_case(c, SubjectNamed("knowledge-sizes"));
    EXPECT_EQ(RowValue(rows, ltlf_ek::kTInStatesMetric),
              std::optional<std::uint64_t>(1u << n));
    EXPECT_EQ(RowValue(rows, ltlf_ek::kPsiInSizeMetric),
              std::optional<std::uint64_t>(
                  ltlf_ek::formula_node_count(
                      spot::parse_infix_psl(*c.psi_in).f)));
  }
}

TEST(FormulaNodeCount, CountsEveryOccurrenceOfEveryOperatorAndLeaf) {
  EXPECT_EQ(ltlf_ek::formula_node_count(spot::parse_infix_psl("a").f), 1u);
  // G, ->, a, X, b
  EXPECT_EQ(ltlf_ek::formula_node_count(spot::parse_infix_psl("G(a -> X b)").f),
            5u);
}

}  // namespace
