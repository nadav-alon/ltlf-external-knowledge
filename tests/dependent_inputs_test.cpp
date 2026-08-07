#include <cstddef>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

#include <gtest/gtest.h>
#include <bddx.h>
#include <spot/tl/formula.hh>
#include <spot/tl/parse.hh>
#include <spot/twa/bdddict.hh>
#include <spot/twa/twagraph.hh>

#include "ltlf_ek/dependency_types.hpp"
#include "ltlf_ek/dependent_inputs.hpp"
#include "ltlf_ek/dependent_outputs.hpp"
#include "ltlf_ek/output_labeled_transducer.hpp"
#include "ltlf_ek/variables.hpp"

// Phase 1 unit fixtures U1-in-U6-in and the O5-in duality oracle
// (docs/prd/input-dependencies-tool.md "Test oracles") for `dependent_inputs`
// (docs/GLOSSARY.md "Input-dependency extraction"), the greedy-lexicographic
// search for a maximally *Dependent input set* Xdep and its materialisation
// as a Tin.
//
// Written against the PRD's frozen Phase 1 *Interfaces & types* block
// (dependency_types.hpp, dependent_inputs.hpp) rather than against the
// implementation, so these assertions bind to the published contract: a
// divergence found here is a PRD-change event, not a test to patch.
namespace {

using ltlf_ek::DependentInputs;
using ltlf_ek::dependent_inputs;
using ltlf_ek::DependentOutputs;
using ltlf_ek::dependent_outputs;
using ltlf_ek::OutputLabeledTransducer;
using ltlf_ek::VariablePartition;

// A full letter over the shared dict: `names` true, everything else in the
// pair false, all other universe vars left as "don't care" in the returned
// cube. Built via a probe twa_graph the way dependent_outputs_test.cpp and
// undetermined_variable_test.cpp do, so AP registration happens on the SAME
// dict the analysis was given.
bdd Letter(const spot::twa_graph_ptr& probe,
           const std::set<std::pair<std::string, bool>>& assignment) {
  bdd v = bddtrue;
  for (const auto& [name, value] : assignment) {
    const bdd var = bdd_ithvar(probe->register_ap(name));
    v &= value ? var : !var;
  }
  return v;
}

// ---------------------------------------------------------------------------
// U1-in -- dependent, non-vacuous. phi = F(a^b), I={a,b}, O={x}.
// !phi = G(a<->b): one live state s (the initial state), functional from
// {b} to {a} -- lambda_in(s,b)=a, lambda_in(s,!b)=!a. This phi is reused
// verbatim by U4-in/U5-in/U6-in/O5-in below (I8's witness formula).
// ---------------------------------------------------------------------------

TEST(DependentInputs, U1DependentNonVacuous) {
  auto dict = spot::make_bdd_dict();
  auto probe = spot::make_twa_graph(dict);
  const auto part = VariablePartition::split(/*inputs=*/{"a", "b"},
                                              /*outputs=*/{"x"},
                                              /*governed=*/{});
  const DependentInputs result =
      dependent_inputs(spot::parse_formula("F(a ^ b)"), part, dict);

  EXPECT_EQ(result.dependent, (std::set<std::string>{"a"}));
  EXPECT_EQ(result.partition.input_known, (std::set<std::string>{"a"}));
  EXPECT_EQ(result.partition.input_free, (std::set<std::string>{"b"}));
  // I9: the output keys pass through verbatim.
  EXPECT_TRUE(result.partition.output_known.empty());
  EXPECT_EQ(result.partition.output_free, (std::set<std::string>{"x"}));
  ASSERT_TRUE(result.t_in.has_value());

  const OutputLabeledTransducer& t_in = *result.t_in;
  const unsigned s = t_in.initial_state();
  const bdd av = bdd_ithvar(probe->register_ap("a"));
  EXPECT_EQ(t_in.lambda(s, Letter(probe, {{"b", true}})),
            std::optional<bdd>(av));
  EXPECT_EQ(t_in.lambda(s, Letter(probe, {{"b", false}})),
            std::optional<bdd>(!av));
}

// ---------------------------------------------------------------------------
// U2-in -- not dependent. phi = G(a -> x), I={a}, O={x}. The environment can
// always still play a and hope for !x, so both a and !a stay live with an
// empty Ydep to separate them: Xdep=empty, t_in==nullopt.
// ---------------------------------------------------------------------------

TEST(DependentInputs, U2NotDependentHasNoTransducer) {
  auto dict = spot::make_bdd_dict();
  const auto part = VariablePartition::split(/*inputs=*/{"a"},
                                              /*outputs=*/{"x"},
                                              /*governed=*/{});
  const DependentInputs result =
      dependent_inputs(spot::parse_formula("G(a -> x)"), part, dict);

  EXPECT_TRUE(result.dependent.empty());
  EXPECT_TRUE(result.partition.input_known.empty());
  EXPECT_EQ(result.partition.input_free, (std::set<std::string>{"a"}));
  EXPECT_EQ(result.t_in, std::nullopt);
}

// ---------------------------------------------------------------------------
// U3-in -- the exists/forall linchpin (I3), REQUIRED.
// phi = F(!a | (b^x)), I={a,b}, O={x} => Xdep={a}.
// !phi = G(a & (b<->x)): one live state s with liveset(s) = a & (b<->x).
// Projecting EXISTENTIALLY over x: liveproj(s) = exists x. a&(b<->x) = a,
// with b free -- {a} is functional from {b} and accepted, {a,b} has an empty
// Ydep and two points differing on b and is rejected.
// A FORALL-projecting implementation instead computes
// forall x. a&(b<->x) = ff, vacuously functional for every candidate, and
// returns {a,b}. This is the ONLY fixture that separates the two readings.
// ---------------------------------------------------------------------------

TEST(DependentInputs, U3ExistsNotForallLinchpin) {
  auto dict = spot::make_bdd_dict();
  const auto part = VariablePartition::split(/*inputs=*/{"a", "b"},
                                              /*outputs=*/{"x"},
                                              /*governed=*/{});
  const DependentInputs result =
      dependent_inputs(spot::parse_formula("F(!a | (b ^ x))"), part, dict);

  EXPECT_EQ(result.dependent, (std::set<std::string>{"a"}))
      << "I3: the Moore projection onto Ifree is EXISTENTIAL over O -- a "
         "forall-projecting implementation returns {a,b} here instead of {a}";
}

// ---------------------------------------------------------------------------
// U4-in -- totality (I6). Reuses U1-in's phi. lambda_in must be DEFINED (the
// all-negative default cube over Xdep={a}, i.e. a=false) at the dead sink of
// Aneg -- the state reached from the live state by any letter with a!=b --
// not nullopt. A partial implementation returns nullopt there.
// ---------------------------------------------------------------------------

TEST(DependentInputs, U4TotalityAtDeadSink) {
  auto dict = spot::make_bdd_dict();
  auto probe = spot::make_twa_graph(dict);
  const auto part = VariablePartition::split(/*inputs=*/{"a", "b"},
                                              /*outputs=*/{"x"},
                                              /*governed=*/{});
  const DependentInputs result =
      dependent_inputs(spot::parse_formula("F(a ^ b)"), part, dict);

  ASSERT_TRUE(result.t_in.has_value());
  const OutputLabeledTransducer& t_in = *result.t_in;
  const unsigned s0 = t_in.initial_state();
  const bdd av = bdd_ithvar(probe->register_ap("a"));

  const auto s_dead =
      t_in.delta(s0, Letter(probe, {{"a", true}, {"b", false}}));
  ASSERT_TRUE(s_dead.has_value());
  ASSERT_NE(*s_dead, s0);

  for (const bool b : {true, false}) {
    const std::optional<bdd> lambda =
        t_in.lambda(*s_dead, Letter(probe, {{"b", b}}));
    ASSERT_TRUE(lambda.has_value())
        << "lambda_in must be total (I6) at the dead sink, b=" << b;
    EXPECT_EQ(*lambda, !av)
        << "I7: the default cube is all-negative over Xdep";
  }
}

// ---------------------------------------------------------------------------
// U5-in -- order determinism (I8). U1-in's phi has two distinct maximal
// input-dependent sets, {a} and {b}; the lexicographic one, {a}, must come
// back on every call in one process.
// ---------------------------------------------------------------------------

TEST(DependentInputs, U5LexicographicOrderIsDeterministicAcrossRepeatedCalls) {
  auto dict = spot::make_bdd_dict();
  const auto part = VariablePartition::split(/*inputs=*/{"a", "b"},
                                              /*outputs=*/{"x"},
                                              /*governed=*/{});
  const spot::formula phi = spot::parse_formula("F(a ^ b)");

  for (int trial = 0; trial < 5; ++trial) {
    const DependentInputs result = dependent_inputs(phi, part, dict);
    EXPECT_EQ(result.dependent, (std::set<std::string>{"a"}))
        << "trial " << trial
        << ": lexicographic order must pick {a}, not {b}, on every call";
  }
}

// ---------------------------------------------------------------------------
// U6-in -- singleton-union is unsound (I8). Same phi; Xdep must be a
// SINGLETON, never {a,b} -- both a=b=true and a=b=false keep the environment
// alive, so the union of the two individually-dependent singletons is not
// itself dependent. A singleton-union implementation (test each input alone
// and union the successes) wrongly returns {a,b} here.
// ---------------------------------------------------------------------------

TEST(DependentInputs, U6SingletonUnionIsUnsound) {
  auto dict = spot::make_bdd_dict();
  const auto part = VariablePartition::split(/*inputs=*/{"a", "b"},
                                              /*outputs=*/{"x"},
                                              /*governed=*/{});
  const DependentInputs result =
      dependent_inputs(spot::parse_formula("F(a ^ b)"), part, dict);

  EXPECT_EQ(result.dependent.size(), 1u);
  EXPECT_NE(result.dependent, (std::set<std::string>{"a", "b"}))
      << "singleton-union unsoundness (I8): {a,b} is NOT input-dependent";
}

// ---------------------------------------------------------------------------
// O5-in -- duality oracle. When O=empty, I3's projection is a no-op and the
// input notion collapses onto the output notion exactly:
//   dependent_inputs(phi, I, {}).dependent
//     == dependent_outputs(!phi, {}, I).dependent
// and the two emitted transducers agree on state count, initial state and
// the lambda relation per state (checked pointwise over every Ifree/Ofree
// letter and every state, since neither the class nor this test has access
// to the raw per-state relation BDD; the Role differs so the sigma0/sigma1
// CUBES are not compared directly, only the relation each computes).
// State indices are comparable because both sides build Aneg = ltlf_to_dfa
// applied to the identical formula Not(phi) on the identical shared dict.
// ---------------------------------------------------------------------------

void AssertDualityHolds(const std::string& phi_str,
                         const std::set<std::string>& inputs) {
  SCOPED_TRACE("phi = " + phi_str);
  auto dict = spot::make_bdd_dict();
  auto probe = spot::make_twa_graph(dict);
  const spot::formula phi = spot::parse_formula(phi_str);

  const auto part_in =
      VariablePartition::split(inputs, /*outputs=*/{}, /*governed=*/{});
  const auto part_out =
      VariablePartition::split(/*inputs=*/{}, inputs, /*governed=*/{});

  const DependentInputs din = dependent_inputs(phi, part_in, dict);
  const DependentOutputs dout =
      dependent_outputs(spot::formula::Not(phi), part_out, dict);

  ASSERT_EQ(din.dependent, dout.dependent)
      << "O5-in: dependent_inputs(phi, I, {}).dependent must equal "
         "dependent_outputs(!phi, {}, I).dependent when O=empty";
  ASSERT_EQ(din.partition.input_free, dout.partition.output_free);

  ASSERT_EQ(din.t_in.has_value(), dout.t_out.has_value());
  if (!din.t_in.has_value()) {
    return;  // Xdep empty both sides -- nothing else to compare.
  }

  const OutputLabeledTransducer& t_in = *din.t_in;
  const OutputLabeledTransducer& t_out = *dout.t_out;

  ASSERT_EQ(t_in.delta_dfa()->num_states(), t_out.delta_dfa()->num_states());
  ASSERT_EQ(t_in.initial_state(), t_out.initial_state());

  const std::vector<std::string> ifree(din.partition.input_free.begin(),
                                        din.partition.input_free.end());
  const std::size_t n = ifree.size();
  ASSERT_LE(n, 8u) << "keep the pointwise enumeration small";

  for (unsigned s = 0; s < t_in.delta_dfa()->num_states(); ++s) {
    for (std::size_t mask = 0; mask < (std::size_t{1} << n); ++mask) {
      std::set<std::pair<std::string, bool>> assignment;
      for (std::size_t i = 0; i < n; ++i) {
        assignment.insert({ifree[i], static_cast<bool>((mask >> i) & 1u)});
      }
      const bdd letter = Letter(probe, assignment);
      EXPECT_EQ(t_in.lambda(s, letter), t_out.lambda(s, letter))
          << "state " << s << ", letter mask " << mask;
    }
  }
}

TEST(DependentInputs, O5DualityWithOutputsEmpty) {
  // U1-in's phi is already input-only (O never occurs in it), satisfying the
  // PRD's "include at least one phi from U1-in/U2-in restricted to inputs"
  // requirement directly.
  AssertDualityHolds("F(a ^ b)", {"a", "b"});
  AssertDualityHolds("G(a -> b)", {"a", "b"});
  AssertDualityHolds("F(a & b)", {"a", "b"});
  AssertDualityHolds("G((a & b) -> c)", {"a", "b", "c"});
}

// ---------------------------------------------------------------------------
// Edge cases (Phase 1 library behaviour).
// ---------------------------------------------------------------------------

// I11: phi VALID (L(!phi) empty) throws UnsatisfiableFormula, the derived
// type -- not the mirror of the output tool's "phi unsatisfiable" case.
TEST(DependentInputs, RefusesValidFormula) {
  auto dict = spot::make_bdd_dict();
  const auto part = VariablePartition::split(/*inputs=*/{"a"},
                                              /*outputs=*/{"x"},
                                              /*governed=*/{});
  EXPECT_THROW(dependent_inputs(spot::parse_formula("1"), part, dict),
               std::invalid_argument);
  EXPECT_THROW(dependent_inputs(spot::parse_formula("1"), part, dict),
               ltlf_ek::UnsatisfiableFormula);
}

// I11's dual: phi UNSATISFIABLE under --direction in is NOT an error. Every
// state of Aneg is live (L(!phi) = everything), so the analysis runs
// normally and typically reports nothing dependent. Worth an explicit test
// since the reflex from the output tool is to expect exit 3 here.
TEST(DependentInputs, UnsatisfiablePhiUnderInputDirectionIsNotAnError) {
  auto dict = spot::make_bdd_dict();
  const auto part = VariablePartition::split(/*inputs=*/{"a"},
                                              /*outputs=*/{"x"},
                                              /*governed=*/{});
  const DependentInputs result =
      dependent_inputs(spot::parse_formula("0"), part, dict);
  EXPECT_TRUE(result.dependent.empty());
  EXPECT_EQ(result.t_in, std::nullopt);
}

// I=empty: the greedy loop is empty, Xdep=empty -- not an error.
TEST(DependentInputs, EmptyInputSetGivesEmptyDependentSet) {
  auto dict = spot::make_bdd_dict();
  const auto part = VariablePartition::split(/*inputs=*/{},
                                              /*outputs=*/{"x"},
                                              /*governed=*/{});
  const DependentInputs result =
      dependent_inputs(spot::parse_formula("G(x)"), part, dict);
  EXPECT_TRUE(result.dependent.empty());
  EXPECT_EQ(result.t_in, std::nullopt);
}

// I9: a non-empty input_known on input is refused -- main.tex has exactly
// one lambda_in producing all of Iknown, so there is no "compose two Tins"
// notion. Must not be reported as UnsatisfiableFormula (that type is I11's
// alone).
TEST(DependentInputs, RefusesNonEmptyInputKnownOnInput) {
  auto dict = spot::make_bdd_dict();
  const auto part = VariablePartition::split(/*inputs=*/{"a"},
                                              /*outputs=*/{"x"},
                                              /*governed=*/{"a"});
  ASSERT_FALSE(part.input_known.empty());
  EXPECT_THROW(dependent_inputs(spot::parse_formula("G(a <-> x)"), part, dict),
               std::invalid_argument);
  try {
    dependent_inputs(spot::parse_formula("G(a <-> x)"), part, dict);
    FAIL() << "expected the I9 input_known refusal to throw";
  } catch (const ltlf_ek::UnsatisfiableFormula&) {
    FAIL() << "the I9 refusal must not be reported as an unsatisfiable "
              "formula";
  } catch (const std::invalid_argument&) {
  }
}

// I10: a non-empty output_known on input is legal, ignored by the analysis,
// and passed through VERBATIM. inputs={a,b} both free (Iknown must start
// empty per I9); outputs={x} governed (Oknown={x}). phi mentions only a,b,
// so x is dependency-irrelevant -- the same {a} answer as U1-in comes back.
TEST(DependentInputs, NonEmptyOutputKnownPassesThroughVerbatim) {
  auto dict = spot::make_bdd_dict();
  const auto part = VariablePartition::split(/*inputs=*/{"a", "b"},
                                              /*outputs=*/{"x"},
                                              /*governed=*/{"x"});
  ASSERT_TRUE(part.input_known.empty());
  ASSERT_EQ(part.output_known, (std::set<std::string>{"x"}));

  const DependentInputs result =
      dependent_inputs(spot::parse_formula("F(a ^ b)"), part, dict);

  EXPECT_EQ(result.dependent, (std::set<std::string>{"a"}));
  EXPECT_EQ(result.partition.input_free, (std::set<std::string>{"b"}));
  EXPECT_EQ(result.partition.input_known, (std::set<std::string>{"a"}));
  // output_free / output_known pass through verbatim (I9/I10); only the two
  // input keys are repartitioned.
  EXPECT_TRUE(result.partition.output_free.empty());
  EXPECT_EQ(result.partition.output_known, (std::set<std::string>{"x"}));
}

// Closed-universe rule: an AP of phi outside partition.universe() throws
// std::invalid_argument, and must NOT be reported as UnsatisfiableFormula
// (an AP may be literally named "unsatisfiable" -- the case that broke the
// old what()-substring dispatch on the output side).
TEST(DependentInputs, RefusesApOutsideUniverse) {
  auto dict = spot::make_bdd_dict();
  const auto part = VariablePartition::split(/*inputs=*/{"a"},
                                              /*outputs=*/{"x"},
                                              /*governed=*/{});
  EXPECT_THROW(
      dependent_inputs(spot::parse_formula("G(a <-> unsatisfiable)"), part,
                        dict),
      std::invalid_argument);
  try {
    dependent_inputs(spot::parse_formula("G(a <-> unsatisfiable)"), part,
                      dict);
    FAIL() << "expected the closed-universe refusal to throw";
  } catch (const ltlf_ek::UnsatisfiableFormula&) {
    FAIL() << "the closed-universe refusal must not be reported as an "
              "unsatisfiable formula (an AP may be NAMED 'unsatisfiable')";
  } catch (const std::invalid_argument&) {
  }
}

}  // namespace
