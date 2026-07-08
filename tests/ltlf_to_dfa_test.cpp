#include <set>
#include <string>

#include <gtest/gtest.h>
#include <spot/tl/parse.hh>
#include <spot/twa/bdddict.hh>
#include <spot/twa/twagraph.hh>
#include <spot/twaalgos/isdet.hh>

#include "ltlf_ek/ltlf_to_dfa.hpp"
#include "support/fixtures.hpp"

// Unit fixtures for ltlf_to_dfa (docs/GLOSSARY.md: "Goal DFA construction
// (LtlfToDfa)").  The wrapper must yield the deterministic, complete DFA A for
// phi on the *given* bdd_dict, with finiteness carried in acceptance marks (no
// extra AP) and accepting states = final states F_D.
namespace {

using ltlf_ek::ltlf_to_dfa;
using ltlf_ek::test_support::Phi;

std::set<std::string> ap_names(const spot::twa_graph_ptr& dfa) {
  std::set<std::string> names;
  for (const spot::formula& ap : dfa->ap()) names.insert(ap.ap_name());
  return names;
}

// Is there a reachable accepting (final) state?  A rejecting DFA (trivially
// false phi) has none.
bool has_reachable_accepting(const spot::twa_graph_ptr& dfa) {
  for (unsigned s = 0; s < dfa->num_states(); ++s)
    if (dfa->state_is_accepting(s)) return true;
  return false;
}

TEST(LtlfToDfa, BuiltOnTheGivenDict) {
  auto dict = spot::make_bdd_dict();
  auto dfa = ltlf_to_dfa(Phi("a & X[!] b"), dict);
  EXPECT_EQ(dfa->get_dict(), dict);
}

// Finiteness lives in acceptance marks, not an extra "alive"/sink AP: the
// alphabet stays exactly the formula's I∪O variables.
TEST(LtlfToDfa, AlphabetHasNoExtraAtomicProposition) {
  auto dict = spot::make_bdd_dict();
  auto dfa = ltlf_to_dfa(Phi("a & X[!] b"), dict);
  EXPECT_EQ(ap_names(dfa), (std::set<std::string>{"a", "b"}));
}

// alg:dfa_product's product loop reads delta_D(s, v) for every letter and
// navigates a unique edge: the DFA must be deterministic and complete.
TEST(LtlfToDfa, IsDeterministicAndComplete) {
  auto dict = spot::make_bdd_dict();
  auto dfa = ltlf_to_dfa(Phi("a U b"), dict);
  EXPECT_TRUE(spot::is_deterministic(dfa));
  EXPECT_TRUE(spot::is_complete(dfa));
}

TEST(LtlfToDfa, UsesStateBasedAcceptance) {
  auto dict = spot::make_bdd_dict();
  auto dfa = ltlf_to_dfa(Phi("a"), dict);
  // state_is_accepting (used to classify F_D in the product) requires it.
  EXPECT_TRUE(static_cast<bool>(dfa->prop_state_acc()));
}

// LTLf traces are non-empty (main.tex §85: models range over (2^{I∪O})^+), so
// even trivially-true phi does NOT accept the empty word: the initial state (the
// empty word read so far) is non-accepting, yet every one-letter word satisfies
// "1", so an accepting state is reachable after a single step.
TEST(LtlfToDfa, TriviallyTrueRejectsEmptyWordButAcceptsAfterOneStep) {
  auto dict = spot::make_bdd_dict();
  auto dfa = ltlf_to_dfa(Phi("1"), dict);
  EXPECT_FALSE(dfa->state_is_accepting(dfa->get_init_state_number()));
  EXPECT_TRUE(has_reachable_accepting(dfa));
}

// Trivially-false phi: no word satisfies it, so no state is accepting
// (a rejecting DFA) --- the initial state in particular is not final.
TEST(LtlfToDfa, TriviallyFalseHasNoAcceptingState) {
  auto dict = spot::make_bdd_dict();
  auto dfa = ltlf_to_dfa(Phi("0"), dict);
  EXPECT_FALSE(dfa->state_is_accepting(dfa->get_init_state_number()));
  EXPECT_FALSE(has_reachable_accepting(dfa));
}

// phi = "a" (a boolean, satisfied by a length-1 word with a): the empty word
// does not satisfy it (init non-accepting), but a reachable final state exists.
TEST(LtlfToDfa, BooleanPhiHasNonAcceptingInitialAndAReachableFinalState) {
  auto dict = spot::make_bdd_dict();
  auto dfa = ltlf_to_dfa(Phi("a"), dict);
  EXPECT_FALSE(dfa->state_is_accepting(dfa->get_init_state_number()));
  EXPECT_TRUE(has_reachable_accepting(dfa));
}

}  // namespace
