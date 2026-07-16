#include <fstream>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include <gtest/gtest.h>
#include <spot/twa/bdddict.hh>
#include <spot/twa/twagraph.hh>
#include <spot/twaalgos/isdet.hh>

#include "ltlf_ek/detail/mona_dfa.hpp"

// P1 unit/structural tests for docs/prd/ltlf-to-nfa.md's MONA subprocess +
// DFA parser (glossary: "Reverse-language DFA (PastLtlfToDfa)", internal
// `detail::run_mona` / `detail::mona_output_to_dfa`).  Driven from a
// checked-in .mona fixture (tests/fixtures/mona/small_example.mona), NOT
// from a Goal formula phi -- the mirror/M2L-Str encoder is Phase 2.
namespace {

using ltlf_ek::detail::mona_output_to_dfa;
using ltlf_ek::detail::run_mona;

#ifndef LTLF_EK_TEST_FIXTURES_DIR
#define LTLF_EK_TEST_FIXTURES_DIR "tests/fixtures"
#endif

std::string ReadFixture(const std::string& relative_path) {
  const std::string path =
      std::string(LTLF_EK_TEST_FIXTURES_DIR) + "/" + relative_path;
  std::ifstream in(path);
  if (!in)
    ADD_FAILURE() << "could not open fixture " << path;
  std::ostringstream ss;
  ss << in.rdbuf();
  return ss.str();
}

// The fixture's declared free-variable order (tests/fixtures/mona/
// small_example.mona: "var2 a, b;").
const std::vector<std::string> kVarOrder = {"a", "b"};

// Skips the whole suite when `mona` is not runnable (same policy as
// tests/ltlfsynt_oracle_test.cpp's LtlfsyntOracleTest fixture): never a hard
// failure on a clean box without mona installed.
class MonaDfaTest : public ::testing::Test {
 protected:
  void SetUp() override {
#ifndef MONA_FOUND
    GTEST_SKIP() << "mona not found (CMake find_program(mona)); skipping "
                    "the MONA subprocess oracle";
#endif
  }
};

std::set<std::string> ap_names(const spot::twa_graph_ptr& dfa) {
  std::set<std::string> names;
  for (const spot::formula& ap : dfa->ap()) names.insert(ap.ap_name());
  return names;
}

// --- run_mona: the subprocess driver ---------------------------------

TEST_F(MonaDfaTest, RunMonaProducesTheDfaForFormulaHeader) {
  const std::string source = ReadFixture("mona/small_example.mona");
  const std::string out = run_mona(source);
  EXPECT_NE(out.find("DFA for formula with free variables:"),
            std::string::npos);
  EXPECT_NE(out.find("Transitions:"), std::string::npos);
}

TEST_F(MonaDfaTest, RunMonaThrowsOnUnparseableMonaSource) {
  EXPECT_THROW(run_mona("this is not valid m2l-str syntax;"),
              std::runtime_error);
}

// --- mona_output_to_dfa: the parser, plus the full round-trip ---------

TEST_F(MonaDfaTest, FixtureRoundTripMatchesExpectedStructure) {
  const std::string source = ReadFixture("mona/small_example.mona");
  const std::string mona_out = run_mona(source);
  auto dict = spot::make_bdd_dict();
  auto dfa = mona_output_to_dfa(mona_out, kVarOrder, dict);

  // tests/fixtures/mona/small_example.mona's documented expected table:
  // 3 states, initial 0, F_D = {1}.
  ASSERT_EQ(dfa->num_states(), 3u);
  EXPECT_EQ(dfa->get_init_state_number(), 0u);
  EXPECT_TRUE(dfa->state_is_accepting(1u));
  EXPECT_FALSE(dfa->state_is_accepting(0u));
  EXPECT_FALSE(dfa->state_is_accepting(2u));
}

TEST_F(MonaDfaTest, FixtureRoundTripBuildsOnTheGivenDict) {
  const std::string mona_out = run_mona(ReadFixture("mona/small_example.mona"));
  auto dict = spot::make_bdd_dict();
  auto dfa = mona_output_to_dfa(mona_out, kVarOrder, dict);
  EXPECT_EQ(dfa->get_dict(), dict);
}

TEST_F(MonaDfaTest, FixtureRoundTripAlphabetIsExactlyVarOrder) {
  const std::string mona_out = run_mona(ReadFixture("mona/small_example.mona"));
  auto dict = spot::make_bdd_dict();
  auto dfa = mona_output_to_dfa(mona_out, kVarOrder, dict);
  EXPECT_EQ(ap_names(dfa), (std::set<std::string>{"a", "b"}));
}

TEST_F(MonaDfaTest, FixtureRoundTripIsDeterministicAndComplete) {
  const std::string mona_out = run_mona(ReadFixture("mona/small_example.mona"));
  auto dict = spot::make_bdd_dict();
  auto dfa = mona_output_to_dfa(mona_out, kVarOrder, dict);
  EXPECT_TRUE(spot::is_deterministic(dfa));
  EXPECT_TRUE(spot::is_complete(dfa));
}

TEST_F(MonaDfaTest, FixtureRoundTripUsesStateBasedAcceptance) {
  const std::string mona_out = run_mona(ReadFixture("mona/small_example.mona"));
  auto dict = spot::make_bdd_dict();
  auto dfa = mona_output_to_dfa(mona_out, kVarOrder, dict);
  EXPECT_TRUE(static_cast<bool>(dfa->prop_state_acc()));
}

// --- mona_output_to_dfa parser errors (hand-crafted -w text) -----------

TEST(MonaDfaParser, ThrowsOnEmptyInput) {
  auto dict = spot::make_bdd_dict();
  EXPECT_THROW(mona_output_to_dfa("", kVarOrder, dict), std::runtime_error);
}

TEST(MonaDfaParser, ThrowsOnFreeVariableOrderMismatch) {
  // Real mona -w output for var_order {"a"}, but the caller expects {"a","b"}.
  constexpr char kOneVarOutput[] =
      "\n"
      "DFA for formula with free variables: a \n"
      "Initial state: 0\n"
      "Accepting states: 1 \n"
      "Rejecting states: \n"
      "Don't-care states: 0 \n"
      "\n"
      "Automaton has 2 states and 1 BDD-nodes\n"
      "Transitions:\n"
      "State 0: X -> state 1\n"
      "State 1: X -> state 1\n";
  auto dict = spot::make_bdd_dict();
  EXPECT_THROW(mona_output_to_dfa(kOneVarOutput, kVarOrder, dict),
              std::runtime_error);
}

TEST(MonaDfaParser, ThrowsOnGuardLengthMismatch) {
  constexpr char kBadGuardLength[] =
      "\n"
      "DFA for formula with free variables: a b \n"
      "Initial state: 0\n"
      "Accepting states: 1 \n"
      "Rejecting states: \n"
      "Don't-care states: 0 \n"
      "\n"
      "Automaton has 2 states and 1 BDD-nodes\n"
      "Transitions:\n"
      "State 0: X -> state 1\n"  // only 1 bit, var_order has 2 free vars.
      "State 1: XX -> state 1\n";
  auto dict = spot::make_bdd_dict();
  EXPECT_THROW(mona_output_to_dfa(kBadGuardLength, kVarOrder, dict),
              std::runtime_error);
}

// Directly exercises the parser (no mona subprocess) on a hand-written -w
// table matching tests/fixtures/mona/small_example.mona's documented output,
// letter by letter, so it runs even where `mona` is not installed.
TEST(MonaDfaParser, ParsesTheFixtureTableAndAcceptsExactlyItsLanguage) {
  constexpr char kFixtureTable[] =
      "\n"
      "DFA for formula with free variables: a b \n"
      "Initial state: 0\n"
      "Accepting states: 1 \n"
      "Rejecting states: 2 \n"
      "Don't-care states: 0 \n"
      "\n"
      "Automaton has 3 states and 4 BDD-nodes\n"
      "Transitions:\n"
      "State 0: XX -> state 1\n"
      "State 1: 0X -> state 1\n"
      "State 1: 10 -> state 2\n"
      "State 1: 11 -> state 1\n"
      "State 2: XX -> state 2\n";
  auto dict = spot::make_bdd_dict();
  auto dfa = mona_output_to_dfa(kFixtureTable, kVarOrder, dict);

  ASSERT_EQ(dfa->num_states(), 3u);
  EXPECT_EQ(dfa->get_init_state_number(), 0u);
  EXPECT_TRUE(spot::is_deterministic(dfa));
  EXPECT_TRUE(spot::is_complete(dfa));
  EXPECT_TRUE(dfa->state_is_accepting(1u));
  EXPECT_FALSE(dfa->state_is_accepting(0u));
  EXPECT_FALSE(dfa->state_is_accepting(2u));
  EXPECT_EQ(ap_names(dfa), (std::set<std::string>{"a", "b"}));

  // State 1's guards {!a, a&!b, a&b} partition the full 2-bit letter space
  // (source "a sub b" reaching state 2 -- the rejecting sink -- exactly on
  // the single letter a&!b).
  unsigned edges_out_of_state1 = 0;
  for (auto& e : dfa->out(1)) {
    ++edges_out_of_state1;
    (void)e;
  }
  EXPECT_EQ(edges_out_of_state1, 3u);
}

}  // namespace
