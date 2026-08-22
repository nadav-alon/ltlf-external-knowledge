#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <map>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include <sys/wait.h>
#include <unistd.h>

#include <gtest/gtest.h>

#include <spot/tl/formula.hh>
#include <spot/twa/bdddict.hh>
#include <spot/twa/twagraph.hh>

#include "ltlf_ek/bench.hpp"
#include "ltlf_ek/bench_suite.hpp"
#include "ltlf_ek/detail/util.hpp"
#include "ltlf_ek/dfa_product.hpp"
#include "ltlf_ek/ltlf_to_dfa.hpp"
#include "ltlf_ek/output_labeled_transducer.hpp"
#include "ltlf_ek/synthesis.hpp"
#include "ltlf_ek/variables.hpp"
#include "ltlf_ek/verify_controller.hpp"
#include "support/fixtures.hpp"

#ifndef LTLF_EK_BENCH_BINARY
#error "LTLF_EK_BENCH_BINARY must be defined by CMake (see CMakeLists.txt)"
#endif
#ifndef LTLF_EK_REPO_ROOT
#error "LTLF_EK_REPO_ROOT must be defined by CMake (see CMakeLists.txt)"
#endif

// Phase 1 of docs/prd/engineered-domain-families.md ("Registry additions",
// "Test oracles" T2-T5 + T7, "Edge cases"): the two enumerated families
// (`slippery-binary`, `slippery-onehot`, D1-D4) and the five no-knowledge
// subjects (`<method>-nk`, D7), reached ONLY by registry name lookup on the
// frozen include/ltlf_ek/bench_suite.hpp contract -- concrete class names
// (e.g. a hypothetical SlipperyBinaryFamily) are not part of that contract
// and this file never names one.
//
// CONCURRENT WORKFLOW: written before the registrations land (a /developer
// agent lands them on its own branch). This file cannot compile or link
// until that integration happens -- expected, not a defect of this file
// (test-writer skill, "Before writing" / "Definition of done").
//
// Out of scope here, and deliberately absent: T1/T6 (need
// produced_trace_equivalent, Phase 2), T8/T9 (need slippery-binary-compact,
// Phase 3).
namespace {

using ltlf_ek::BenchCase;
using ltlf_ek::BenchFamily;
using ltlf_ek::BenchParams;
using ltlf_ek::BenchRow;
using ltlf_ek::BenchSubject;
using ltlf_ek::bench_families;
using ltlf_ek::bench_subjects;
using ltlf_ek::Controller;
using ltlf_ek::DfaProduct;
using ltlf_ek::ltlf_to_dfa;
using ltlf_ek::run_bench_case;
using ltlf_ek::SizeMetric;
using ltlf_ek::size_metric_name;
using ltlf_ek::verify_controller;
using ltlf_ek::VerifyResult;

// The two Phase 1 arms, named exactly as the PRD's "Registry additions"
// table spells them -- never a concrete class name.
const std::vector<std::string> kPhase1Families = {"slippery-binary",
                                                   "slippery-onehot"};

// D7's five no-knowledge subjects, named exactly as the PRD spells them.
const std::vector<std::string> kNkSubjects = {
    "dfa-product-nk", "nfa-product-nk", "mtdfa-product-nk",
    "mtnfa-product-nk", "otf-mtdfa-product-nk"};

// The five knowledge-aware methods T2 races against each other (PRD "Test
// oracles" T2) -- the landed Phase 2 subject names, tests/bench_suite_test.cpp's
// precedent.
const std::vector<std::string> kFiveMethods = {
    "dfa-product", "nfa-product", "mtdfa-product", "mtnfa-product",
    "otf-mtdfa-product"};

// One measured-infeasible cell, excluded by construction rather than by a
// wall-clock race (a race would be flaky, and `run_bench_case` has no
// cancellation hook -- src/ltlf_ek_bench.cpp bounds it by DETACHING the
// worker thread, which is exactly the thing a test binary must not do).
//
// `NfaProduct` does not finish on `slippery-onehot` at n = 3 -- measured
// 2026-08-20 on both goals, EK and `-nk` columns alike: > 90 s against
// < 1 s for all 19 other (method, arm, n) cells at n = 2, 3, and still
// unfinished at 5.6 min. One-hot at n = 3 carries 2*2^3 = 16 position APs
// and MONA is the only method whose cost tracks the alphabet that way;
// `MtnfaProduct` uses MONA too and is instant here, so this is a property of
// NfaProduct's construction, not of MONA's presence.
//
// This is PRD Stop-list 8 arriving early ("One-hot dying at large n is a
// reportable data point, not a bug to tune around ... record the row and
// continue the other arms"). The remaining four methods still cross-check
// each other on the cell, so T2 keeps its teeth; the exclusion is reported
// in docs/runs/2026-08-20-edf-phase1.md, never silently absorbed.
bool MethodInfeasibleHere(const std::string& method,
                          const std::string& family_name, std::int64_t n) {
  return (method == "nfa-product" || method == "nfa-product-nk") &&
         family_name == "slippery-onehot" && n >= 3;
}

// ---------------------------------------------------------------------------
// Shared lookup / measurement helpers (duplicated from tests/bench_suite_test.cpp
// rather than shared across translation units -- this project's one-file-per-
// suite norm, see tests/ltlfsynt_oracle_test.cpp's ShellQuote/CliResult comment).
// ---------------------------------------------------------------------------

const BenchFamily& FamilyNamed(const std::string& name) {
  for (const auto& f : bench_families())
    if (f->name() == name) return *f;
  throw std::runtime_error("slippery_world_test: no family named '" + name +
                           "'");
}

const BenchSubject& SubjectNamed(const std::string& name) {
  for (const auto& s : bench_subjects())
    if (s->name() == name) return *s;
  throw std::runtime_error("slippery_world_test: no subject named '" + name +
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

// nullopt => the subject skipped this case entirely (rows empty); otherwise
// the realizability verdict read off controller_states' presence (B2 rule 1
// / tests/bench_suite_test.cpp's precedent).
std::optional<bool> RealizabilityVerdict(const std::vector<BenchRow>& rows) {
  if (rows.empty()) return std::nullopt;
  return RowValue(rows, MetricKey(SizeMetric::controller_states)).has_value();
}

std::uint64_t IntPow(std::uint64_t base, std::uint64_t exp) {
  std::uint64_t r = 1;
  for (std::uint64_t i = 0; i < exp; ++i) r *= base;
  return r;
}

// Is there a reachable accepting (final) state? Same idiom as
// tests/ltlf_to_dfa_test.cpp's has_reachable_accepting: ltlf_to_dfa's result
// carries only reachable states, so a plain scan is a satisfiability check.
bool HasReachableAccepting(const spot::twa_graph_ptr& dfa) {
  for (unsigned s = 0; s < dfa->num_states(); ++s)
    if (dfa->state_is_accepting(s)) return true;
  return false;
}

}  // namespace

// ---------------------------------------------------------------------------
// Unit: instantiate() rejects n < 2 (D2's floor), scoped explicitly to the
// two Phase 1 families for direct traceability to the PRD's unit-test list
// (the landed tests/bench_suite_test.cpp BenchFamilyNFloor test already
// re-covers this generically once these families are registered).
// ---------------------------------------------------------------------------

TEST(SlipperyWorldNFloor, BothFamiliesRejectNBelowTwoRatherThanInstantiating) {
  for (const std::string& name : kPhase1Families) {
    SCOPED_TRACE("family=" + name);
    const BenchFamily& family = FamilyNamed(name);
    EXPECT_THROW(family.instantiate(Params(0, true)), std::invalid_argument)
        << "n=0/n=1 are degenerate (D2): c = 2^(n-1)-1 requires n>=2";
    EXPECT_THROW(family.instantiate(Params(0, false)), std::invalid_argument);
    EXPECT_THROW(family.instantiate(Params(1, true)), std::invalid_argument)
        << "n=1 (N=2): centre IS the start cell, carries no information (D2)";
    EXPECT_THROW(family.instantiate(Params(1, false)), std::invalid_argument);
    EXPECT_THROW(family.sweep(1, 3), std::invalid_argument)
        << "sweep(n_min below the floor) must also reject, not silently "
          "clamp";
  }
}

// ---------------------------------------------------------------------------
// Unit (D7 / "Edge cases"): a BenchCase without psi_in makes every -nk
// subject skip cleanly -- recording NOTHING, not a zero row -- so the five
// landed T1 families (and any t2/t3 family) are unaffected. Reuses an
// EXISTING landed family with no psi_in (tier t2: "knowledge-chain-inert",
// tests/bench_suite_test.cpp's ExactlyTheFourTrivialKnowledgeFamiliesAreT1...
// pins this) rather than hand-building a BenchCase, since the frozen
// BenchCase shape is already exercised by the landed registry.
// ---------------------------------------------------------------------------

TEST(SlipperyWorldNkSubjects, SkipCleanlyRecordingNothingWhenPsiInIsAbsent) {
  const BenchCase c =
      FamilyNamed("knowledge-chain-inert").instantiate(Params(2, true));
  ASSERT_FALSE(c.psi_in.has_value())
      << "sanity: knowledge-chain-inert is tier t2 and declares no psi_in "
        "(tests/bench_suite_test.cpp TierDeclaration)";
  for (const std::string& subject_name : kNkSubjects) {
    SCOPED_TRACE("subject=" + subject_name);
    const std::vector<BenchRow> rows =
        run_bench_case(c, SubjectNamed(subject_name));
    EXPECT_TRUE(rows.empty())
        << "a BenchCase with no psi_in must make a -nk subject skip "
          "cleanly (D7), recording nothing -- got " << rows.size()
        << " row(s)";
  }
}

// ---------------------------------------------------------------------------
// Unit (D7): the -nk partition really has Iknown = Oknown = empty. Not
// observable directly (the demoted VariablePartition is internal to the
// subject), so this is the black-box proof D7 itself names: "Order matters
// ... enforced by a throw, not by convention" -- trivial_transducer throws
// std::invalid_argument whenever the role's produced slice (Iknown for t_in)
// is non-empty (include/ltlf_ek/output_labeled_transducer.hpp). Both Phase 1
// families have non-empty domain Iknown (the position APs, D1), so a -nk
// subject that did NOT demote Iknown before building its trivial transducer
// would throw here. It must not.
// ---------------------------------------------------------------------------

TEST(SlipperyWorldNkSubjects, DemotesKnownVariablesBeforeBuildingTheTrivialTransducer) {
  for (const std::string& family_name : kPhase1Families) {
    for (std::int64_t n : {2, 3}) {
      for (bool realizable : {true, false}) {
        SCOPED_TRACE("family=" + family_name + " n=" + std::to_string(n) +
                    " realizable=" + std::to_string(realizable));
        const BenchCase c =
            FamilyNamed(family_name).instantiate(Params(n, realizable));
        ASSERT_FALSE(c.vars.input_known.empty())
            << "sanity: the domain partition must have non-empty Iknown "
              "(D1: the position APs) for this to be a meaningful demotion "
              "check";
        for (const std::string& subject_name : kNkSubjects) {
          if (MethodInfeasibleHere(subject_name, family_name, n)) continue;
          SCOPED_TRACE("subject=" + subject_name);
          EXPECT_NO_THROW({
            const std::vector<BenchRow> rows =
                run_bench_case(c, SubjectNamed(subject_name));
            (void)rows;
          }) << "trivial_transducer throws when Iknown is non-empty (D7); a "
               "throw here means the -nk subject built its transducer "
               "against the DOMAIN partition instead of the demoted one";
        }
      }
    }
  }
}

// ---------------------------------------------------------------------------
// T2 -- cross-method agreement. The five knowledge-aware methods agree with
// each other and with the family's declared expected_realizable, at n = 2, 3
// (N = 4, 8) for both goals. Backed by measurement, not assumption:
// `ltlfsynt --semantics=Mealy --realizability` on A_N -> gamma_N gave corner
// REALIZABLE / centre UNREALIZABLE at both N=4 and N=8 (run 2026-08-19,
// binary arm) -- this test checks the five in-process methods reproduce that
// same declaration; T3 below checks ltlfsynt itself agrees, live.
// ---------------------------------------------------------------------------

TEST(SlipperyWorldCrossMethodAgreement, FiveMethodsAgreeWithEachOtherAndTheDeclaredVerdict) {
  for (const std::string& family_name : kPhase1Families) {
    for (std::int64_t n : {2, 3}) {
      for (bool realizable : {true, false}) {
        SCOPED_TRACE("family=" + family_name + " n=" + std::to_string(n) +
                    " realizable=" + std::to_string(realizable));
        const BenchCase c =
            FamilyNamed(family_name).instantiate(Params(n, realizable));
        ASSERT_EQ(c.expected_realizable, realizable);

        std::optional<bool> reference;
        int subjects_ran = 0;
        for (const std::string& method : kFiveMethods) {
          if (MethodInfeasibleHere(method, family_name, n)) continue;
          const std::vector<BenchRow> rows =
              run_bench_case(c, SubjectNamed(method));
          const std::optional<bool> verdict = RealizabilityVerdict(rows);
          if (!verdict.has_value()) continue;  // e.g. mona absent.
          ++subjects_ran;
          if (!reference.has_value()) {
            reference = verdict;
          } else {
            EXPECT_EQ(*verdict, *reference)
                << "method=" << method
                << " disagrees with an earlier method on this case (T2)";
          }
        }
        ASSERT_TRUE(reference.has_value())
            << "every case must have at least one method that actually ran";
        EXPECT_EQ(*reference, c.expected_realizable)
            << "the agreed verdict disagrees with the family's declared "
              "expected_realizable (T2 / Stop-list item 4)";
        RecordProperty("t2_subjects_ran", subjects_ran);
      }
    }
  }
}

// ---------------------------------------------------------------------------
// T4 -- metamorphic round-trip. verify_controller accepts the controller
// synthesized for every realizable (corner) case, at n = 2, 3. DfaProduct is
// the reference method here (no mona dependency, so this oracle never
// skips).
// ---------------------------------------------------------------------------

TEST(SlipperyWorldMetamorphicRoundTrip, VerifyControllerAcceptsTheCornerCaseController) {
  DfaProduct method;
  for (const std::string& family_name : kPhase1Families) {
    for (std::int64_t n : {2, 3}) {
      SCOPED_TRACE("family=" + family_name + " n=" + std::to_string(n));
      const BenchCase c =
          FamilyNamed(family_name).instantiate(Params(n, /*realizable=*/true));
      ASSERT_TRUE(c.expected_realizable)
          << "sanity: realizable=true must instantiate the corner case (D3)";

      const std::optional<Controller> controller =
          method.synthesize(c.phi, c.vars, c.t_in, c.t_out);
      ASSERT_TRUE(controller.has_value())
          << "corner case declared realizable but DfaProduct found no "
            "controller";

      const VerifyResult vr =
          verify_controller(c.phi, c.vars, c.t_in, c.t_out, *controller);
      EXPECT_TRUE(vr.ok)
          << "verify_controller rejected the synthesized corner-case "
            "controller (T4)";
    }
  }
}

// ---------------------------------------------------------------------------
// T5 -- structural, exact. |T_in| = 4^n (all arms); A_N has exactly 14N
// G-rooted rules plus a literals-only initial-cell remainder, i.e. 14N + 2n
// (binary) / 14N + 2N (one-hot) top-level conjuncts, NOT the 14N+1 D3/D8
// first claimed (arms 1, 2 -- see the test body below and the compact arm's
// 14 + 2n counterpart); EK goal_mtdfa_roots = 1.
// ---------------------------------------------------------------------------

TEST(SlipperyWorldStructural, TinStateCountIsFourToTheN) {
  for (const std::string& family_name : kPhase1Families) {
    for (std::int64_t n : {2, 3}) {
      for (bool realizable : {true, false}) {
        SCOPED_TRACE("family=" + family_name + " n=" + std::to_string(n) +
                    " realizable=" + std::to_string(realizable));
        const BenchCase c =
            FamilyNamed(family_name).instantiate(Params(n, realizable));
        EXPECT_EQ(c.t_in.delta_dfa()->num_states(),
                 static_cast<unsigned>(IntPow(4, n)))
            << "|T_in| must be 4^n = N^2 regardless of the realizable flag "
              "(D8: 'all three arms')";
      }
    }
  }
}

TEST(SlipperyWorldStructural, ANHasExactlyFourteenNGRulesPlusALiteralOnlyInit) {
  for (const std::string& family_name : kPhase1Families) {
    for (std::int64_t n : {2, 3}) {
      SCOPED_TRACE("family=" + family_name + " n=" + std::to_string(n));
      const BenchCase c =
          FamilyNamed(family_name).instantiate(Params(n, /*realizable=*/true));
      ASSERT_TRUE(c.psi_in.has_value())
          << "T1 family must carry psi_in (D4)";
      const spot::formula an = ltlf_ek::test_support::Phi(*c.psi_in);
      const std::uint64_t nn = static_cast<std::uint64_t>(n);
      const std::uint64_t big_n = IntPow(2, nn);
      ASSERT_EQ(an.kind(), spot::op::And)
          << "D5: A_N is 1 init conjunct + 2x7 implications, a top-level "
            "And; got a different top-level operator instead";

      // D3/D8's "14N+1 top-level conjuncts" counts the initial-cell
      // constraint as ONE conjunct.  Spot's And is n-ary and flattens, so
      // that conjunct's own literals ("pos = (0,0)", a conjunction of 2n
      // binary / 2N one-hot literals) become top-level children of the same
      // And and an.size() reads 14N + 2n (resp. 14N + 2N) instead.  The
      // structural claim is therefore asserted the way it is CONSTRUCTED:
      // exactly 14N G-rooted rules -- 2 axes x N cells x 7 rules -- and an
      // initial-cell remainder that is literals only, nothing else hiding
      // in it.  This is also the form that stays comparable to Phase 3's
      // compact arm, whose count is 14 G-rules against this arm's 14N.
      std::uint64_t g_rules = 0;
      std::uint64_t init_literals = 0;
      for (unsigned i = 0; i < an.size(); ++i) {
        const spot::formula child = an[i];
        if (child.kind() == spot::op::G) {
          ++g_rules;
        } else {
          ++init_literals;
          EXPECT_TRUE(child.is_literal())
              << "a non-G top-level conjunct of A_N must be an initial-cell "
                "literal (D3: the only non-G conjunct is 'pos = (0,0)'); got "
              << child;
        }
      }
      EXPECT_EQ(g_rules, 14 * big_n)
          << "D5/T5: A_N must carry exactly 14N G-rooted rules -- 2 axes x N "
            "cells x 7 rules (2 movers x 2 slip values + 3 non-movers) -- "
            "(N=" << big_n << ")";
      const std::uint64_t expected_init =
          family_name == "slippery-onehot" ? 2 * big_n : 2 * nn;
      EXPECT_EQ(init_literals, expected_init)
          << "D5/T5: the flattened initial-cell conjunct must contribute "
            "exactly one literal per position AP (N=" << big_n << ")";
    }
  }
}

TEST(SlipperyWorldStructural, EkGoalMtdfaRootsIsExactlyOne) {
  for (const std::string& family_name : kPhase1Families) {
    for (std::int64_t n : {2, 3}) {
      for (bool realizable : {true, false}) {
        SCOPED_TRACE("family=" + family_name + " n=" + std::to_string(n) +
                    " realizable=" + std::to_string(realizable));
        const BenchCase c =
            FamilyNamed(family_name).instantiate(Params(n, realizable));
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
}

// ---------------------------------------------------------------------------
// T7 -- A_N satisfiable. Guards D6's silent X[!] collapse: with X[!] instead
// of weak X, A_N would be `false`, and `false -> gamma` is vacuously valid,
// so every verdict would come back REALIZABLE looking fine. This test would
// fail loudly on that regression.
// ---------------------------------------------------------------------------

TEST(SlipperyWorldSatisfiability, ANIsSatisfiableGuardingTheWeakXCollapse) {
  for (const std::string& family_name : kPhase1Families) {
    for (std::int64_t n : {2, 3}) {
      for (bool realizable : {true, false}) {
        SCOPED_TRACE("family=" + family_name + " n=" + std::to_string(n) +
                    " realizable=" + std::to_string(realizable));
        const BenchCase c =
            FamilyNamed(family_name).instantiate(Params(n, realizable));
        ASSERT_TRUE(c.psi_in.has_value());
        const spot::formula an = ltlf_ek::test_support::Phi(*c.psi_in);
        const spot::bdd_dict_ptr dict = spot::make_bdd_dict();
        const spot::twa_graph_ptr dfa = ltlf_to_dfa(an, dict);
        EXPECT_TRUE(HasReachableAccepting(dfa))
            << "A_N must be satisfiable (D6/T7); an unsatisfiable A_N here "
              "means weak X silently collapsed to X[!] (or an equivalent "
              "totality bug), and 'false -> gamma' would make every "
              "verdict come back REALIZABLE unnoticed";
      }
    }
  }
}

// ---------------------------------------------------------------------------
// T3 -- ltlfsynt T1 race agreement, driven live through the `ltlf-ek-bench`
// binary (existing machinery, docs/prd/benchmark-suite.md "The ltlfsynt T1
// race"): verdict_mismatch_count must be 0 for both Phase 1 arms at n=2,3.
// `ltlfsynt` is invoked by the ABSOLUTE path
// $HOME/opt/spot-2.15.1/bin/ltlfsynt (the bare name can resolve to a
// 2.14.4.dev install with the mtdfa backprop bug, project CLAUDE.md).
// --subjects=dfa-product keeps the run cheap; the T1 race itself runs once
// per (family, n, realizable) independent of which subjects were selected
// (src/ltlf_ek_bench.cpp: the race block sits after, not inside, the subject
// loop).
// ---------------------------------------------------------------------------

namespace {

struct CliResult {
  int exit_code = -1;
  std::string stdout_text;
  std::string stderr_text;
};

std::string ShellQuote(const std::string& s) {
  std::string out = "'";
  for (char c : s) {
    if (c == '\'')
      out += "'\\''";
    else
      out += c;
  }
  out += "'";
  return out;
}

class ScopedTempFile {
 public:
  explicit ScopedTempFile() {
    path_ = ltlf_ek::detail::temp_template("slippery_world_test");
    const int fd = mkstemp(path_.data());
    EXPECT_GE(fd, 0) << "mkstemp failed for " << path_;
    if (fd >= 0) close(fd);
  }
  ~ScopedTempFile() { std::remove(path_.c_str()); }
  const std::string& path() const { return path_; }

 private:
  std::string path_;
};

// Runs `binary` with `args` via a POSIX shell, with a wall-clock `timeout`
// (coreutils `timeout`), returning its exit code, stdout, stderr. Same idiom
// as tests/ltlf_ek_synth_test.cpp's RunCli / tests/ltlfsynt_oracle_test.cpp's
// RunSubprocess (duplicated per this project's one-file-per-suite norm).
CliResult RunCli(const std::string& binary, const std::vector<std::string>& args,
                 unsigned timeout_secs) {
  ScopedTempFile out_capture, err_capture;
  std::ostringstream cmd;
  cmd << "timeout " << timeout_secs << "s " << ShellQuote(binary);
  for (const auto& a : args) cmd << " " << ShellQuote(a);
  cmd << " >" << ShellQuote(out_capture.path()) << " 2>"
      << ShellQuote(err_capture.path());
  const int rc = std::system(cmd.str().c_str());

  CliResult result;
  result.exit_code = WIFEXITED(rc) ? WEXITSTATUS(rc) : -1;
  std::ifstream out_in(out_capture.path());
  std::ostringstream out_ss;
  out_ss << out_in.rdbuf();
  result.stdout_text = out_ss.str();
  std::ifstream err_in(err_capture.path());
  std::ostringstream err_ss;
  err_ss << err_in.rdbuf();
  result.stderr_text = err_ss.str();
  return result;
}

bool IsRunnable(const std::string& binary) {
  if (binary.empty()) return false;
  std::ostringstream cmd;
  cmd << ShellQuote(binary) << " --version >/dev/null 2>&1";
  const int rc = std::system(cmd.str().c_str());
  return WIFEXITED(rc) && WEXITSTATUS(rc) == 0;
}

// Minimal scalar-field extractor over the JSON `ltlf-ek-bench` prints
// (src/ltlf_ek_bench.cpp: `json << ",\"verdict_mismatch_count\":" <<
// mismatch_count;` -- no whitespace around the colon). Deliberately not a
// full JSON parser (tests/bench_test.cpp already has one for its own schema
// oracle; this file needs exactly one scalar field).
std::optional<long long> ExtractJsonIntField(const std::string& json,
                                             const std::string& key) {
  const std::string needle = "\"" + key + "\":";
  const std::size_t pos = json.find(needle);
  if (pos == std::string::npos) return std::nullopt;
  std::size_t i = pos + needle.size();
  const std::size_t start = i;
  if (i < json.size() && json[i] == '-') ++i;
  while (i < json.size() && std::isdigit(static_cast<unsigned char>(json[i])))
    ++i;
  if (i == start || (i == start + 1 && json[start] == '-')) return std::nullopt;
  return std::stoll(json.substr(start, i - start));
}

}  // namespace

TEST(SlipperyWorldLtlfsyntRace, ReportsZeroVerdictMismatchesForBothArmsAtNEqualsTwoAndThree) {
  const char* home = std::getenv("HOME");
  if (!home || !*home) {
    GTEST_SKIP() << "$HOME not set; cannot resolve the required absolute "
                    "ltlfsynt path";
  }
  const std::string ltlfsynt_path =
      std::string(home) + "/opt/spot-2.15.1/bin/ltlfsynt";
  if (!IsRunnable(ltlfsynt_path)) {
    GTEST_SKIP() << "ltlfsynt not found/runnable at " << ltlfsynt_path
                 << " (project convention: the bare name can resolve to a "
                    "2.14.4.dev install with the mtdfa backprop bug, so "
                    "this test does not fall back to it)";
  }

  const std::string out_path =
      std::string(LTLF_EK_REPO_ROOT) + "/build/bench_test_out/slippery_world_t3_race.json";
  const CliResult r = RunCli(
      LTLF_EK_BENCH_BINARY,
      {"--families=slippery-binary,slippery-onehot", "--subjects=dfa-product",
       "--n-min=2", "--n-max=3", "--repeat=1", "--timeout=30",
       "--ltlfsynt=" + ltlfsynt_path, "--out=" + out_path},
      /*timeout_secs=*/180);
  ASSERT_EQ(r.exit_code, 0)
      << "ltlf-ek-bench exited non-zero; stdout=[" << r.stdout_text
      << "] stderr=[" << r.stderr_text << "]";

  std::ifstream report_in(out_path);
  ASSERT_TRUE(report_in.good()) << "could not open " << out_path;
  std::ostringstream report_ss;
  report_ss << report_in.rdbuf();
  const std::string report_json = report_ss.str();

  const std::optional<long long> mismatch_count =
      ExtractJsonIntField(report_json, "verdict_mismatch_count");
  ASSERT_TRUE(mismatch_count.has_value())
      << "verdict_mismatch_count key missing from the JSON report: "
      << report_json;
  EXPECT_EQ(*mismatch_count, 0)
      << "T3: the ltlfsynt T1 race must report zero verdict mismatches "
        "(Stop-list item 3/4 is an O5-class theory finding, never a "
        "benchmark bug); full report=[" << report_json << "]";
}
