#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>
#include <bddx.h>
#include <spot/tl/parse.hh>
#include <spot/twa/bdddict.hh>
#include <spot/twa/twagraph.hh>

#include "ltlf_ek/ltlf_to_dfa.hpp"
#include "ltlf_ek/output_labeled_transducer.hpp"
#include "ltlf_ek/product.hpp"
#include "ltlf_ek/produced_trace_equivalence.hpp"
#include "ltlf_ek/role.hpp"
#include "ltlf_ek/transducer.hpp"
#include "ltlf_ek/variables.hpp"

// Unit tests for produced_trace_equivalent, against the FROZEN interface in
// docs/prd/engineered-domain-families.md "New library API --- Produced-trace
// equivalence".  Scope (per that PRD's Phase 2): the API's own behaviour ---
// degenerate psi = true/false, a nowhere-defined tau, one hand-built positive and
// one hand-built negative case with the witness checked for shortest-ness/
// determinism.  The full oracle set (T1 certificate over every landed T1
// family, T6's two-mutant negative control) is a separate /test-writer pass
// against the engineered-domain-families families, not duplicated here.
namespace {

using ltlf_ek::EquivalenceResult;
using ltlf_ek::goal_delta;
using ltlf_ek::ltlf_to_dfa;
using ltlf_ek::OutputLabeledTransducer;
using ltlf_ek::produced_trace_equivalent;
using ltlf_ek::Role;
using ltlf_ek::Transducer;
using ltlf_ek::trivial_transducer;
using ltlf_ek::VariablePartition;

spot::formula Phi(const std::string& s) { return spot::parse_formula(s); }

// ---------------------------------------------------------------------------
// Independent reference walkers --- never touch produced_trace_equivalent's
// own internals, so they can check its output rather than assume it.
// ---------------------------------------------------------------------------

// Mirrors tests/emits_dfa_test.cpp's ReferenceAccepts: true iff tau's run on
// `word` stays delta-defined and lambda-agreeing throughout --- exactly
// emits_dfa(tau)'s language, computed straight off the Transducer interface.
bool TauAcceptsWord(const Transducer& tau, const std::vector<bdd>& word) {
  unsigned q = tau.initial_state();
  for (const bdd& v : word) {
    if ((v & tau.emits_region(q)) == bddfalse) return false;
    std::optional<unsigned> next;
    for (const auto& [g, d] : tau.delta_edges(q))
      if ((v & g) != bddfalse) { next = d; break; }
    if (!next) return false;
    q = *next;
  }
  return true;
}

// Walks a freshly-built ltlf_to_dfa(psi) on `word` via the public goal_delta
// helper --- same public surface produced_trace_equivalent itself uses, but
// a separately-built automaton, not the one hidden inside that call.
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

// A single-AP {i} partition with i KNOWN (Iknown = {i}), no free/output APs
// --- the smallest partition Role::t_in is meaningful over.
VariablePartition SingleKnownInputVars() {
  return VariablePartition::split(/*inputs=*/{"i"}, /*outputs=*/{},
                                  /*governed=*/{"i"});
}

// A single-AP {i} partition with i FREE (Iknown = empty, i not governed) ---
// the smallest partition `trivial_transducer(vars, Role::t_in, ...)` accepts
// (it throws unless the role's known set is empty).
VariablePartition SingleFreeInputVars() {
  return VariablePartition::split(/*inputs=*/{"i"}, /*outputs=*/{},
                                  /*governed=*/{});
}

// tau: state0 commits i=true, unconditionally moves to state1; state1
// commits i=false forever (self-loop).  Sigma0 = Ifree = empty (lambda is
// total, ignores nothing --- there is nothing to observe), Sigma1 = Iknown =
// {i}.  Produced-trace language over non-empty words: w[0] has i=true, and
// w[t] has i=false for every t >= 1 --- exactly "i & X(G(!i))" under this
// project's weak-X, empty-continuation-vacuously-true LTLf convention.
OutputLabeledTransducer BuildITrueThenFalseForever(
    const spot::bdd_dict_ptr& dict) {
  auto g = spot::make_twa_graph(dict);
  const int i = g->register_ap("i");
  g->new_states(2);
  g->set_init_state(0);
  g->new_edge(0, 1, bddtrue);
  g->new_edge(1, 1, bddtrue);
  return OutputLabeledTransducer(g, {bdd_ithvar(i), bdd_nithvar(i)},
                                 /*sigma0_cube=*/bddtrue,
                                 /*sigma1_cube=*/bdd_ithvar(i));
}

// tau whose delta is nowhere defined --- one state, zero out-edges (pinned
// behaviour #7: "A tau whose delta is nowhere defined").
OutputLabeledTransducer BuildNowhereDefinedTau(const spot::bdd_dict_ptr& dict) {
  auto g = spot::make_twa_graph(dict);
  const int i = g->register_ap("i");
  g->new_states(1);
  g->set_init_state(0);
  // No new_edge calls at all: delta_edges(0) is empty.
  return OutputLabeledTransducer(g, {bdd_ithvar(i)}, /*sigma0_cube=*/bddtrue,
                                 /*sigma1_cube=*/bdd_ithvar(i));
}

// ---------------------------------------------------------------------------
// Degenerate psi = true / false (pinned behaviour #7).
// ---------------------------------------------------------------------------

TEST(ProducedTraceEquivalence, PsiTrueAgainstRealTauDecidesNormally) {
  auto dict = spot::make_bdd_dict();
  auto vars = SingleKnownInputVars();
  auto tau = BuildITrueThenFalseForever(dict);

  const EquivalenceResult r =
      produced_trace_equivalent(tau, Phi("true"), vars, Role::t_in);

  // tau's language is a strict subset of "every non-empty word" (true), so a
  // real disagreement must be found, not a crash or a vacuous verdict.
  EXPECT_FALSE(r.equivalent_on_nonempty);
  ASSERT_TRUE(r.counterexample.has_value());
  EXPECT_GE(r.tau_dfa_states, 1u);
  EXPECT_GE(r.psi_dfa_states, 1u);
}

TEST(ProducedTraceEquivalence, PsiFalseAgainstRealTauDecidesNormally) {
  auto dict = spot::make_bdd_dict();
  auto vars = SingleKnownInputVars();
  auto tau = BuildITrueThenFalseForever(dict);

  const EquivalenceResult r =
      produced_trace_equivalent(tau, Phi("false"), vars, Role::t_in);

  // false's non-empty language is empty, tau's is not (e.g. "i" alone
  // satisfies it), so this must also disagree.
  EXPECT_FALSE(r.equivalent_on_nonempty);
  ASSERT_TRUE(r.counterexample.has_value());
}

// ---------------------------------------------------------------------------
// Nowhere-defined tau (pinned behaviour #7).
// ---------------------------------------------------------------------------

TEST(ProducedTraceEquivalence, NowhereDefinedTauAgreesWithFalse) {
  auto dict = spot::make_bdd_dict();
  auto vars = SingleKnownInputVars();
  auto tau = BuildNowhereDefinedTau(dict);

  const EquivalenceResult r =
      produced_trace_equivalent(tau, Phi("false"), vars, Role::t_in);

  // Both sides have an empty non-empty-word language: "the comparison is
  // still exact" (pinned behaviour #7).
  EXPECT_TRUE(r.equivalent_on_nonempty);
  EXPECT_FALSE(r.counterexample.has_value());
  EXPECT_GE(r.tau_dfa_states, 1u);
}

TEST(ProducedTraceEquivalence, NowhereDefinedTauDisagreesWithTrue) {
  auto dict = spot::make_bdd_dict();
  auto vars = SingleKnownInputVars();
  auto tau = BuildNowhereDefinedTau(dict);

  const EquivalenceResult r =
      produced_trace_equivalent(tau, Phi("true"), vars, Role::t_in);

  // true's non-empty language is everything; tau's is empty --- must disagree,
  // and as short as a witness can possibly be (the very first letter already
  // has no tau edge to follow).
  EXPECT_FALSE(r.equivalent_on_nonempty);
  ASSERT_TRUE(r.counterexample.has_value());
  EXPECT_EQ(r.counterexample->size(), 1u);
}

// ---------------------------------------------------------------------------
// Empty word: always reported, never folded into the verdict (pinned #4).
// ---------------------------------------------------------------------------

TEST(ProducedTraceEquivalence, EmptyWordAlwaysDisagreesAndNeverAffectsVerdict) {
  auto dict = spot::make_bdd_dict();
  auto vars = SingleKnownInputVars();
  auto tau = BuildITrueThenFalseForever(dict);

  const EquivalenceResult matching =
      produced_trace_equivalent(tau, Phi("i & X(G(!i))"), vars, Role::t_in);
  EXPECT_TRUE(matching.equivalent_on_nonempty);
  EXPECT_FALSE(matching.empty_word_agrees)
      << "emits_dfa's initial state is final by construction while the "
        "repo's LTLf convention rejects the empty word --- always false, "
        "even when the non-empty verdict is a clean equivalence";
}

// ---------------------------------------------------------------------------
// Hand-built positive case.
// ---------------------------------------------------------------------------

TEST(ProducedTraceEquivalence, MatchingPsiIsEquivalent) {
  auto dict = spot::make_bdd_dict();
  auto vars = SingleKnownInputVars();
  auto tau = BuildITrueThenFalseForever(dict);

  const EquivalenceResult r =
      produced_trace_equivalent(tau, Phi("i & X(G(!i))"), vars, Role::t_in);

  EXPECT_TRUE(r.equivalent_on_nonempty);
  EXPECT_FALSE(r.counterexample.has_value());
}

// ---------------------------------------------------------------------------
// Hand-built negative case, with the witness checked for shortest-ness
// (no strictly shorter prefix already disagrees, verified independently)
// and determinism (a second call returns a byte-identical witness).
// ---------------------------------------------------------------------------

TEST(ProducedTraceEquivalence, MismatchingPsiWitnessIsShortestAndDeterministic) {
  auto dict = spot::make_bdd_dict();
  auto vars = SingleKnownInputVars();
  auto tau = BuildITrueThenFalseForever(dict);
  // Too STRONG: requires i forever after t=0, instead of !i forever after
  // t=0 --- the same flavour of mutant as T6(b) in docs/prd/
  // engineered-domain-families.md (Keep -> Inc at a wall).
  const spot::formula psi_wrong = Phi("i & X(G(i))");

  const EquivalenceResult r1 =
      produced_trace_equivalent(tau, psi_wrong, vars, Role::t_in);
  ASSERT_FALSE(r1.equivalent_on_nonempty);
  ASSERT_TRUE(r1.counterexample.has_value());
  const std::vector<bdd>& w = *r1.counterexample;
  ASSERT_FALSE(w.empty());

  // The full witness genuinely disagrees between the two independent
  // reference walkers.
  EXPECT_NE(TauAcceptsWord(tau, w), PsiAcceptsWord(psi_wrong, dict, w));

  // Shortest: strip the last letter and confirm the two sides still AGREE
  // there --- i.e. no strictly shorter non-empty prefix already disagrees.
  if (w.size() > 1) {
    const std::vector<bdd> prefix(w.begin(), w.end() - 1);
    EXPECT_EQ(TauAcceptsWord(tau, prefix), PsiAcceptsWord(psi_wrong, dict, prefix))
        << "witness of length " << w.size()
        << " is not shortest: a strictly shorter prefix already disagrees";
  }

  // Deterministic: a second, independent call returns the identical witness.
  const EquivalenceResult r2 =
      produced_trace_equivalent(tau, psi_wrong, vars, Role::t_in);
  ASSERT_TRUE(r2.counterexample.has_value());
  ASSERT_EQ(r2.counterexample->size(), w.size());
  for (std::size_t idx = 0; idx < w.size(); ++idx)
    EXPECT_EQ((*r2.counterexample)[idx], w[idx]) << "at index " << idx;
}

// ---------------------------------------------------------------------------
// Regression (code-review, 2026-08-21): a non-empty word that returns the
// walk to the INITIAL product pair must still be checked, not pruned by a
// pre-seeded `visited`. tau = trivial_transducer (L(tau) = Sigma+, a single
// state that self-loops on every letter) against psi = F(i): the word "!i"
// alone is in L(tau) but not in L(F(i)), and tau's/psi's product returns to
// the initial pair after that one letter --- exactly the pair a pre-seeded
// `visited` would have swallowed, silently reporting a false equivalence.
// ---------------------------------------------------------------------------

TEST(ProducedTraceEquivalence,
    NonEmptyWordReturningToInitialPairIsCheckedNotPruned) {
  auto dict = spot::make_bdd_dict();
  auto vars = SingleFreeInputVars();
  auto tau = trivial_transducer(vars, Role::t_in, dict);

  const EquivalenceResult r =
      produced_trace_equivalent(tau, Phi("F(i)"), vars, Role::t_in);

  ASSERT_FALSE(r.equivalent_on_nonempty)
      << "tau accepts every non-empty word (L(tau) = Sigma+), F(i) does not "
        "accept a word where i never holds --- must diverge";
  ASSERT_TRUE(r.counterexample.has_value());
  const std::vector<bdd>& w = *r.counterexample;

  // Independent check: the witness genuinely disagrees between tau and psi.
  EXPECT_NE(TauAcceptsWord(tau, w), PsiAcceptsWord(Phi("F(i)"), dict, w));

  // Shortest possible: a single letter already returns the walk to the
  // initial pair with differing finality --- no strictly shorter non-empty
  // word exists to disagree on.
  EXPECT_EQ(w.size(), 1u);
}

}  // namespace
