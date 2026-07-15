#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>
#include <bddx.h>
#include <spot/twa/bdddict.hh>
#include <spot/twa/twagraph.hh>
#include <spot/twaalgos/isdet.hh>
#include <spot/twaalgos/ltlf2dfa.hh>

#include "ltlf_ek/emits_dfa.hpp"
#include "ltlf_ek/output_labeled_transducer.hpp"
#include "ltlf_ek/role.hpp"
#include "ltlf_ek/transducer.hpp"
#include "ltlf_ek/variables.hpp"

// Unit fixtures for emits_dfa (docs/prd/mtdfa-product.md "emits_dfa --- pinned
// specification", "Interfaces & types", "Test oracles" #2).  Written against
// the FROZEN header (include/ltlf_ek/emits_dfa.hpp) before its implementation
// lands (concurrent workflow, docs/prd/mtdfa-product.md "Phase 1"): this file
// will not compile/link until that header and src/emits_dfa.cpp exist --- the
// /developer worktree lands them; a build failure here before integration is
// expected, not a bug in this file.
//
// Phase 0/Q1 absorption trap (PRD "Test oracles" #2): twadfa_to_mtdfa absorbs
// a NON-INITIAL state with an ACCEPTING bddtrue self-loop into the bddtrue
// constant, so a fixture whose only accepting structure is such a sink
// round-trips under either acceptance convention and tests nothing --- this
// produced a confident wrong answer in Phase 0.  Every fixture below gives
// its non-sink states letter-DEPENDENT out-edges (never a bddtrue
// self-loop on a non-initial accepting state), and the language-equivalence
// oracle (LanguageMatchesTheRunOfTauClaim) is paired with a deliberate
// negative control (LanguageOracleIsDiscriminating) that must disagree
// somewhere, so a vacuous oracle would be caught, mirroring Phase 0's own
// G(a)-vs-F(a) discriminating check.
namespace {

using ltlf_ek::emits_dfa;
using ltlf_ek::OutputLabeledTransducer;
using ltlf_ek::Role;
using ltlf_ek::Transducer;
using ltlf_ek::VariablePartition;

// ---------------------------------------------------------------------------
// Shared alphabet / word-enumeration helpers over a 2-variable domain {a, k}.
// ---------------------------------------------------------------------------

std::vector<bdd> AllLettersOverTwoVars(int a, int k) {
  return {bdd_ithvar(a) & bdd_ithvar(k), bdd_ithvar(a) & bdd_nithvar(k),
          bdd_nithvar(a) & bdd_ithvar(k), bdd_nithvar(a) & bdd_nithvar(k)};
}

// Every word (including the empty word) of length 0..max_len over `alphabet`
// --- small brute-force enumeration, no seed, deterministic (mirrors
// tests/ltlfsynt_oracle_test.cpp's ifree_sequences_of_length idiom at a
// 2-variable / short-length scale where exhaustive enumeration is cheap).
std::vector<std::vector<bdd>> AllWordsUpToLength(
    const std::vector<bdd>& alphabet, unsigned max_len) {
  std::vector<std::vector<bdd>> all{{}};
  std::vector<std::vector<bdd>> frontier{{}};
  for (unsigned len = 1; len <= max_len; ++len) {
    std::vector<std::vector<bdd>> next;
    for (const auto& w : frontier)
      for (const bdd& letter : alphabet) {
        std::vector<bdd> nw = w;
        nw.push_back(letter);
        next.push_back(nw);
      }
    all.insert(all.end(), next.begin(), next.end());
    frontier = next;
  }
  return all;
}

// ---------------------------------------------------------------------------
// Reference predicates over `tau` alone (never touch emits_dfa's output):
// the pinned "unit oracle" language claim (PRD "emits_dfa --- pinned
// specification", "Language claim (the unit oracle)") --- "the run of tau on
// w is defined and every letter agrees with lambda at its state" --- computed
// directly from Transducer::delta_edges / Transducer::emits_region, never by
// re-deriving emits_dfa's own terminal/sink-construction algorithm.
// ---------------------------------------------------------------------------

bool ReferenceAccepts(const Transducer& tau, const std::vector<bdd>& word) {
  unsigned q = tau.initial_state();
  for (const bdd& v : word) {
    if ((v & tau.emits_region(q)) == bddfalse) return false;
    std::optional<unsigned> next;
    for (const auto& [g, d] : tau.delta_edges(q))
      if ((v & g) != bddfalse) {
        next = d;
        break;
      }
    if (!next) return false;
    q = *next;
  }
  return true;
}

// Deliberately WRONG reference (negative control): drops the lambda-agrees
// check entirely, "accepting" as long as delta stays defined.  If the real
// language-equivalence check below were vacuous (e.g. because every fixture's
// accepting structure round-trips regardless of the lambda test --- exactly
// the Q1 absorption hazard), this wrong predicate would never be caught
// disagreeing with emits_dfa's graph either.  LanguageOracleIsDiscriminating
// asserts it IS caught, i.e. that the oracle actually exercises the lambda
// check.
bool WrongReferenceIgnoringLambda(const Transducer& tau,
                                  const std::vector<bdd>& word) {
  unsigned q = tau.initial_state();
  for (const bdd& v : word) {
    std::optional<unsigned> next;
    for (const auto& [g, d] : tau.delta_edges(q))
      if ((v & g) != bddfalse) {
        next = d;
        break;
      }
    if (!next) return false;
    q = *next;
  }
  return true;
}

// Walks emits_dfa's OWN graph (never touches tau): requires --- and, via the
// separate DeterministicAndComplete test below, confirms --- that the graph
// is deterministic and complete, so exactly one out-edge always matches.
bool GraphAccepts(const spot::twa_graph_ptr& g, const std::vector<bdd>& word) {
  unsigned s = g->get_init_state_number();
  for (const bdd& v : word) {
    std::optional<unsigned> next;
    for (const auto& e : g->out(s)) {
      if ((v & e.cond) != bddfalse) {
        next = e.dst;
        break;
      }
    }
    if (!next)
      throw std::runtime_error(
          "GraphAccepts: emits_dfa's result is expected complete");
    s = *next;
  }
  return g->state_is_accepting(s);
}

// ---------------------------------------------------------------------------
// Fixture A: "AgreesWithPreviousLetter" --- partial lambda, sink reachable
// (PRD "Test oracles" #2 bullet "partial lambda (sink reachable)").  Two
// states track whether the PREVIOUS letter's `a` was true; lambda commits k
// to match that tracked bit, independent of the CURRENT `a` (Sigma0 = empty,
// Sigma1 = {k}).  Neither state's out-edges are a bddtrue self-loop (both
// depend on `a`), so this dodges the Q1 absorption trap by construction, and
// doubles as the PRD's suggested "last letter is a" shape (here: "does k
// equal whether the letter before last had a").
//   state 0 (committed k=false): -- a --> state 1, -- !a --> state 0
//   state 1 (committed k=true):  -- a --> state 1, -- !a --> state 0
struct EmitsDfaFixture {
  spot::bdd_dict_ptr dict;
  OutputLabeledTransducer tau;
  int a_var;
  int k_var;
};

EmitsDfaFixture BuildAgreesWithPreviousLetterFixture() {
  auto dict = spot::make_bdd_dict();
  auto g = spot::make_twa_graph(dict);
  const int a = g->register_ap("a");
  const int k = g->register_ap("k");
  g->new_states(2);
  g->set_init_state(0);
  g->new_edge(0, 1, bdd_ithvar(a));
  g->new_edge(0, 0, bdd_nithvar(a));
  g->new_edge(1, 1, bdd_ithvar(a));
  g->new_edge(1, 0, bdd_nithvar(a));
  OutputLabeledTransducer tau(g, {bdd_nithvar(k), bdd_ithvar(k)},
                             /*sigma0_cube=*/bddtrue,
                             /*sigma1_cube=*/bdd_ithvar(k));
  return {dict, std::move(tau), a, k};
}

// Fixture B: "TotalPermissive" --- total lambda (PRD bullet "total lambda"),
// defined and unconstrained at every state, so covered(q) = bddtrue at both
// states and emits_dfa must skip creating the sink state entirely (PRD
// "emits_dfa --- pinned specification" "Sink state: create it only if...").
// Same delta shape as fixture A so it is not merely the trivial single-state
// case (that is fixture D, below).
EmitsDfaFixture BuildTotalPermissiveFixture() {
  auto dict = spot::make_bdd_dict();
  auto g = spot::make_twa_graph(dict);
  const int a = g->register_ap("a");
  const int k = g->register_ap("k");
  g->new_states(2);
  g->set_init_state(0);
  g->new_edge(0, 1, bdd_ithvar(a));
  g->new_edge(0, 0, bdd_nithvar(a));
  g->new_edge(1, 1, bdd_ithvar(a));
  g->new_edge(1, 0, bdd_nithvar(a));
  OutputLabeledTransducer tau(g, {bddtrue, bddtrue}, /*sigma0_cube=*/bddtrue,
                             /*sigma1_cube=*/bdd_ithvar(k));
  return {dict, std::move(tau), a, k};
}

// Fixture C: "UndefinedAtState" --- lambda undefined at a state (PRD "Edge
// cases": "lambda undefined at q => emits_region(q) = bddfalse => every edge
// from q is bddfalse and is skipped => not-covered(q) = bddtrue => a single
// bddtrue edge to the sink").  Single state, lambda_by_state[0] = bddfalse.
EmitsDfaFixture BuildUndefinedAtStateFixture() {
  auto dict = spot::make_bdd_dict();
  auto g = spot::make_twa_graph(dict);
  const int a = g->register_ap("a");
  const int k = g->register_ap("k");
  g->new_states(1);
  g->set_init_state(0);
  g->new_edge(0, 0, bddtrue);  // any delta edge; ANDed with bddfalse below.
  OutputLabeledTransducer tau(g, {bddfalse}, /*sigma0_cube=*/bddtrue,
                             /*sigma1_cube=*/bdd_ithvar(k));
  return {dict, std::move(tau), a, k};
}

// Fixture D: "SingleState" --- the trivial transducer (PRD bullet "single-
// state transducer"), reusing the library factory per tests/support/
// fixtures.hpp's stated policy ("prefer the library factory over a
// fixture").  Sigma1 = Iknown = empty, so lambda commits the empty cube
// (bddtrue): covered(0) = bddtrue, no sink, single accepting state.
EmitsDfaFixture BuildSingleStateFixture() {
  auto dict = spot::make_bdd_dict();
  auto part = VariablePartition::split({"a"}, /*outputs=*/{}, /*governed=*/{});
  OutputLabeledTransducer tau = ltlf_ek::trivial_transducer(part, Role::t_in, dict);
  auto probe = spot::make_twa_graph(dict);
  const int a = probe->register_ap("a");
  return {dict, std::move(tau), a, /*k_var=*/-1};
}

// ---------------------------------------------------------------------------
// Structural free-riders: determinism + completeness (PRD "emits_dfa ---
// pinned specification": "deterministic and complete by construction").
// ---------------------------------------------------------------------------

TEST(EmitsDfa, AgreesWithPreviousLetterIsDeterministicAndComplete) {
  const EmitsDfaFixture f = BuildAgreesWithPreviousLetterFixture();
  const spot::twa_graph_ptr g = emits_dfa(f.tau, f.dict);
  EXPECT_TRUE(spot::is_deterministic(g));
  EXPECT_TRUE(spot::is_complete(g));
}

TEST(EmitsDfa, TotalPermissiveIsDeterministicAndComplete) {
  const EmitsDfaFixture f = BuildTotalPermissiveFixture();
  const spot::twa_graph_ptr g = emits_dfa(f.tau, f.dict);
  EXPECT_TRUE(spot::is_deterministic(g));
  EXPECT_TRUE(spot::is_complete(g));
}

TEST(EmitsDfa, UndefinedAtStateIsDeterministicAndComplete) {
  const EmitsDfaFixture f = BuildUndefinedAtStateFixture();
  const spot::twa_graph_ptr g = emits_dfa(f.tau, f.dict);
  EXPECT_TRUE(spot::is_deterministic(g));
  EXPECT_TRUE(spot::is_complete(g));
}

TEST(EmitsDfa, SingleStateIsDeterministicAndComplete) {
  const EmitsDfaFixture f = BuildSingleStateFixture();
  const spot::twa_graph_ptr g = emits_dfa(f.tau, f.dict);
  EXPECT_TRUE(spot::is_deterministic(g));
  EXPECT_TRUE(spot::is_complete(g));
}

// The runtime precondition emits_dfa's own doc comment calls out (Phase
// 0/Q1, ltlf2dfa.cc:2958): twadfa_to_mtdfa THROWS on a non-deterministic
// input.  Confirming it does NOT throw here is a direct check that
// emits_dfa's "deterministic by construction" claim actually holds at
// runtime, not just on paper --- across every fixture, since Q1 also pins
// that prop_state_acc(true) must be called explicitly (twadfa_to_mtdfa
// branches on that property).
TEST(EmitsDfa, LiftsThroughTwadfaToMtdfaWithoutThrowing) {
  for (auto build : {&BuildAgreesWithPreviousLetterFixture,
                     &BuildTotalPermissiveFixture, &BuildUndefinedAtStateFixture,
                     &BuildSingleStateFixture}) {
    const EmitsDfaFixture f = build();
    const spot::twa_graph_ptr g = emits_dfa(f.tau, f.dict);
    EXPECT_NO_THROW(spot::twadfa_to_mtdfa(g));
  }
}

// ---------------------------------------------------------------------------
// Acceptance shape (PRD "emits_dfa --- pinned specification" "Acceptance").
// ---------------------------------------------------------------------------

TEST(EmitsDfa, MarksStateBasedAcceptanceExplicitly) {
  const EmitsDfaFixture f = BuildAgreesWithPreviousLetterFixture();
  const spot::twa_graph_ptr g = emits_dfa(f.tau, f.dict);
  EXPECT_TRUE(static_cast<bool>(g->prop_state_acc()));
}

// "emits_dfa accepts the empty word" (PRD "Edge cases"): the initial state is
// always one of tau's own states (never the sink), and every one of tau's
// states is accepting --- so the initial state must be accepting for every
// fixture, independent of tau's own structure.
TEST(EmitsDfa, AcceptsTheEmptyWordAcrossEveryFixture) {
  for (auto build : {&BuildAgreesWithPreviousLetterFixture,
                     &BuildTotalPermissiveFixture, &BuildUndefinedAtStateFixture,
                     &BuildSingleStateFixture}) {
    const EmitsDfaFixture f = build();
    const spot::twa_graph_ptr g = emits_dfa(f.tau, f.dict);
    EXPECT_TRUE(g->state_is_accepting(g->get_init_state_number()));
  }
}

// Fixture B: no sink is ever needed (PRD "Sink state: create it only if some
// not-covered(q) edge is actually added") --- exactly tau's own state count,
// no extra state, and the WHOLE language is accepted (nothing is ever
// rejected).
TEST(EmitsDfa, TotalPermissiveCreatesNoSinkState) {
  const EmitsDfaFixture f = BuildTotalPermissiveFixture();
  const spot::twa_graph_ptr g = emits_dfa(f.tau, f.dict);
  EXPECT_EQ(g->num_states(), 2u);
  const std::vector<bdd> alphabet = AllLettersOverTwoVars(f.a_var, f.k_var);
  for (const auto& word : AllWordsUpToLength(alphabet, 4))
    EXPECT_TRUE(GraphAccepts(g, word));
}

// Fixture C: lambda undefined at the (only) state routes EVERY letter to a
// single, non-accepting sink (PRD "Edge cases").
TEST(EmitsDfa, UndefinedAtStateRoutesEveryLetterToARejectingSink) {
  const EmitsDfaFixture f = BuildUndefinedAtStateFixture();
  const spot::twa_graph_ptr g = emits_dfa(f.tau, f.dict);
  ASSERT_EQ(g->num_states(), 2u);
  const unsigned init = g->get_init_state_number();
  const auto out = g->out(init);
  auto it = out.begin();
  ASSERT_NE(it, out.end());
  EXPECT_EQ(it->cond, bddtrue);
  const unsigned sink = it->dst;
  EXPECT_NE(sink, init);
  EXPECT_FALSE(g->state_is_accepting(sink));
  ++it;
  EXPECT_EQ(it, out.end()) << "state 0 must have exactly one out-edge";
  // Every non-empty word is rejected --- the sink is reached on the very
  // first letter and never leaves (self-loop, PRD "Sink edge").
  const std::vector<bdd> alphabet = AllLettersOverTwoVars(f.a_var, f.k_var);
  for (const bdd& letter : alphabet)
    EXPECT_FALSE(GraphAccepts(g, {letter}));
}

// Fixture D: exactly one state, accepting, self-looping on bddtrue --- the
// initial-state EXEMPTION from the Q1 absorption rule (PRD "Two hazards
// found on the way": "the initial state is exempt (i == init is tested
// first)") applies here, since this fixture's only state IS the initial
// state.
TEST(EmitsDfa, SingleStateIsOneAcceptingSelfLoopingState) {
  const EmitsDfaFixture f = BuildSingleStateFixture();
  const spot::twa_graph_ptr g = emits_dfa(f.tau, f.dict);
  ASSERT_EQ(g->num_states(), 1u);
  const unsigned init = g->get_init_state_number();
  EXPECT_TRUE(g->state_is_accepting(init));
  const auto out = g->out(init);
  auto it = out.begin();
  ASSERT_NE(it, out.end());
  EXPECT_EQ(it->cond, bddtrue);
  EXPECT_EQ(it->dst, init);
}

// ---------------------------------------------------------------------------
// The language-equivalence oracle (PRD "Test oracles" #2, "the language
// claim above, per fixture") + its discriminating negative control.
// ---------------------------------------------------------------------------

TEST(EmitsDfa, LanguageMatchesTheRunOfTauClaim) {
  for (auto build : {&BuildAgreesWithPreviousLetterFixture,
                     &BuildTotalPermissiveFixture, &BuildUndefinedAtStateFixture}) {
    const EmitsDfaFixture f = build();
    const spot::twa_graph_ptr g = emits_dfa(f.tau, f.dict);
    const std::vector<bdd> alphabet = AllLettersOverTwoVars(f.a_var, f.k_var);
    for (const auto& word : AllWordsUpToLength(alphabet, 4)) {
      SCOPED_TRACE("word length " + std::to_string(word.size()));
      EXPECT_EQ(GraphAccepts(g, word), ReferenceAccepts(f.tau, word))
          << "emits_dfa's language diverges from \"the run of tau is defined "
             "and every letter agrees with lambda at its state\"";
    }
  }
}

// Negative control (PRD "Test oracles" #2: "pair any language-equivalence
// oracle with a negative control that must fail"): a WRONG reference that
// drops the lambda-agrees check must be CAUGHT disagreeing with emits_dfa's
// real graph on fixture A, whose sink is genuinely reachable --- proving the
// language oracle above is not vacuously true regardless of what lambda
// says (the exact Q1 absorption failure mode).
TEST(EmitsDfa, LanguageOracleIsDiscriminating) {
  const EmitsDfaFixture f = BuildAgreesWithPreviousLetterFixture();
  const spot::twa_graph_ptr g = emits_dfa(f.tau, f.dict);
  const std::vector<bdd> alphabet = AllLettersOverTwoVars(f.a_var, f.k_var);
  bool any_mismatch = false;
  for (const auto& word : AllWordsUpToLength(alphabet, 4)) {
    if (GraphAccepts(g, word) != WrongReferenceIgnoringLambda(f.tau, word)) {
      any_mismatch = true;
      break;
    }
  }
  EXPECT_TRUE(any_mismatch)
      << "negative control never disagreed with emits_dfa's own graph --- "
         "the language-equivalence oracle would be vacuous";
}

}  // namespace
