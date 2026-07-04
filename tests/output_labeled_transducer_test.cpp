#include <optional>
#include <vector>

#include <gtest/gtest.h>
#include <bddx.h>
#include <spot/twa/twagraph.hh>

#include "ltlf_ek/output_labeled_transducer.hpp"

// Unit fixtures for OutputLabeledTransducer (docs/GLOSSARY.md:
// "output-labeled transducer").  A hand-built 3-state transducer over
//   sigma0 = {a}   (observed slice, e.g. Ifree),
//   sigma1 = {b}   (produced slice, e.g. Iknown),
//   c              (an extra history variable, in neither slice),
// exercising delta, lambda, partiality (nullopt), and the abuse-of-notation
// property (main.tex §87: lambda reads only its sigma0 slice of the full
// letter).
namespace {

using ltlf_ek::OutputLabeledTransducer;

struct Vars {
  int a, b, c;
};

// Builds the fixture transducer on a fresh shared bdd_dict.
//
// delta:  s0 --a--> s1,  s0 --!a--> s0,  s1 --a--> s2,  s2 (no edges).
//         => delta(s1, !a) and delta(s2, *) are undefined (partial).
// lambda: out_[0] = (a<->b)   (commits b := a),
//         out_[1] = b         (commits b := true, ignoring a),
//         out_[2] = false     (lambda undefined at s2).
OutputLabeledTransducer MakeFixture(Vars* v) {
  auto aut = spot::make_twa_graph(spot::make_bdd_dict());
  v->a = aut->register_ap("a");
  v->b = aut->register_ap("b");
  v->c = aut->register_ap("c");

  const bdd av = bdd_ithvar(v->a);
  const bdd bv = bdd_ithvar(v->b);

  aut->new_states(3);
  aut->set_init_state(0);
  aut->new_edge(0, 1, av);          // a
  aut->new_edge(0, 0, bdd_nithvar(v->a));  // !a
  aut->new_edge(1, 2, av);          // a  (undefined on !a)
  // state 2: no outgoing edges -> delta always undefined.

  std::vector<bdd> lambda_by_state = {
      bdd_biimp(av, bv),  // out_[0]: b <-> a
      bv,                 // out_[1]: b := true
      bddfalse,           // out_[2]: undefined
  };

  return OutputLabeledTransducer(aut, std::move(lambda_by_state),
                                 /*sigma0_cube=*/av, /*sigma1_cube=*/bv);
}

// Full letter over {a,b,c} with the given polarities.
bdd Letter(const Vars& v, bool a, bool b, bool c) {
  return (a ? bdd_ithvar(v.a) : bdd_nithvar(v.a)) &
         (b ? bdd_ithvar(v.b) : bdd_nithvar(v.b)) &
         (c ? bdd_ithvar(v.c) : bdd_nithvar(v.c));
}

TEST(OutputLabeledTransducer, InitialStateIsQ0) {
  Vars v;
  auto t = MakeFixture(&v);
  EXPECT_EQ(t.initial_state(), 0u);
}

TEST(OutputLabeledTransducer, DeltaFollowsTheSatisfiedGuard) {
  Vars v;
  auto t = MakeFixture(&v);
  // Only a's value selects the edge out of s0; b and c are ignored by delta.
  EXPECT_EQ(t.delta(0, Letter(v, /*a=*/true, false, false)), std::optional<unsigned>(1));
  EXPECT_EQ(t.delta(0, Letter(v, /*a=*/true, true, true)), std::optional<unsigned>(1));
  EXPECT_EQ(t.delta(0, Letter(v, /*a=*/false, true, false)), std::optional<unsigned>(0));
  EXPECT_EQ(t.delta(1, Letter(v, /*a=*/true, false, false)), std::optional<unsigned>(2));
}

TEST(OutputLabeledTransducer, DeltaIsNulloptWhenNoGuardMatches) {
  Vars v;
  auto t = MakeFixture(&v);
  // s1 only has an edge on a; !a is undefined (partial delta).
  EXPECT_EQ(t.delta(1, Letter(v, /*a=*/false, false, false)), std::nullopt);
  // s2 has no edges at all.
  EXPECT_EQ(t.delta(2, Letter(v, /*a=*/true, true, true)), std::nullopt);
}

TEST(OutputLabeledTransducer, LambdaCommitsSigma1ValueFromSigma0Slice) {
  Vars v;
  auto t = MakeFixture(&v);
  const bdd bv = bdd_ithvar(v.b);
  // out_[0] = (b <-> a): a true -> b, a false -> !b.
  EXPECT_EQ(t.lambda(0, Letter(v, /*a=*/true, false, false)), std::optional<bdd>(bv));
  EXPECT_EQ(t.lambda(0, Letter(v, /*a=*/false, true, false)),
            std::optional<bdd>(bdd_nithvar(v.b)));
  // out_[1] = b: constant, independent of a.
  EXPECT_EQ(t.lambda(1, Letter(v, /*a=*/false, false, false)), std::optional<bdd>(bv));
  EXPECT_EQ(t.lambda(1, Letter(v, /*a=*/true, true, true)), std::optional<bdd>(bv));
}

TEST(OutputLabeledTransducer, LambdaIsNulloptWhenUndefined) {
  Vars v;
  auto t = MakeFixture(&v);
  // out_[2] = bddfalse: lambda undefined at s2 for every letter.
  EXPECT_EQ(t.lambda(2, Letter(v, /*a=*/true, true, true)), std::nullopt);
  EXPECT_EQ(t.lambda(2, Letter(v, /*a=*/false, false, false)), std::nullopt);
}

TEST(OutputLabeledTransducer, UndefinedLetterYieldsNulloptOnBothDeltaAndLambda) {
  Vars v;
  auto t = MakeFixture(&v);
  // At s2 the letter is undefined on both delta (no edge) and lambda (false).
  const bdd w = Letter(v, /*a=*/true, false, true);
  EXPECT_EQ(t.delta(2, w), std::nullopt);
  EXPECT_EQ(t.lambda(2, w), std::nullopt);
}

// Abuse-of-notation (main.tex §87): lambda reads ONLY its sigma0 slice, so
// varying variables outside sigma0 (here c, and even the sigma1 var b in the
// letter) never changes the output.
TEST(OutputLabeledTransducer, LambdaIgnoresVariablesOutsideSigma0) {
  Vars v;
  auto t = MakeFixture(&v);
  auto out_c0 = t.lambda(0, Letter(v, /*a=*/true, /*b=*/false, /*c=*/false));
  auto out_c1 = t.lambda(0, Letter(v, /*a=*/true, /*b=*/true, /*c=*/true));
  EXPECT_EQ(out_c0, out_c1);
}

// lambda's committed value is a cube over sigma1 only: existentially
// quantifying sigma1 out of it leaves bddtrue.
TEST(OutputLabeledTransducer, LambdaResultConstrainsOnlySigma1) {
  Vars v;
  auto t = MakeFixture(&v);
  const bdd sigma1 = bdd_ithvar(v.b);
  auto out = t.lambda(0, Letter(v, /*a=*/true, false, false));
  ASSERT_TRUE(out.has_value());
  EXPECT_EQ(bdd_exist(*out, sigma1), bddtrue);
}

// --- Gap 1: per-observation partiality (docs/prd/concrete-transducer.md
// "Edge cases": partial delta/lambda -> nullopt; src/output_labeled_transducer
// .cpp:62's `r == bddfalse` branch) -----------------------------------------
//
// A state can be defined (out_[q] != bddfalse) while still having no
// committed completion for one specific Sigma0 observation: out_[0] = a & b
// commits b := true only on the observation a = true; the observation
// a = false has no completion, so lambda must be nullopt there even though
// out_[0] itself is not bddfalse.  This is a distinct branch from
// LambdaIsNulloptWhenUndefined above, which covers out_[q] == bddfalse at the
// whole-state level.
namespace {

struct PartialObservationVars {
  int a, b;
};

// delta is irrelevant to this gap; the fixture has one state and no edges.
OutputLabeledTransducer MakePartialObservationFixture(PartialObservationVars* v) {
  auto aut = spot::make_twa_graph(spot::make_bdd_dict());
  v->a = aut->register_ap("a");
  v->b = aut->register_ap("b");
  const bdd av = bdd_ithvar(v->a);
  const bdd bv = bdd_ithvar(v->b);

  aut->new_states(1);
  aut->set_init_state(0);

  std::vector<bdd> lambda_by_state = {av & bv};  // out_[0]: only a=true commits b.

  return OutputLabeledTransducer(aut, std::move(lambda_by_state),
                                 /*sigma0_cube=*/av, /*sigma1_cube=*/bv);
}

bdd PartialObservationLetter(const PartialObservationVars& v, bool a, bool b) {
  return (a ? bdd_ithvar(v.a) : bdd_nithvar(v.a)) &
         (b ? bdd_ithvar(v.b) : bdd_nithvar(v.b));
}

}  // namespace

TEST(OutputLabeledTransducer,
     LambdaIsNulloptOnAnUncommittedObservationEvenWhenTheStateIsDefined) {
  PartialObservationVars v;
  auto t = MakePartialObservationFixture(&v);
  // Observation a=false has no completion in (a & b): nullopt regardless of
  // b's value in the letter, since lambda reads only the sigma0 (a) slice.
  EXPECT_EQ(t.lambda(0, PartialObservationLetter(v, /*a=*/false, /*b=*/false)),
            std::nullopt);
  EXPECT_EQ(t.lambda(0, PartialObservationLetter(v, /*a=*/false, /*b=*/true)),
            std::nullopt);
}

TEST(OutputLabeledTransducer, LambdaCommitsOnTheOneObservationWithACompletion) {
  PartialObservationVars v;
  auto t = MakePartialObservationFixture(&v);
  const bdd bv = bdd_ithvar(v.b);
  // Observation a=true commits b := true; the letter's actual b value is
  // ignored (abuse-of-notation, main.tex footnote sec87).
  EXPECT_EQ(t.lambda(0, PartialObservationLetter(v, /*a=*/true, /*b=*/false)),
            std::optional<bdd>(bv));
  EXPECT_EQ(t.lambda(0, PartialObservationLetter(v, /*a=*/true, /*b=*/true)),
            std::optional<bdd>(bv));
}

// --- Gap 2: the two documented throws ---------------------------------------

TEST(OutputLabeledTransducer, DeltaThrowsRuntimeErrorOnNondeterministicTransitionStructure) {
  auto aut = spot::make_twa_graph(spot::make_bdd_dict());
  int a = aut->register_ap("a");
  const bdd av = bdd_ithvar(a);

  aut->new_states(2);
  aut->set_init_state(0);
  // Two edges out of s0 whose guards both accept the letter a=true, violating
  // the deterministic-delta contract (main.tex: delta is a partial function).
  aut->new_edge(0, 1, av);
  aut->new_edge(0, 1, bddtrue);

  std::vector<bdd> lambda_by_state = {bddtrue, bddtrue};
  OutputLabeledTransducer t(aut, lambda_by_state, /*sigma0_cube=*/bddtrue,
                            /*sigma1_cube=*/bddtrue);

  EXPECT_THROW(t.delta(0, av), std::runtime_error);
}

TEST(OutputLabeledTransducer, ConstructorThrowsInvalidArgumentWhenLambdaByStateSizeMismatchesNumStates) {
  auto aut = spot::make_twa_graph(spot::make_bdd_dict());
  aut->register_ap("a");
  aut->new_states(2);
  aut->set_init_state(0);
  // No edges needed: the size check runs before delta/lambda are ever called.
  std::vector<bdd> lambda_by_state = {bddtrue};  // size 1 != delta_dfa->num_states() == 2

  EXPECT_THROW(
      OutputLabeledTransducer(aut, lambda_by_state, /*sigma0_cube=*/bddtrue,
                              /*sigma1_cube=*/bddtrue),
      std::invalid_argument);
}

// --- Gap 3: empty Sigma0 (docs/prd/concrete-transducer.md "Edge cases":
// "Empty Sigma0 ... lambda ignores v and returns a constant cube --
// bdd_restrict/bdd_exist with an empty cube must still work") -------------

TEST(OutputLabeledTransducer, LambdaWithEmptySigma0IgnoresTheLetterAndAlwaysReturnsTheConstantCube) {
  auto aut = spot::make_twa_graph(spot::make_bdd_dict());
  int b = aut->register_ap("b");
  const bdd bv = bdd_ithvar(b);

  aut->new_states(1);
  aut->set_init_state(0);

  std::vector<bdd> lambda_by_state = {bv};  // out_[0] = b: a constant commit.
  // sigma0_cube = bddtrue: Sigma0 is empty, so lambda observes nothing; this
  // exercises bdd_exist(v, sigma1_cube_) collapsing to bddtrue and the final
  // bdd_exist(r, sigma0_cube_) call with an empty cube.
  OutputLabeledTransducer t(aut, lambda_by_state, /*sigma0_cube=*/bddtrue,
                            /*sigma1_cube=*/bv);

  EXPECT_EQ(t.lambda(0, bdd_ithvar(b)), std::optional<bdd>(bv));
  EXPECT_EQ(t.lambda(0, bdd_nithvar(b)), std::optional<bdd>(bv));
}

}  // namespace
