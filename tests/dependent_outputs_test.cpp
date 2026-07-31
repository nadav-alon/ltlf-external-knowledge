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

#include "ltlf_ek/dependent_outputs.hpp"
#include "ltlf_ek/output_labeled_transducer.hpp"
#include "ltlf_ek/variables.hpp"

// Phase 2 unit fixtures U1-U5 (docs/prd/output-dependencies-tool.md "Test
// oracles") for `dependent_outputs` (docs/GLOSSARY.md "extract dependent
// outputs"), the greedy-lexicographic search for a maximally *Dependent output
// set* $\Xdep$ and its materialisation as a $\Tout$.
//
// Asserts the PRD's frozen Phase 2 *Interfaces & types* block; a difference
// there is a PRD-change event, not something to adjust here.
namespace {

using ltlf_ek::DependentOutputs;
using ltlf_ek::dependent_outputs;
using ltlf_ek::OutputLabeledTransducer;
using ltlf_ek::VariablePartition;

// A full letter over the shared dict: `names` true, everything else in the
// pair false. Built via a probe twa_graph the way transducer_io_test.cpp and
// undetermined_variable_test.cpp do, so AP registration happens on the SAME
// dict `dependent_outputs` is given.
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
// U1 -- dependent, non-vacuous.  phi = G(a <-> x), I={a}, O={x}.
// Xdep={x}; one live state (the initial state, per the PRD's "one live
// state" framing -- this class of G(...) formula has exactly one live and
// reachable state, the dead-on-violation sink being the other), functional
// lambda: lambda(s,a)=x, lambda(s,!a)=!x.
// ---------------------------------------------------------------------------

TEST(DependentOutputs, U1DependentNonVacuous) {
  auto dict = spot::make_bdd_dict();
  auto probe = spot::make_twa_graph(dict);
  const auto part = VariablePartition::split(/*inputs=*/{"a"},
                                              /*outputs=*/{"x"},
                                              /*governed=*/{});
  const DependentOutputs result =
      dependent_outputs(spot::parse_formula("G(a <-> x)"), part, dict);

  EXPECT_EQ(result.dependent, (std::set<std::string>{"x"}));
  EXPECT_EQ(result.partition.output_known, (std::set<std::string>{"x"}));
  EXPECT_TRUE(result.partition.output_free.empty());
  ASSERT_TRUE(result.t_out.has_value());

  const OutputLabeledTransducer& t_out = *result.t_out;
  const unsigned s = t_out.initial_state();
  const bdd xv = bdd_ithvar(probe->register_ap("x"));
  EXPECT_EQ(t_out.lambda(s, Letter(probe, {{"a", true}})),
            std::optional<bdd>(xv));
  EXPECT_EQ(t_out.lambda(s, Letter(probe, {{"a", false}})),
            std::optional<bdd>(!xv));
}

// ---------------------------------------------------------------------------
// U2 -- not dependent.  phi = G(a -> x), same partition.  At !a both x and !x
// stay live, so x is not dependent: Xdep=empty and t_out==nullopt.
// ---------------------------------------------------------------------------

TEST(DependentOutputs, U2NotDependentHasNoTransducer) {
  auto dict = spot::make_bdd_dict();
  const auto part = VariablePartition::split(/*inputs=*/{"a"},
                                              /*outputs=*/{"x"},
                                              /*governed=*/{});
  const DependentOutputs result =
      dependent_outputs(spot::parse_formula("G(a -> x)"), part, dict);

  EXPECT_TRUE(result.dependent.empty());
  EXPECT_TRUE(result.partition.output_known.empty());
  EXPECT_EQ(result.partition.output_free, (std::set<std::string>{"x"}));
  EXPECT_EQ(result.t_out, std::nullopt);
}

// ---------------------------------------------------------------------------
// U3 -- singleton-union is unsound (I6).  phi = G(x <-> y), I={a}, O={x,y}.
// {x,y} is NOT dependent on {a} (at each a, both x=y=T and x=y=F stay live),
// but {x} alone is dependent on {a,y} (lexicographic-greedy picks it first).
// The direct guard: a singleton-union implementation (testing each output
// alone and unioning the successes, rather than accumulating) would wrongly
// return {x,y} here and fail this assertion.
// ---------------------------------------------------------------------------

TEST(DependentOutputs, U3SingletonUnionIsUnsound) {
  auto dict = spot::make_bdd_dict();
  const auto part = VariablePartition::split(/*inputs=*/{"a"},
                                              /*outputs=*/{"x", "y"},
                                              /*governed=*/{});
  const DependentOutputs result =
      dependent_outputs(spot::parse_formula("G(x <-> y)"), part, dict);

  // The set-vs-singleton linchpin: {x} only, never {x,y}.
  EXPECT_EQ(result.dependent, (std::set<std::string>{"x"}));
  EXPECT_EQ(result.partition.output_known, (std::set<std::string>{"x"}));
  EXPECT_EQ(result.partition.output_free, (std::set<std::string>{"y"}));
}

// ---------------------------------------------------------------------------
// U4 -- totality (I4).  phi = G(!a) & G(x), I={a}, O={x}.
// Xdep={x}; the single live state has liveset(s) = (!a & x), so lambda(s,a)
// is UNCOVERED by liveset(s) and must be totalised to the default cube, not
// left nullopt.  A partial-lambda implementation returns nullopt at a=true
// and fails this assertion.
// ---------------------------------------------------------------------------

TEST(DependentOutputs, U4TotalityDefaultsUncoveredLetters) {
  auto dict = spot::make_bdd_dict();
  auto probe = spot::make_twa_graph(dict);
  const auto part = VariablePartition::split(/*inputs=*/{"a"},
                                              /*outputs=*/{"x"},
                                              /*governed=*/{});
  const DependentOutputs result =
      dependent_outputs(spot::parse_formula("G(!a) & G(x)"), part, dict);

  EXPECT_EQ(result.dependent, (std::set<std::string>{"x"}));
  ASSERT_TRUE(result.t_out.has_value());

  const OutputLabeledTransducer& t_out = *result.t_out;
  const unsigned s = t_out.initial_state();
  const bdd xv = bdd_ithvar(probe->register_ap("x"));

  // Covered letter: !a -> x is directly in liveset(s).
  EXPECT_EQ(t_out.lambda(s, Letter(probe, {{"a", false}})),
            std::optional<bdd>(xv));

  // Uncovered letter: a=true has no live successor at all (G(!a) forbids it),
  // so liveset(s) does not mention it -- I5's default cube (all-negative over
  // Xdep, i.e. x=false) must fill it in.  This is the assertion a
  // partial-lambda (nullopt-on-empty-successor-set) implementation fails.
  const std::optional<bdd> defaulted = t_out.lambda(s, Letter(probe, {{"a", true}}));
  ASSERT_NE(defaulted, std::nullopt) << "lambda(s, a) must be defined (totalised), not nullopt (I4)";
  EXPECT_EQ(*defaulted, !xv) << "I5: the default cube is all-negative over Xdep";
}

// ---------------------------------------------------------------------------
// U5 -- order determinism (I6).  Reuses U3's phi = G(x <-> y), which has two
// distinct maximal dependent sets, {x} and {y}; the lexicographic one, {x},
// must come back -- including on REPEATED calls in one process, guarding
// against a set/hash-iteration-order bug that only shows up on a re-run.
// ---------------------------------------------------------------------------

TEST(DependentOutputs, U5LexicographicOrderIsDeterministicAcrossRepeatedCalls) {
  auto dict = spot::make_bdd_dict();
  const auto part = VariablePartition::split(/*inputs=*/{"a"},
                                              /*outputs=*/{"x", "y"},
                                              /*governed=*/{});
  const spot::formula phi = spot::parse_formula("G(x <-> y)");

  for (int trial = 0; trial < 5; ++trial) {
    const DependentOutputs result = dependent_outputs(phi, part, dict);
    EXPECT_EQ(result.dependent, (std::set<std::string>{"x"}))
        << "trial " << trial
        << ": lexicographic order must pick {x}, not {y}, on every call";
  }
}

// ---------------------------------------------------------------------------
// Edge cases (Phase 2 library behaviour).
// ---------------------------------------------------------------------------

// I9: a non-empty output_known on input is refused -- there is no "compose
// two Touts" notion.
TEST(DependentOutputs, RefusesNonEmptyOutputKnownOnInput) {
  auto dict = spot::make_bdd_dict();
  const auto part = VariablePartition::split(/*inputs=*/{"a"},
                                              /*outputs=*/{"x"},
                                              /*governed=*/{"x"});
  ASSERT_FALSE(part.output_known.empty());
  EXPECT_THROW(dependent_outputs(spot::parse_formula("G(a <-> x)"), part, dict),
               std::invalid_argument);
}

// Unsatisfiable phi: the initial state is not live, so every Xdep would be
// vacuously dependent -- detected and refused rather than confidently
// returning Xdep = O.
TEST(DependentOutputs, RefusesUnsatisfiableFormula) {
  auto dict = spot::make_bdd_dict();
  const auto part = VariablePartition::split(/*inputs=*/{"a"},
                                              /*outputs=*/{"x"},
                                              /*governed=*/{});
  EXPECT_THROW(dependent_outputs(spot::parse_formula("0"), part, dict),
               std::invalid_argument);
}

// ...and it throws the DERIVED type, so a caller mapping this one case to its
// own exit code can catch it precisely instead of grepping what(). The base
// assertion above still holds (UnsatisfiableFormula IS-A invalid_argument), so
// existing catch sites are unaffected.
TEST(DependentOutputs, UnsatisfiableFormulaThrowsItsOwnExceptionType) {
  auto dict = spot::make_bdd_dict();
  const auto part = VariablePartition::split(/*inputs=*/{"a"},
                                              /*outputs=*/{"x"},
                                              /*governed=*/{});
  EXPECT_THROW(dependent_outputs(spot::parse_formula("0"), part, dict),
               ltlf_ek::UnsatisfiableFormula);
}

// The other two refusals must NOT be that type -- they share exit code 2 with
// every other usage error. An AP literally named `unsatisfiable` is the case
// that broke the old what()-substring dispatch.
TEST(DependentOutputs, OtherRefusalsAreNotUnsatisfiableFormula) {
  auto dict = spot::make_bdd_dict();
  const auto part = VariablePartition::split(/*inputs=*/{"a"},
                                              /*outputs=*/{"x"},
                                              /*governed=*/{});
  EXPECT_THROW(
      dependent_outputs(spot::parse_formula("G(a <-> unsatisfiable)"), part,
                        dict),
      std::invalid_argument);
  try {
    dependent_outputs(spot::parse_formula("G(a <-> unsatisfiable)"), part,
                      dict);
    FAIL() << "expected the closed-universe refusal to throw";
  } catch (const ltlf_ek::UnsatisfiableFormula&) {
    FAIL() << "the closed-universe refusal must not be reported as an "
              "unsatisfiable formula (an AP may be NAMED 'unsatisfiable')";
  } catch (const std::invalid_argument&) {
  }

  auto governed = VariablePartition::split(/*inputs=*/{"a"},
                                            /*outputs=*/{"x"},
                                            /*governed=*/{"x"});
  try {
    dependent_outputs(spot::parse_formula("G(a <-> x)"), governed, dict);
    FAIL() << "expected the I9 output_known refusal to throw";
  } catch (const ltlf_ek::UnsatisfiableFormula&) {
    FAIL() << "the I9 refusal must not be reported as an unsatisfiable formula";
  } catch (const std::invalid_argument&) {
  }
}

// The CandidateObserver hook (I6): fires once per output in the same
// lexicographic order the greedy loop walks, reports acceptance matching the
// returned Xdep exactly, and carries the determinacy witness on rejection.
// This is what lets `ltlf-ek-deps --verbose` narrate the real search instead
// of re-deriving it from a copy of the algorithm.
TEST(DependentOutputs, CandidateObserverNarratesTheRealGreedyWalk) {
  auto dict = spot::make_bdd_dict();
  const auto part = VariablePartition::split(/*inputs=*/{"a"},
                                              /*outputs=*/{"x", "y"},
                                              /*governed=*/{});
  std::vector<std::string> order;
  std::vector<bool> accepted;
  std::vector<std::optional<std::string>> witnesses;
  const DependentOutputs result = dependent_outputs(
      spot::parse_formula("G(x <-> y)"), part, dict,
      [&](const std::string& z, bool ok,
          const std::optional<std::string>& bad) {
        order.push_back(z);
        accepted.push_back(ok);
        witnesses.push_back(bad);
      });

  // One call per output, in std::set order (I6's lexicographic order).
  EXPECT_EQ(order, (std::vector<std::string>{"x", "y"}));
  // U3/U5: {x} is dependent, {x,y} is not -- so x accepts and y rejects.
  EXPECT_EQ(accepted, (std::vector<bool>{true, false}));
  EXPECT_EQ(result.dependent, (std::set<std::string>{"x"}));

  // A witness accompanies exactly the rejections.
  ASSERT_EQ(witnesses.size(), 2u);
  EXPECT_EQ(witnesses[0], std::nullopt);
  ASSERT_TRUE(witnesses[1].has_value());
  EXPECT_TRUE(*witnesses[1] == "x" || *witnesses[1] == "y")
      << "the witness must name a variable of the rejected candidate {x, y}, "
         "got '"
      << *witnesses[1] << "'";
}

// The observer is purely observational: passing one must not change the
// result. Guards against a future refactor routing the verdict through it.
TEST(DependentOutputs, CandidateObserverDoesNotChangeTheResult) {
  auto dict = spot::make_bdd_dict();
  const auto part = VariablePartition::split(/*inputs=*/{"a"},
                                              /*outputs=*/{"x", "y"},
                                              /*governed=*/{});
  const spot::formula phi = spot::parse_formula("G(a <-> x)");
  const DependentOutputs without = dependent_outputs(phi, part, dict);
  const DependentOutputs with =
      dependent_outputs(phi, part, dict, [](const std::string&, bool,
                                            const std::optional<std::string>&) {
      });
  EXPECT_EQ(with.dependent, without.dependent);
  EXPECT_EQ(with.partition.output_known, without.partition.output_known);
  EXPECT_EQ(with.partition.output_free, without.partition.output_free);
  EXPECT_EQ(with.t_out.has_value(), without.t_out.has_value());
}

// O = empty: the greedy loop is empty, Xdep=empty -- not an error, same shape
// as U2 (no transducer, exit-0 case).
TEST(DependentOutputs, EmptyOutputSetGivesEmptyDependentSet) {
  auto dict = spot::make_bdd_dict();
  const auto part = VariablePartition::split(/*inputs=*/{"a"},
                                              /*outputs=*/{},
                                              /*governed=*/{});
  const DependentOutputs result =
      dependent_outputs(spot::parse_formula("G(a)"), part, dict);
  EXPECT_TRUE(result.dependent.empty());
  EXPECT_EQ(result.t_out, std::nullopt);
}

// I = empty: legal.  With no Ydep variables at all, dependence means
// liveset(s) pins a single Xdep-tuple per state -- phi=G(x) over O={x} alone
// has exactly one live letter (x=true), trivially functional, so x IS
// reported dependent.
TEST(DependentOutputs, EmptyInputSetStillAnalysesOutputs) {
  auto dict = spot::make_bdd_dict();
  const auto part = VariablePartition::split(/*inputs=*/{},
                                              /*outputs=*/{"x"},
                                              /*governed=*/{});
  const DependentOutputs result =
      dependent_outputs(spot::parse_formula("G(x)"), part, dict);
  EXPECT_EQ(result.dependent, (std::set<std::string>{"x"}));
}

// I10: a non-empty input_known on input is legal, ignored by the analysis,
// and passed through VERBATIM -- worth its own test so the pass-through does
// not silently regress (the PRD's explicit call-out).
TEST(DependentOutputs, NonEmptyInputKnownPassesThroughVerbatim) {
  auto dict = spot::make_bdd_dict();
  // inputs={a,b}, b governed (Iknown={b}); outputs={x}, nothing governed yet
  // (Oknown must start empty per I9).
  const auto part = VariablePartition::split(/*inputs=*/{"a", "b"},
                                              /*outputs=*/{"x"},
                                              /*governed=*/{"b"});
  ASSERT_EQ(part.input_free, (std::set<std::string>{"a"}));
  ASSERT_EQ(part.input_known, (std::set<std::string>{"b"}));
  ASSERT_TRUE(part.output_known.empty());

  // b does not occur in phi at all -- the analysis (I10) ignores T_in/Iknown
  // entirely and runs on A alone; b is dependency-irrelevant here.
  const DependentOutputs result =
      dependent_outputs(spot::parse_formula("G(a <-> x)"), part, dict);

  EXPECT_EQ(result.dependent, (std::set<std::string>{"x"}));
  // input_free / input_known pass through verbatim (I9); only the two output
  // keys are repartitioned.
  EXPECT_EQ(result.partition.input_free, (std::set<std::string>{"a"}));
  EXPECT_EQ(result.partition.input_known, (std::set<std::string>{"b"}));
  EXPECT_EQ(result.partition.output_known, (std::set<std::string>{"x"}));
  EXPECT_TRUE(result.partition.output_free.empty());
}

// ---------------------------------------------------------------------------
// Finite-language formulas: a live state whose live-letter region is EMPTY.
//
// Every fixture above is a G(...) formula, so every live state has a live
// successor and \liveset{s} is never empty.  `!X[!]1` ("the trace ends here")
// makes L(phi) finite, and the Goal DFA then has a TERMINAL ACCEPTING state:
// s0 --guard--> s1(acc) --true--> sink.  s1 is live (live = "an accepting
// state is reachable from s, INCLUDING s itself") yet all its successors are
// dead, so \liveset{s1} = bddfalse.  compute_live_regions used to assert this
// away as "impossible by definition of live"; the reasoning was wrong and the
// assertion aborted the process on these perfectly satisfiable formulas.
//
// Both consumers of an empty region are pinned below: is_dependent must read
// it as carrying NO constraint (not as "everything is dependent"), and the I5
// totalisation must still emit a total lambda at that state.
// ---------------------------------------------------------------------------

// The soundness direction.  phi = !X[!]1 over I={a}, O={x}: L(phi) is every
// one-letter word, so x is wholly unconstrained and Xdep must be EMPTY.  The
// terminal accepting state contributes bddfalse; reading that as vacuously
// functional at s1 is correct (no word continues past it), but it must not be
// mistaken for evidence that x is determined -- s0's live-letter region is
// `true`, which leaves x undetermined and rejects the candidate.
TEST(DependentOutputs, TerminalAcceptingStateDoesNotVacuouslyInflateXdep) {
  auto dict = spot::make_bdd_dict();
  const auto part = VariablePartition::split(/*inputs=*/{"a"},
                                              /*outputs=*/{"x"},
                                              /*governed=*/{});
  const DependentOutputs result =
      dependent_outputs(spot::parse_formula("!X[!]1"), part, dict);

  EXPECT_TRUE(result.dependent.empty());
  EXPECT_EQ(result.t_out, std::nullopt);
}

// The liveness direction, plus totality at the empty-region state.  phi =
// (a <-> x) & !X[!]1 over I={a}, O={x}: the single step pins x := a, so
// Xdep={x}.  s0's region is (a <-> x); s1 (terminal, accepting) has region
// bddfalse and every letter there must still be defaulted to default_X
// (= !x), never left nullopt.
TEST(DependentOutputs, FiniteLanguageLambdaIsTotalAtTerminalAcceptingState) {
  auto dict = spot::make_bdd_dict();
  auto probe = spot::make_twa_graph(dict);
  const auto part = VariablePartition::split(/*inputs=*/{"a"},
                                              /*outputs=*/{"x"},
                                              /*governed=*/{});
  const DependentOutputs result =
      dependent_outputs(spot::parse_formula("(a <-> x) & !X[!]1"), part, dict);

  EXPECT_EQ(result.dependent, (std::set<std::string>{"x"}));
  ASSERT_TRUE(result.t_out.has_value());
  const OutputLabeledTransducer& t_out = *result.t_out;
  const bdd xv = bdd_ithvar(probe->register_ap("x"));

  // s0: lambda commits x := a, exactly as in the infinite-language U1.
  const unsigned s0 = t_out.initial_state();
  EXPECT_EQ(t_out.lambda(s0, Letter(probe, {{"a", true}})),
            std::optional<bdd>(xv));
  EXPECT_EQ(t_out.lambda(s0, Letter(probe, {{"a", false}})),
            std::optional<bdd>(!xv));

  // Step to the terminal accepting state on the accepted letter (a & x), and
  // assert lambda there is TOTAL (I5) -- defaulted, not undefined, even though
  // its live-letter region is empty.
  const auto s1 = t_out.delta(s0, Letter(probe, {{"a", true}, {"x", true}}));
  ASSERT_TRUE(s1.has_value());
  ASSERT_NE(*s1, s0);
  // Pin that *s1 really is the TERMINAL ACCEPTING state and not the dead sink.
  // Without this the test is vacuous: a dead state's lambda is defaulted to the
  // same !x (I3), so every assertion below would still pass on the sink, and a
  // regression that zeroed the live regions at accepting states would go
  // unnoticed here and in U1 (which only probes initial_state()).
  ASSERT_TRUE(t_out.delta_dfa()->state_is_accepting(*s1));
  EXPECT_EQ(t_out.lambda(*s1, Letter(probe, {{"a", true}})),
            std::optional<bdd>(!xv));
  EXPECT_EQ(t_out.lambda(*s1, Letter(probe, {{"a", false}})),
            std::optional<bdd>(!xv));
}

}  // namespace
