#include <cstdint>
#include <optional>
#include <queue>
#include <stdexcept>
#include <string>
#include <vector>

#include <gtest/gtest.h>
#include <bddx.h>
#include <spot/tl/parse.hh>
#include <spot/twa/bdddict.hh>
#include <spot/twa/twagraph.hh>

#include "ltlf_ek/bench_suite.hpp"
#include "ltlf_ek/ltlf_to_dfa.hpp"
#include "ltlf_ek/output_labeled_transducer.hpp"
#include "ltlf_ek/product.hpp"
#include "ltlf_ek/produced_trace_equivalence.hpp"
#include "ltlf_ek/role.hpp"
#include "ltlf_ek/transducer.hpp"
#include "ltlf_ek/variables.hpp"

// The T1 / T6 oracle-layer tests for produced_trace_equivalent (docs/prd/
// engineered-domain-families.md "Test oracles (for /test-writer)"), the two
// oracles left for this pass. The API's own unit-level behaviour (degenerate
// psi, a nowhere-defined tau, one hand-built positive/negative case with the
// witness checked for shortest-ness/determinism) is covered by
// tests/produced_trace_equivalence_test.cpp and is intentionally not
// repeated here.
//
// T1 -- certificate green (D4): the retrofit that closes benchmark-suite.md
// B3's hole, where a family's declared psi_in was hand-written with nothing
// checking it. Enumerated from bench_families() so a future family is
// covered automatically. A RED result here is a finding against that
// family's psi_in, not a test bug -- see the assertion messages below and
// the HARD RULES in this pass's task description: never repaired by editing
// psi_in, relaxing to containment, or excluding the witness.
//
// T6 -- negative control: two satisfiable mutants of a slippery-world-style
// A_N must be CAUGHT, with a witness. Built on a small, hand-checkable
// one-axis "slippery-line" reduction (N = 2) of the landed slippery-world
// construction (D1/D3), not on D5's Keep/Inc vocabulary -- rationale and
// /code-reviewer sign-off recorded in docs/prd/engineered-domain-families.md
// "Developer comments / PRD disagreements", 2026-08-21.
namespace {

using ltlf_ek::BenchCase;
using ltlf_ek::BenchFamily;
using ltlf_ek::BenchParams;
using ltlf_ek::bench_families;
using ltlf_ek::ComparabilityTier;
using ltlf_ek::comparability_tier_name;
using ltlf_ek::EquivalenceResult;
using ltlf_ek::goal_delta;
using ltlf_ek::LetterAlphabet;
using ltlf_ek::ltlf_to_dfa;
using ltlf_ek::OutputLabeledTransducer;
using ltlf_ek::produced_trace_equivalent;
using ltlf_ek::Role;
using ltlf_ek::Transducer;
using ltlf_ek::VariablePartition;

spot::formula Phi(const std::string& s) { return spot::parse_formula(s); }

// Same lookup pattern as tests/bench_suite_test.cpp's FamilyNamed --
// enumerating bench_families() rather than hard-coding a family list is the
// point of this retrofit (D4).
const BenchFamily& FamilyNamed(const std::string& name) {
  for (const auto& f : bench_families())
    if (f->name() == name) return *f;
  throw std::runtime_error(
      "produced_trace_equivalence_oracles_test: no family named '" + name +
      "'");
}

// ---------------------------------------------------------------------------
// T1 -- certificate green, over the whole registry.
// ---------------------------------------------------------------------------

struct T1Param {
  std::string family;
  std::int64_t n;
  bool realizable;
};

std::string T1ParamName(const testing::TestParamInfo<T1Param>& info) {
  std::string family = info.param.family;
  for (char& c : family)
    if (c == '-') c = '_';
  return family + "_n" + std::to_string(info.param.n) +
        (info.param.realizable ? "_corner" : "_centre");
}

// Every registered family x n in {2, 3} x both goals. A family whose
// BenchCase::psi_in is nullopt (legal for a t2/t3 tier, PRD D4/"Edge cases")
// is skipped cleanly inside the test body, not excluded from this list --
// the point is that the retrofit runs over the registry unconditionally.
std::vector<T1Param> AllRegistryT1Params() {
  std::vector<T1Param> out;
  for (const auto& f : bench_families())
    for (const std::int64_t n : {std::int64_t{2}, std::int64_t{3}})
      for (const bool realizable : {false, true})
        out.push_back(T1Param{f->name(), n, realizable});
  return out;
}

class ProducedTraceEquivalenceT1Registry
    : public testing::TestWithParam<T1Param> {};

TEST_P(ProducedTraceEquivalenceT1Registry, CertificateGreenOrCleanSkip) {
  const T1Param& p = GetParam();
  const BenchFamily& family = FamilyNamed(p.family);
  const BenchCase c = family.instantiate(
      BenchParams{{"n", p.n}, {"realizable", p.realizable ? 1 : 0}});

  if (!c.psi_in.has_value()) {
    GTEST_SKIP() << c.family << " (tier " << comparability_tier_name(c.tier)
                 << "): no psi_in declared -- legal for a non-T1 tier (PRD "
                    "D4/\"Edge cases\"), skipped cleanly, not a failure.";
  }
  ASSERT_EQ(c.tier, ComparabilityTier::t1)
      << c.family << ": declares psi_in but its tier is not t1 -- only a T1 "
                     "family may carry psi_in (PRD B3).";

  const spot::formula psi = Phi(*c.psi_in);
  const EquivalenceResult r =
      produced_trace_equivalent(c.t_in, psi, c.vars, Role::t_in);

  // Reported on its own line, never folded into the verdict (pinned #4,
  // Stop-list 5).
  EXPECT_FALSE(r.empty_word_agrees);

  // The retrofit's actual claim. RED here is a finding against c.psi_in,
  // not a test bug -- do not repair by editing psi_in, relaxing this check
  // to containment, or excluding the witness (Stop-list 1, 4).
  EXPECT_TRUE(r.equivalent_on_nonempty)
      << "T1 certificate is RED for family '" << c.family << "' n=" << p.n
      << " realizable=" << p.realizable << ": L(t_in) != L(psi_in). "
      << (r.counterexample.has_value()
              ? ("witness length " +
                std::to_string(r.counterexample->size()))
              : "no witness reported")
      << " -- report this as a finding against the declared psi_in.";
}

INSTANTIATE_TEST_SUITE_P(Registry, ProducedTraceEquivalenceT1Registry,
                         testing::ValuesIn(AllRegistryT1Params()),
                         T1ParamName);

// slippery-binary / slippery-binary-compact at n = 4, kept separate from the
// n in {2, 3} registry sweep above: this pass's scope makes n = 4
// conditional on staying fast, not a uniform registry rule. Both binary
// arms are ~1.6 s/case at n = 4 -- slippery-binary-compact reuses
// slippery-binary's t_in verbatim (D5), so its cost here tracks the same
// number, only its (15-conjunct) psi_in differs. Added by Phase 3
// (docs/prd/engineered-domain-families.md T1: "both goals, n = 2, 3, 4")
// to close the compact arm's own n = 4 cell.
// slippery-onehot at n = 4 is NOT included here: measured (not guessed) --
// it throws std::bad_alloc building ltlf_to_dfa's 2 * 2^4 = 32-AP A_N (D8's
// own edge case note: "one-hot at large n ... n = 6 is 128 APs and may time
// out" -- n = 4 already blows up on memory, not just wall-clock). Dropped
// per this pass's instructions ("n = 4 too if it stays fast -- drop it if
// not, and say so"): slippery-onehot's T1 coverage stops at n = 3 (still
// asserted above, in the n in {2, 3} registry sweep).
std::vector<T1Param> SlipperyN4Params() {
  return {T1Param{"slippery-binary", 4, false},
          T1Param{"slippery-binary", 4, true},
          T1Param{"slippery-binary-compact", 4, false},
          T1Param{"slippery-binary-compact", 4, true}};
}

class ProducedTraceEquivalenceT1SlipperyN4
    : public testing::TestWithParam<T1Param> {};

TEST_P(ProducedTraceEquivalenceT1SlipperyN4, CertificateGreenAtN4) {
  const T1Param& p = GetParam();
  const BenchFamily& family = FamilyNamed(p.family);
  const BenchCase c = family.instantiate(
      BenchParams{{"n", p.n}, {"realizable", p.realizable ? 1 : 0}});
  ASSERT_TRUE(c.psi_in.has_value())
      << p.family << " is a T1 family and must declare psi_in";

  const spot::formula psi = Phi(*c.psi_in);
  const EquivalenceResult r =
      produced_trace_equivalent(c.t_in, psi, c.vars, Role::t_in);

  EXPECT_FALSE(r.empty_word_agrees);
  EXPECT_TRUE(r.equivalent_on_nonempty)
      << "T1 certificate is RED for family '" << c.family << "' n=" << p.n
      << " realizable=" << p.realizable
      << " -- report this as a finding against the declared psi_in.";
}

INSTANTIATE_TEST_SUITE_P(SlipperyN4, ProducedTraceEquivalenceT1SlipperyN4,
                         testing::ValuesIn(SlipperyN4Params()), T1ParamName);

// ---------------------------------------------------------------------------
// T6 -- negative control, on a hand-checkable "slippery-line" toy.
//
// A one-axis, N = 2 (one position bit `bx0`) reduction of the landed
// slippery-world construction (D1/D3): `mv` (move, replacing the four-way
// mvl/mvr/mvu/mvd) and `slip` are Ifree; `bx0` is Iknown. State x commits
// bx0 = (x == 1). delta: !mv keeps; mv steps by 1 cell, or 2 under slip,
// CLAMPED at the wall x = N - 1 = 1 (D1: "walls saturate, they do not
// block") -- both `!slip` and `slip` already reach the wall from cell 0 at
// N = 2, so this stays small while still exercising a genuine slip-only
// rule and a genuine wall-clamp rule.
// ---------------------------------------------------------------------------

VariablePartition SlipperyLineVars() {
  return VariablePartition::split(/*inputs=*/{"bx0", "slip"},
                                  /*outputs=*/{"mv"},
                                  /*governed=*/{"bx0"});
}

OutputLabeledTransducer BuildSlipperyLineTau(const spot::bdd_dict_ptr& dict) {
  auto g = spot::make_twa_graph(dict);
  const int bx0 = g->register_ap("bx0");
  const int mv = g->register_ap("mv");
  const int slip = g->register_ap("slip");
  g->new_states(2);
  g->set_init_state(0);

  const bdd not_mv = bdd_nithvar(mv);
  const bdd mv_noslip = bdd_ithvar(mv) & bdd_nithvar(slip);
  const bdd mv_slip = bdd_ithvar(mv) & bdd_ithvar(slip);

  // state 0 (cell 0): !mv keeps; either mv guard steps to cell 1 (the wall).
  g->new_edge(0, 0, not_mv);
  g->new_edge(0, 1, mv_noslip);
  g->new_edge(0, 1, mv_slip);
  // state 1 (cell 1, the wall): !mv keeps; either mv guard CLAMPS (stays)
  // -- the "Keep at a wall" rule T6(b) mutates.
  g->new_edge(1, 1, not_mv);
  g->new_edge(1, 1, mv_noslip);
  g->new_edge(1, 1, mv_slip);

  return OutputLabeledTransducer(g, {bdd_nithvar(bx0), bdd_ithvar(bx0)},
                                 /*sigma0_cube=*/bddtrue,
                                 /*sigma1_cube=*/bdd_ithvar(bx0));
}

// The correct A_N for BuildSlipperyLineTau: 1 init conjunct + 2 cells x 3
// rules (keep, move-no-slip, move-slip), exactly mirroring
// slippery_assumption()'s per-cell/per-class/per-slip shape (D3/D8).
const char kSlipperyLineGoldenAssumption[] =
    "(!bx0) & "
    "G(((!bx0) & (!mv)) -> X(!bx0)) & "
    "G(((!bx0) & mv & (!slip)) -> X(bx0)) & "
    "G(((!bx0) & mv & slip) -> X(bx0)) & "
    "G((bx0 & (!mv)) -> X(bx0)) & "
    "G((bx0 & mv & (!slip)) -> X(bx0)) & "
    "G((bx0 & mv & slip) -> X(bx0))";

// T6(a): the cell-0 slip rule dropped entirely -- A_N no longer constrains
// what happens on `mv & slip` at cell 0, so it accepts strictly more words
// than the tau actually produces (too weak).
const char kMutantDropSlipCase[] =
    "(!bx0) & "
    "G(((!bx0) & (!mv)) -> X(!bx0)) & "
    "G(((!bx0) & mv & (!slip)) -> X(bx0)) & "
    "G((bx0 & (!mv)) -> X(bx0)) & "
    "G((bx0 & mv & (!slip)) -> X(bx0)) & "
    "G((bx0 & mv & slip) -> X(bx0))";

// T6(b): the wall's Keep rule (`bx0 & mv & !slip -> X(bx0)`, i.e. "stay at
// the wall") retargeted to `X(!bx0)` -- an Inc that ignores the clamp -- so
// A_N now REJECTS the word the tau actually produces at the wall (too
// strong).
const char kMutantWallKeepToInc[] =
    "(!bx0) & "
    "G(((!bx0) & (!mv)) -> X(!bx0)) & "
    "G(((!bx0) & mv & (!slip)) -> X(bx0)) & "
    "G(((!bx0) & mv & slip) -> X(bx0)) & "
    "G((bx0 & (!mv)) -> X(bx0)) & "
    "G((bx0 & mv & (!slip)) -> X(!bx0)) & "
    "G((bx0 & mv & slip) -> X(bx0))";

// Independent reference walkers, mirroring
// tests/produced_trace_equivalence_test.cpp's (not shared -- that file's own
// header comment reserves the oracle set for this separate pass).
bool TauAcceptsWord(const Transducer& tau, const std::vector<bdd>& word) {
  unsigned q = tau.initial_state();
  for (const bdd& v : word) {
    if ((v & tau.emits_region(q)) == bddfalse) return false;
    std::optional<unsigned> next;
    for (const auto& [guard, dst] : tau.delta_edges(q))
      if ((v & guard) != bddfalse) {
        next = dst;
        break;
      }
    if (!next) return false;
    q = *next;
  }
  return true;
}

bool PsiAcceptsWord(const spot::formula& psi, const spot::bdd_dict_ptr& dict,
                    const std::vector<bdd>& word) {
  const spot::twa_graph_ptr dfa = ltlf_to_dfa(psi, dict);
  unsigned s = dfa->get_init_state_number();
  for (const bdd& v : word) {
    const std::optional<unsigned> next = goal_delta(dfa, s, v);
    if (!next) return false;  // ltlf_to_dfa is complete; defensive only.
    s = *next;
  }
  return dfa->state_is_accepting(s);
}

// True iff some non-empty word satisfies `psi`, decided by a plain BFS over
// ltlf_to_dfa(psi) (finite, complete, so this terminates) for a reachable
// accepting state strictly beyond the initial one -- the same non-empty-word
// convention produced_trace_equivalent itself uses (Stop-list 5). Built on
// the public LetterAlphabet + goal_delta surface (product.hpp), not a
// re-implementation of the DFA walk.
bool SatisfiableOnSomeNonEmptyWord(const spot::formula& psi,
                                   const VariablePartition& vars,
                                   const spot::bdd_dict_ptr& dict) {
  const spot::twa_graph_ptr dfa = ltlf_to_dfa(psi, dict);
  const LetterAlphabet alphabet(vars, dfa);
  std::vector<bool> visited(dfa->num_states(), false);
  std::queue<unsigned> frontier;
  const unsigned s0 = dfa->get_init_state_number();
  visited[s0] = true;
  frontier.push(s0);
  while (!frontier.empty()) {
    const unsigned s = frontier.front();
    frontier.pop();
    for (const bdd& v : alphabet.letters()) {
      const std::optional<unsigned> next = goal_delta(dfa, s, v);
      if (!next) continue;
      if (dfa->state_is_accepting(*next)) return true;
      if (!visited[*next]) {
        visited[*next] = true;
        frontier.push(*next);
      }
    }
  }
  return false;
}

// Sanity precondition for T6 below: the hand-written golden A_N really does
// match BuildSlipperyLineTau -- not one of the PRD's numbered oracles, but
// what makes the two mutants below trustworthy (a mutant is only meaningful
// relative to a verified-correct baseline).
TEST(ProducedTraceEquivalenceT6NegativeControl,
    GoldenAssumptionMatchesSlipperyLineTau) {
  auto dict = spot::make_bdd_dict();
  auto vars = SlipperyLineVars();
  auto tau = BuildSlipperyLineTau(dict);

  const EquivalenceResult r = produced_trace_equivalent(
      tau, Phi(kSlipperyLineGoldenAssumption), vars, Role::t_in);

  EXPECT_TRUE(r.equivalent_on_nonempty);
  EXPECT_FALSE(r.counterexample.has_value());
}

TEST(ProducedTraceEquivalenceT6NegativeControl,
    DroppedSlipCaseIsTooWeakAndCaught) {
  auto dict = spot::make_bdd_dict();
  auto vars = SlipperyLineVars();
  auto tau = BuildSlipperyLineTau(dict);
  const spot::formula mutant = Phi(kMutantDropSlipCase);

  ASSERT_TRUE(SatisfiableOnSomeNonEmptyWord(mutant, vars, dict))
      << "T6(a) mutant must stay satisfiable -- dropping a conjunct only "
         "weakens A_N, it must never collapse to false";

  const EquivalenceResult r =
      produced_trace_equivalent(tau, mutant, vars, Role::t_in);

  EXPECT_FALSE(r.equivalent_on_nonempty)
      << "T6(a): dropping the slip case must be CAUGHT, not silently "
         "accepted -- without this the certificate proves nothing";
  ASSERT_TRUE(r.counterexample.has_value());
  const std::vector<bdd>& w = *r.counterexample;
  EXPECT_FALSE(w.empty());
  EXPECT_NE(TauAcceptsWord(tau, w), PsiAcceptsWord(mutant, dict, w))
      << "witness must genuinely disagree between the two independent "
         "reference walkers";
}

TEST(ProducedTraceEquivalenceT6NegativeControl,
    WallKeepReplacedByIncIsTooStrongAndCaught) {
  auto dict = spot::make_bdd_dict();
  auto vars = SlipperyLineVars();
  auto tau = BuildSlipperyLineTau(dict);
  const spot::formula mutant = Phi(kMutantWallKeepToInc);

  ASSERT_TRUE(SatisfiableOnSomeNonEmptyWord(mutant, vars, dict))
      << "T6(b) mutant must stay satisfiable -- e.g. never reaching the "
         "wall trivially satisfies it";

  const EquivalenceResult r =
      produced_trace_equivalent(tau, mutant, vars, Role::t_in);

  EXPECT_FALSE(r.equivalent_on_nonempty)
      << "T6(b): replacing Keep at the wall with Inc must be CAUGHT, not "
         "silently accepted -- without this the certificate proves nothing";
  ASSERT_TRUE(r.counterexample.has_value());
  const std::vector<bdd>& w = *r.counterexample;
  EXPECT_FALSE(w.empty());
  EXPECT_NE(TauAcceptsWord(tau, w), PsiAcceptsWord(mutant, dict, w))
      << "witness must genuinely disagree between the two independent "
         "reference walkers";
}

}  // namespace
