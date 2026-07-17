#include <map>
#include <queue>
#include <set>
#include <stdexcept>
#include <vector>

#include <gtest/gtest.h>
#include <bddx.h>
#include <spot/twa/bdddict.hh>
#include <spot/twa/twagraph.hh>

#include "ltlf_ek/output_labeled_transducer.hpp"
#include "ltlf_ek/product.hpp"
#include "ltlf_ek/variables.hpp"

#include "support/fixtures.hpp"

// Unit fixtures for agreeing_successor(...) and build_product(...)
// (docs/prd/transducer-product.md Phase 1; docs/GLOSSARY.md "Product",
// generalized ProductState<goal, taus...>).  The Goal DFA and transducers
// below are hand-built directly as twa_graphs (not via ltlf_to_dfa), so every
// expected ProductState / acc / edge is hand-computed here, not re-derived
// from another oracle --- this is the primary net for the refactor's new
// atoms; DfaProduct.* in dfa_product_test.cpp is the metamorphic cross-check
// that build_product reproduces the old arena.  LetterAlphabet gets its own
// TEST group below (docs/prd/architecture-cleanup.md "Test oracles" #2).
namespace {

using ltlf_ek::agreeing_successor;
using ltlf_ek::build_product;
using ltlf_ek::build_product_nondet;
using ltlf_ek::LetterAlphabet;
using ltlf_ek::OutputLabeledTransducer;
using ltlf_ek::ProductGuards;
using ltlf_ek::ProductNode;
using ltlf_ek::ProductState;
using ltlf_ek::Transducer;
using ltlf_ek::VariablePartition;

const spot::acc_cond::mark_t kAcc = {0};
const spot::acc_cond::mark_t kNoAcc = {};

// Registers "i" and "o" on a fresh dict and returns their var numbers.
struct Vars {
  spot::bdd_dict_ptr dict;
  int iv, ov;
};

Vars MakeVars() {
  auto dict = spot::make_bdd_dict();
  auto probe = spot::make_twa_graph(dict);
  const int iv = probe->register_ap("i");
  const int ov = probe->register_ap("o");
  return {dict, iv, ov};
}

// I = {i} free, O = {o} free, V = empty --- input_free/output_free block
// order matches MakeVars' (iv, ov) registration order 1:1, so a LetterAlphabet
// built from it on the same dict enumerates letters LSB-first as (i, o), the
// same order the fixtures below already hand-trace.
using ltlf_ek::test_support::IoFreeVars;

// A LetterAlphabet for `IoFreeVars()` registered on `v`'s own dict (register_ap
// is idempotent, so this reuses v.iv / v.ov rather than allocating new ones).
LetterAlphabet MakeAlphabet(const Vars& v) {
  auto registrar = spot::make_twa_graph(v.dict);
  return LetterAlphabet(IoFreeVars(), registrar);
}

bdd Letter(const Vars& v, bool i, bool o) {
  return (i ? bdd_ithvar(v.iv) : bdd_nithvar(v.iv)) &
         (o ? bdd_ithvar(v.ov) : bdd_nithvar(v.ov));
}

// Goal DFA over (i, o), deterministic and complete (main.tex "Goal automaton
// is deterministic and complete"): state 0 (non-accepting) reaches state 1
// (the accepting sink) exactly when o holds; i is irrelevant to both edges.
//   0 --o--> 1 (accepting sink, self-loops on every letter)
//   0 --!o-> 0
spot::twa_graph_ptr TwoStateGoal(const Vars& v) {
  auto g = spot::make_twa_graph(v.dict);
  g->set_buchi();
  g->prop_state_acc(true);
  g->new_states(2);
  g->set_init_state(0);
  // Marks are per SOURCE state (state-based acc): state 0 is non-accepting,
  // so both its out-edges carry kNoAcc; state 1 is accepting, so its
  // self-loop carries kAcc.
  g->new_edge(0, 1, bdd_ithvar(v.ov), kNoAcc);
  g->new_edge(0, 0, bdd_nithvar(v.ov), kNoAcc);
  g->new_edge(1, 1, bddtrue, kAcc);
  return g;
}

// A single accepting-doesn't-matter state, complete over every letter --- used
// where the goal side must not be the reason agreeing_successor returns
// nullopt (the partial-delta fixture below).
spot::twa_graph_ptr SingleStateCompleteGoal(const Vars& v) {
  auto g = spot::make_twa_graph(v.dict);
  g->set_buchi();
  g->prop_state_acc(true);
  g->new_states(1);
  g->set_init_state(0);
  g->new_edge(0, 0, bddtrue, kNoAcc);
  return g;
}

// A single state with NO edge for o=false --- goal_delta misses on any letter
// with o false.
spot::twa_graph_ptr IncompleteGoalMissingOFalse(const Vars& v) {
  auto g = spot::make_twa_graph(v.dict);
  g->set_buchi();
  g->prop_state_acc(true);
  g->new_states(1);
  g->set_init_state(0);
  g->new_edge(0, 0, bdd_ithvar(v.ov), kNoAcc);  // only o=true.
  return g;
}

// Trivial transducer (docs mirror dfa_product_test.cpp's Trivial()): Sigma0 =
// Sigma1 = empty, one state, delta a total self-loop --- so emits(...) is
// true for every letter and delta is always defined.
OutputLabeledTransducer TrivialTransducer(const Vars& v) {
  auto g = spot::make_twa_graph(v.dict);
  g->new_states(1);
  g->set_init_state(0);
  g->new_edge(0, 0, bddtrue);
  return OutputLabeledTransducer(g, {bddtrue}, /*sigma0=*/bddtrue,
                                 /*sigma1=*/bddtrue);
}

// A 2-state Oknown transducer (Sigma1 = {o}): state 0 commits o:=true and
// unconditionally advances to state 1; state 1 commits o:=false and
// self-loops.  delta is total throughout, so only the lambda/cons half of
// def:consistency ever filters a letter here.
OutputLabeledTransducer OKnownTransducer(const Vars& v) {
  auto g = spot::make_twa_graph(v.dict);
  g->new_states(2);
  g->set_init_state(0);
  g->new_edge(0, 1, bddtrue);
  g->new_edge(1, 1, bddtrue);
  return OutputLabeledTransducer(g, {bdd_ithvar(v.ov), bdd_nithvar(v.ov)},
                                 /*sigma0=*/bddtrue, /*sigma1=*/bdd_ithvar(v.ov));
}

// A transducer whose lambda is trivially defined everywhere (like
// TrivialTransducer) but whose delta is undefined unless i holds --- isolates
// the delta-partiality half of def:consistency from the lambda/cons half.
OutputLabeledTransducer PartialDeltaOnInputTransducer(const Vars& v) {
  auto g = spot::make_twa_graph(v.dict);
  g->new_states(1);
  g->set_init_state(0);
  g->new_edge(0, 0, bdd_ithvar(v.iv));  // no edge when i is false.
  return OutputLabeledTransducer(g, {bddtrue}, /*sigma0=*/bddtrue,
                                 /*sigma1=*/bddtrue);
}

// --- agreeing_successor ------------------------------------------------

TEST(AgreeingSuccessor, EnabledLetterYieldsCorrectSuccessor) {
  auto v = MakeVars();
  auto goal = TwoStateGoal(v);
  auto t_in = TrivialTransducer(v);
  auto t_out = OKnownTransducer(v);
  const std::vector<const Transducer*> taus{&t_in, &t_out};
  const ProductState state{0, {0, 0}};

  // i=true, o=true: t_out's state 0 commits o:=true, so this letter agrees;
  // the goal advances on o, t_out advances unconditionally.
  const auto succ = agreeing_successor(goal, taus, state, Letter(v, true, true),
                                       /*goal_must_be_complete=*/true);
  ASSERT_TRUE(succ.has_value());
  EXPECT_EQ(succ->goal, 1u);
  EXPECT_EQ(succ->taus, (std::vector<unsigned>{0, 1}));
}

TEST(AgreeingSuccessor, ConsViolatingLetterReturnsNullopt) {
  auto v = MakeVars();
  auto goal = TwoStateGoal(v);
  auto t_in = TrivialTransducer(v);
  auto t_out = OKnownTransducer(v);
  const std::vector<const Transducer*> taus{&t_in, &t_out};
  const ProductState state{0, {0, 0}};

  // i=true, o=false: t_out's state 0 commits o:=true, so o=false disagrees ---
  // emits(t_out, ...) is false, the letter is not enabled.
  const auto succ = agreeing_successor(
      goal, taus, state, Letter(v, true, false), /*goal_must_be_complete=*/true);
  EXPECT_FALSE(succ.has_value());
}

TEST(AgreeingSuccessor, PartialDeltaLetterReturnsNullopt) {
  auto v = MakeVars();
  auto goal = SingleStateCompleteGoal(v);
  auto t_in = PartialDeltaOnInputTransducer(v);
  auto t_out = TrivialTransducer(v);
  const std::vector<const Transducer*> taus{&t_in, &t_out};
  const ProductState state{0, {0, 0}};

  // i=false: t_in's lambda is trivially defined (emits true) but its delta has
  // no outgoing edge for i=false --- the delta half of def:consistency fails,
  // independent of consistency.
  const auto succ = agreeing_successor(
      goal, taus, state, Letter(v, false, true), /*goal_must_be_complete=*/true);
  EXPECT_FALSE(succ.has_value());
}

TEST(AgreeingSuccessor, GoalMissThrowsWhenGoalMustBeComplete) {
  auto v = MakeVars();
  auto goal = IncompleteGoalMissingOFalse(v);
  auto t_in = TrivialTransducer(v);
  auto t_out = TrivialTransducer(v);
  const std::vector<const Transducer*> taus{&t_in, &t_out};
  const ProductState state{0, {0, 0}};

  // Both transducers agree on every letter, so the transducer filter always
  // passes; o=false has no goal edge --- a goal miss on an "enabled" letter,
  // which DfaProduct's completeness invariant forbids.
  EXPECT_THROW(agreeing_successor(goal, taus, state, Letter(v, true, false),
                                  /*goal_must_be_complete=*/true),
              std::runtime_error);
}

TEST(AgreeingSuccessor, GoalMissReturnsNulloptWhenGoalNeedNotBeComplete) {
  auto v = MakeVars();
  auto goal = IncompleteGoalMissingOFalse(v);
  auto t_in = TrivialTransducer(v);
  auto t_out = TrivialTransducer(v);
  const std::vector<const Transducer*> taus{&t_in, &t_out};
  const ProductState state{0, {0, 0}};

  // Same goal miss, but goal_must_be_complete=false (the verifier's case): a
  // legitimate non-agreement, not an error.
  const auto succ = agreeing_successor(goal, taus, state, Letter(v, true, false),
                                       /*goal_must_be_complete=*/false);
  EXPECT_FALSE(succ.has_value());
}

// --- build_product -------------------------------------------------------

// A tiny fixed product (TwoStateGoal x {TrivialTransducer, OKnownTransducer}):
// hand-traced reachable-state set, each node's acc, and each node's edges.
// MakeAlphabet(v) enumerates 2^{i,o} LSB-first in (i, o) order:
//   idx0 = i=F,o=F   idx1 = i=T,o=F   idx2 = i=F,o=T   idx3 = i=T,o=T.
TEST(BuildProduct, ReachableStatesAccAndEdgesOnTinyFixedProduct) {
  auto v = MakeVars();
  auto goal = TwoStateGoal(v);
  auto t_in = TrivialTransducer(v);
  auto t_out = OKnownTransducer(v);
  const std::vector<const Transducer*> taus{&t_in, &t_out};
  const ProductState init{0, {0, 0}};
  const LetterAlphabet alphabet = MakeAlphabet(v);
  ASSERT_EQ(alphabet.size(), 4u);

  const std::map<ProductState, ProductNode> graph =
      build_product(goal, taus, init, alphabet, /*goal_must_be_complete=*/true);

  // From init, only the two o=true letters (idx 2, idx 3) are enabled ---
  // t_out's state 0 commits o:=true --- and both land on the same successor
  // (t_out advances to state 1, the goal advances to its accepting sink).
  const ProductState reached{1, {0, 1}};
  ASSERT_EQ(graph.size(), 2u)
      << "only {init} and its single o-successor are reachable";
  ASSERT_TRUE(graph.count(init));
  ASSERT_TRUE(graph.count(reached));

  const ProductNode& n0 = graph.at(init);
  EXPECT_FALSE(n0.acc) << "goal state 0 is not accepting";
  ASSERT_EQ(n0.edges.size(), 2u);
  EXPECT_EQ(n0.edges[0].first, 2u);
  EXPECT_EQ(n0.edges[0].second, reached);
  EXPECT_EQ(n0.edges[1].first, 3u);
  EXPECT_EQ(n0.edges[1].second, reached);

  // From `reached`, only the two o=false letters (idx 0, idx 1) are enabled ---
  // t_out's state 1 commits o:=false --- and both self-loop (t_out and the
  // goal's accepting sink both stay put).
  const ProductNode& n1 = graph.at(reached);
  EXPECT_TRUE(n1.acc) << "goal state 1 is the accepting sink";
  ASSERT_EQ(n1.edges.size(), 2u);
  EXPECT_EQ(n1.edges[0].first, 0u);
  EXPECT_EQ(n1.edges[0].second, reached);
  EXPECT_EQ(n1.edges[1].first, 1u);
  EXPECT_EQ(n1.edges[1].second, reached);
}

TEST(BuildProduct, EmptyTausProductIsTheGoalAutomatonAlone) {
  // Edge case (PRD "Edge cases"): n=0 transducers, the filter is vacuously
  // true --- unlike the fixture above, NOTHING restricts a letter beyond the
  // goal edge itself, so all 4 letters are enabled at every node --- and
  // build_product must not crash looping over zero transducers.
  auto v = MakeVars();
  auto goal = TwoStateGoal(v);
  const std::vector<const Transducer*> taus;  // empty.
  const ProductState init{0, {}};
  const LetterAlphabet alphabet = MakeAlphabet(v);

  const std::map<ProductState, ProductNode> graph =
      build_product(goal, taus, init, alphabet, /*goal_must_be_complete=*/true);

  const ProductState reached{1, {}};
  ASSERT_EQ(graph.size(), 2u);
  const ProductNode& n0 = graph.at(init);
  ASSERT_EQ(n0.edges.size(), 4u);  // idx 0,1 (o=false) self-loop, 2,3 (o=true) advance.
  EXPECT_EQ(n0.edges[0].second, init);
  EXPECT_EQ(n0.edges[1].second, init);
  EXPECT_EQ(n0.edges[2].second, reached);
  EXPECT_EQ(n0.edges[3].second, reached);
  const ProductNode& n1 = graph.at(reached);
  ASSERT_EQ(n1.edges.size(), 4u);  // the accepting sink self-loops on every letter.
  for (const auto& [idx, succ] : n1.edges) EXPECT_EQ(succ, reached);
}

// --- LetterAlphabet (docs/prd/architecture-cleanup.md "Test oracles" #2) ---

TEST(LetterAlphabet, SizeIsTwoToThePowerOfUniverseSize) {
  auto dict = spot::make_bdd_dict();
  auto registrar = spot::make_twa_graph(dict);
  // |universe| = 4: i0, i1 free, o0 known (governed), o1 free.
  auto vars = VariablePartition::split({"i0", "i1"}, {"o0", "o1"}, {"o0"});
  const LetterAlphabet alphabet(vars, registrar);
  EXPECT_EQ(alphabet.size(), 16u);
  EXPECT_EQ(alphabet.size(), alphabet.letters().size());
}

TEST(LetterAlphabet, LettersArePairwiseDisjointAndCoverBddtrue) {
  auto dict = spot::make_bdd_dict();
  auto registrar = spot::make_twa_graph(dict);
  const LetterAlphabet alphabet(IoFreeVars(), registrar);
  bdd union_all = bddfalse;
  for (std::size_t k = 0; k < alphabet.size(); ++k) {
    for (std::size_t j = k + 1; j < alphabet.size(); ++j)
      EXPECT_EQ(alphabet.letters()[k] & alphabet.letters()[j], bddfalse)
          << "letters " << k << " and " << j;
    union_all |= alphabet.letters()[k];
  }
  EXPECT_EQ(union_all, bddtrue);
}

TEST(LetterAlphabet, LsbFirstOrderingLetterZeroIsAllNegativeCube) {
  auto dict = spot::make_bdd_dict();
  auto registrar = spot::make_twa_graph(dict);
  const LetterAlphabet alphabet(IoFreeVars(), registrar);
  ASSERT_EQ(alphabet.size(), 4u);
  // Ifree-first block order (input_free, output_free here, V empty): i is
  // registered before o, so letter 0 (all bits 0) is the all-negative cube
  // over (i, o) --- the same order the hand-traced BuildProduct fixtures
  // above assume.
  const int iv = registrar->register_ap("i");
  const int ov = registrar->register_ap("o");
  EXPECT_EQ(alphabet.letters()[0], bdd_nithvar(iv) & bdd_nithvar(ov));
}

TEST(LetterAlphabet, IfreeIndexAgainstHandComputedTwoVarExample) {
  auto dict = spot::make_bdd_dict();
  auto registrar = spot::make_twa_graph(dict);
  // Ifree = {i0, i1} (2 vars, low bits), Ofree = {o} (1 var, high bit): io_vars
  // = [i0, i1, o] --- ifree_index masks off the top (o) bit.
  auto vars = VariablePartition::split({"i0", "i1"}, {"o"}, {});
  const LetterAlphabet alphabet(vars, registrar);
  ASSERT_EQ(alphabet.n_ifree_combos(), 4u);
  ASSERT_EQ(alphabet.size(), 8u);
  for (std::size_t idx = 0; idx < alphabet.size(); ++idx)
    EXPECT_EQ(alphabet.ifree_index(idx), idx % 4)
        << "idx=" << idx;
}

TEST(LetterAlphabet, EmptyUniverseYieldsSingleBddtrueLetter) {
  auto dict = spot::make_bdd_dict();
  auto registrar = spot::make_twa_graph(dict);
  const LetterAlphabet alphabet(VariablePartition{}, registrar);
  ASSERT_EQ(alphabet.size(), 1u);
  EXPECT_EQ(alphabet.letters()[0], bddtrue);
  EXPECT_EQ(alphabet.n_ifree_combos(), 1u);
  EXPECT_EQ(alphabet.ifree_index(0), 0u);
}

TEST(LetterAlphabet, EmptyIfreeYieldsSingleIfreeCombo) {
  auto dict = spot::make_bdd_dict();
  auto registrar = spot::make_twa_graph(dict);
  // i is governed (Iknown, not Ifree): input_free is empty even though I
  // itself is not.
  auto vars = VariablePartition::split({"i"}, {"o"}, /*governed=*/{"i"});
  const LetterAlphabet alphabet(vars, registrar);
  ASSERT_EQ(alphabet.n_ifree_combos(), 1u);
  for (std::size_t idx = 0; idx < alphabet.size(); ++idx)
    EXPECT_EQ(alphabet.ifree_index(idx), 0u) << "idx=" << idx;
}

// --- build_product_nondet (docs/prd/nfa-product.md Phase 1 (c)) -----------
//
// A separate ap ('a') and dict-registration helper from Vars/MakeAlphabet
// above -- build_product_nondet's whole point is a NONDETERMINISTIC,
// COMPLETE goal automaton, a shape TwoStateGoal's deterministic goal doesn't
// exercise.
struct AVars {
  spot::bdd_dict_ptr dict;
  int av;
};

AVars MakeAVars() {
  auto dict = spot::make_bdd_dict();
  auto probe = spot::make_twa_graph(dict);
  const int av = probe->register_ap("a");
  return {dict, av};
}

// Goal N over ap 'a': nondeterministic AND complete (build_product_nondet's
// documented precondition -- the caller passes spot::complete_here(N), as
// NfaProduct does; this fixture is complete by hand-construction instead).
//   0 (init, non-acc) --a--> 1, --a--> 2  (nondeterministic: SAME guard,
//                                          two destinations)
//   0                 --!a--> 0
//   1 (acc)           --true--> 1         (accepting sink)
//   2 (non-acc)       --true--> 2         (non-accepting sink)
spot::twa_graph_ptr BranchingCompleteGoal(const AVars& v) {
  auto g = spot::make_twa_graph(v.dict);
  g->set_buchi();
  g->prop_state_acc(true);
  g->new_states(3);
  g->set_init_state(0);
  g->new_edge(0, 1, bdd_ithvar(v.av), kNoAcc);
  g->new_edge(0, 2, bdd_ithvar(v.av), kNoAcc);
  g->new_edge(0, 0, bdd_nithvar(v.av), kNoAcc);
  g->new_edge(1, 1, bddtrue, kAcc);
  g->new_edge(2, 2, bddtrue, kNoAcc);
  return g;
}

// t_in: a 2-state "toggle on a" -- its delta depends on the SAME ap the goal
// branches on, so q_in genuinely advances in step with the goal's
// nondeterministic branch (needed so the reachability-invariant check below
// is not vacuous: with a 1-state transducer, taus could never differ across
// a subset regardless of whether the invariant holds). Sigma0=Sigma1=bddtrue
// (lambda trivially bddtrue everywhere) so emits never blocks a letter --
// isolates the goal's own nondeterminism as the only source of branching.
OutputLabeledTransducer ToggleOnA(const AVars& v) {
  auto g = spot::make_twa_graph(v.dict);
  g->new_states(2);
  g->set_init_state(0);
  g->new_edge(0, 1, bdd_ithvar(v.av));
  g->new_edge(0, 0, bdd_nithvar(v.av));
  g->new_edge(1, 1, bddtrue);
  return OutputLabeledTransducer(g, {bddtrue, bddtrue}, /*sigma0=*/bddtrue,
                                 /*sigma1=*/bddtrue);
}

// t_out: trivial, single state, total self-loop -- q_out never moves, so any
// variation in a reachable subset's taus traces entirely to t_in's toggle.
OutputLabeledTransducer TrivialOutTransducer(const AVars& v) {
  auto g = spot::make_twa_graph(v.dict);
  g->new_states(1);
  g->set_init_state(0);
  g->new_edge(0, 0, bddtrue);
  return OutputLabeledTransducer(g, {bddtrue}, /*sigma0=*/bddtrue,
                                 /*sigma1=*/bddtrue);
}

TEST(BuildProductNondet, GoalBranchYieldsMultiDestinationEdges) {
  auto v = MakeAVars();
  auto goal = BranchingCompleteGoal(v);
  auto t_in = ToggleOnA(v);
  auto t_out = TrivialOutTransducer(v);
  const std::vector<const Transducer*> taus{&t_in, &t_out};
  const ProductState init{0, {0, 0}};
  auto vars = VariablePartition::split({"a"}, {}, {});
  const LetterAlphabet alphabet(vars, goal);
  ASSERT_EQ(alphabet.size(), 2u);

  const ProductGuards pg = build_product_nondet(goal, taus, init, alphabet);

  // From init, letter a=T lands on BOTH goal successors 1 and 2
  // (build_product_nondet's "for every s' in goal_delta_set" loop) --- two
  // DISTINCT destinations, each carrying the SAME letter in their guard: the
  // native multi-destination representation build_product's single-goal-
  // successor assumption cannot express.
  const ProductState dst1{1, {1, 0}};  // t_in advances 0->1 on a=T.
  const ProductState dst2{2, {1, 0}};
  ASSERT_TRUE(pg.nodes.count(init));
  const auto& [acc0, guards0] = pg.nodes.at(init);
  EXPECT_FALSE(acc0) << "goal state 0 is not accepting";
  ASSERT_EQ(guards0.size(), 3u)
      << "dst1, dst2 (both via a=T), and the a=F self-loop back to init";
  ASSERT_TRUE(guards0.count(dst1));
  ASSERT_TRUE(guards0.count(dst2));
  const bdd a_true = bdd_ithvar(v.av);
  EXPECT_NE(guards0.at(dst1) & a_true, bddfalse)
      << "dst1's guard must include the letter that produced it";
  EXPECT_NE(guards0.at(dst2) & a_true, bddfalse)
      << "dst2's guard must include the SAME letter -- the multi-"
         "destination point";
}

// Independent, test-local subset construction directly over ProductGuards
// (NOT nfa_to_dfa, which is separately unit-tested in tests/nfa_to_dfa_test
// .cpp): mirrors its algorithm one level up, in ProductState space, purely
// so the reachability invariant (main.tex:241) can be checked against the
// taus of whichever raw ProductStates end up sharing a subset. Subsets are
// std::set<ProductState> (ordered by ProductState::operator<); BFS from
// {init} over `alphabet.letters()`; R' = union over p in R of
// { dst : pg's guard(p, dst) & v != bddfalse }; an empty R' is skipped
// (nfa_to_dfa's own ∅-skip rule, mirrored here for reachability fidelity,
// though nfa_to_dfa itself is not under test here).
std::vector<std::set<ProductState>> ReachableProductSubsets(
    const ProductGuards& pg, const ProductState& init,
    const LetterAlphabet& alphabet) {
  std::vector<std::set<ProductState>> subsets;
  std::set<std::set<ProductState>> interned;
  std::queue<std::set<ProductState>> worklist;

  const std::set<ProductState> r0{init};
  interned.insert(r0);
  subsets.push_back(r0);
  worklist.push(r0);

  while (!worklist.empty()) {
    const std::set<ProductState> cur = worklist.front();
    worklist.pop();
    for (const bdd& v : alphabet.letters()) {
      std::set<ProductState> next;
      for (const ProductState& p : cur) {
        const auto& guards = pg.nodes.at(p).second;
        for (const auto& [dst, guard] : guards)
          if ((v & guard) != bddfalse) next.insert(dst);
      }
      if (next.empty()) continue;
      if (interned.insert(next).second) {
        subsets.push_back(next);
        worklist.push(next);
      }
    }
  }
  return subsets;
}

TEST(BuildProductNondet, EveryReachableSubsetHasASingleTausPair) {
  auto v = MakeAVars();
  auto goal = BranchingCompleteGoal(v);
  auto t_in = ToggleOnA(v);
  auto t_out = TrivialOutTransducer(v);
  const std::vector<const Transducer*> taus{&t_in, &t_out};
  const ProductState init{0, {0, 0}};
  auto vars = VariablePartition::split({"a"}, {}, {});
  const LetterAlphabet alphabet(vars, goal);

  const ProductGuards pg = build_product_nondet(goal, taus, init, alphabet);
  const std::vector<std::set<ProductState>> subsets =
      ReachableProductSubsets(pg, init, alphabet);

  // Hand-traced (see BranchingCompleteGoal / ToggleOnA above): reading a=T
  // from init reaches the subset {(1,{1,0}), (2,{1,0})} --- two DIFFERENT
  // goal components (1 vs 2) paired with the exact SAME (q_in,q_out) =
  // (1,0). That subset is NOT a singleton (the goal's nondeterminism really
  // did merge two distinct P-states together), yet its taus is uniform ---
  // the main.tex:241 reachability invariant made concrete.
  bool found_nonsingleton = false;
  for (const std::set<ProductState>& subset : subsets) {
    ASSERT_FALSE(subset.empty());
    const std::vector<unsigned>& first_taus = subset.begin()->taus;
    for (const ProductState& p : subset)
      EXPECT_EQ(p.taus, first_taus)
          << "reachable subset mixes (q_in,q_out) pairs -- violates the "
             "main.tex:241 reachability invariant";
    if (subset.size() > 1) found_nonsingleton = true;
  }
  EXPECT_TRUE(found_nonsingleton)
      << "fixture must actually exercise a merged (non-singleton) subset, "
         "else the invariant check above is vacuous";
}

TEST(LetterAlphabet, IdempotentRegistrationYieldsIdenticalLetters) {
  auto vars = IoFreeVars();
  auto dict = spot::make_bdd_dict();
  auto registrar = spot::make_twa_graph(dict);

  const LetterAlphabet first(vars, registrar);
  // Re-construct on the SAME already-registered dict: register_ap must be a
  // no-op for APs it already owns, so the second alphabet's letters are
  // bit-for-bit identical to the first's, not a fresh (possibly differently
  // numbered) enumeration.
  const LetterAlphabet again(vars, registrar);

  ASSERT_EQ(first.size(), again.size());
  for (std::size_t k = 0; k < first.size(); ++k)
    EXPECT_EQ(first.letters()[k], again.letters()[k]) << "letter " << k;
}

}  // namespace
