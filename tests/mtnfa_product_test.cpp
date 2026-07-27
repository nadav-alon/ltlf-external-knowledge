#include <cstdint>
#include <memory>
#include <optional>
#include <random>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include <bddx.h>
#include <gtest/gtest.h>
#include <spot/misc/optionmap.hh>
#include <spot/tl/defaultenv.hh>
#include <spot/tl/formula.hh>
#include <spot/tl/parse.hh>
#include <spot/tl/randomltl.hh>
#include <spot/twa/acc.hh>
#include <spot/twa/bdddict.hh>
#include <spot/twa/twagraph.hh>
#include <spot/twaalgos/ltlf2dfa.hh>

#include "ltlf_ek/bench.hpp"
#include "ltlf_ek/cli.hpp"
#include "ltlf_ek/dfa_product.hpp"
#include "ltlf_ek/emits_dfa.hpp"
#include "ltlf_ek/mtdfa_product.hpp"
#include "ltlf_ek/mtnfa.hpp"
#include "ltlf_ek/mtnfa_product.hpp"
#include "ltlf_ek/nfa_product.hpp"
#include "ltlf_ek/output_labeled_transducer.hpp"
#include "ltlf_ek/role.hpp"
#include "ltlf_ek/synthesis.hpp"
#include "ltlf_ek/transducer.hpp"
#include "ltlf_ek/turn_order.hpp"
#include "ltlf_ek/variables.hpp"
#include "ltlf_ek/verify_controller.hpp"

#include "support/fixtures.hpp"

// Full suite for docs/prd/mtnfa-product.md, bound to the FROZEN "Interfaces &
// types" block (mtnfa_product_to_mtdfa(goal, taus, vars); MtnfaProduct ::
// Synthesis). CONCURRENT WORKFLOW: /developer is landing
// include/ltlf_ek/mtnfa_product.hpp + src/mtnfa_product.cpp on a separate
// worktree; this file will not compile/link until that lands, and that is the
// correct state on this branch (test-writer skill, "Before writing").
//
// Territory discipline: this file + the `unit_tests` CMakeLists.txt entry are
// this agent's ONLY changes, so the "make_synthesis_method('mtnfa-product')"
// structural free-rider (PRD "Test oracles") lives HERE rather than being
// added to the shared tests/cli_test.cpp (where the analogous mtdfa-product /
// nfa-product dispatch tests live) -- a deliberate deviation from that
// precedent, see the report to the launcher.
namespace {

using ltlf_ek::BenchReport;
using ltlf_ek::BenchScope;
using ltlf_ek::Controller;
using ltlf_ek::DfaProduct;
using ltlf_ek::emits_dfa;
using ltlf_ek::make_synthesis_method;
using ltlf_ek::Mtnfa;
using ltlf_ek::mtnfa_product_to_mtdfa;
using ltlf_ek::MtdfaProduct;
using ltlf_ek::MtnfaProduct;
using ltlf_ek::nfa_to_mtnfa;
using ltlf_ek::ltlf_to_mtnfa;
using ltlf_ek::NfaProduct;
using ltlf_ek::OutputLabeledTransducer;
using ltlf_ek::register_turn_order_aps;
using ltlf_ek::Role;
using ltlf_ek::Stage;
using ltlf_ek::stage_name;
using ltlf_ek::Synthesis;
using ltlf_ek::Transducer;
using ltlf_ek::trivial_transducer;
using ltlf_ek::VariablePartition;
using ltlf_ek::verify_controller;

using ltlf_ek::test_support::IoFreeVars;
using ltlf_ek::test_support::Phi;

// ---------------------------------------------------------------------------
// Shared MTBDD-walking helpers (duplicated from tests/mtnfa_test.cpp per this
// project's one-file-per-suite duplication norm -- see that file's own
// banner comment and docs/GLOSSARY.md "Generated corpus").
// ---------------------------------------------------------------------------

// A spot::mtdfa row's leaf CAN be the literal bddfalse/bddtrue sink; the
// descent must check identity against both explicitly (bdd_is_terminal is
// false for BOTH bddfalse and bddtrue -- verified in tests/mtnfa_test.cpp).
bdd DescendMtdfaRow(bdd node, const bdd& letter) {
  while (!bdd_is_terminal(node) && node != bddfalse && node != bddtrue) {
    const int v = bdd_var(node);
    node = ((letter & bdd_ithvar(v)) != bddfalse) ? bdd_high(node) : bdd_low(node);
  }
  return node;
}

// Walks `dfa` from states[0] over `word`; acceptance is the LAST transition's
// bit (transition-based, finite-word semantics) -- the empty word always
// rejects.
bool MtdfaAccepts(const spot::mtdfa_ptr& dfa, const std::vector<bdd>& word) {
  if (word.empty() || dfa->states.empty()) return false;
  bdd cur = dfa->states[0];
  bool accepting = false;
  for (const bdd& letter : word) {
    if (cur == bddtrue) { accepting = true; continue; }
    if (cur == bddfalse) { accepting = false; continue; }
    const bdd leaf = DescendMtdfaRow(cur, letter);
    if (leaf == bddfalse) { cur = bddfalse; accepting = false; continue; }
    if (leaf == bddtrue) { cur = bddtrue; accepting = true; continue; }
    const int t = bdd_get_terminal(leaf);
    const unsigned d = static_cast<unsigned>(t) / 2;
    accepting = (t % 2) == 1;
    cur = dfa->states[d];
  }
  return accepting;
}

std::set<std::string> ApNameSet(const std::vector<spot::formula>& aps) {
  std::set<std::string> names;
  for (const spot::formula& ap : aps) names.insert(ap.ap_name());
  return names;
}

std::string FormulaStr(const spot::formula& phi) {
  std::ostringstream os;
  os << phi;
  return os.str();
}

// ---------------------------------------------------------------------------
// SECTION A -- Hand-built unit fixtures for mtnfa_product_to_mtdfa. No MONA
// anywhere in this section (PRD "Edge cases" "MONA absent": "A test driving
// mtnfa_product_to_mtdfa from a hand-built Mtnfa needs no mona and always
// runs").
//
// Goal N (2 states, AP "a" ONLY): 0 --(a)--> 1 (accepting, self-loops on
// bddtrue forever, mirroring tests/mtnfa_test.cpp's overlapping-guards
// fixture's "give a final state a self-loop so its mark survives the lift").
// t_in: Sigma0={a}, Sigma1={b}, commits b <-> a (TOTAL: single state,
// self-loop bddtrue). t_out: Sigma0={a}, Sigma1={c}, commits c (constant
// true, TOTAL). vars' universe is {a,b,c} -- STRICTLY LARGER than goal's own
// support {a} -- the exact shape the "aps == vars.universe(), not goal.aps"
// structural free-rider needs (PRD "Test oracles" / "Novel mechanisms (e)").
//
// Hand-derived cons = emits_region(t_in) & emits_region(t_out) = (b<->a) & c.
// ---------------------------------------------------------------------------

class MtnfaProductHandFixture : public ::testing::Test {
 protected:
  void SetUp() override {
    dict_ = spot::make_bdd_dict();

    auto goal_g = spot::make_twa_graph(dict_);
    a_ = goal_g->register_ap("a");
    goal_g->set_buchi();
    goal_g->prop_state_acc(true);
    goal_g->new_states(2);
    goal_g->set_init_state(0);
    const spot::acc_cond::mark_t kFinal = {0};
    const spot::acc_cond::mark_t kNone = {};
    goal_g->new_edge(0, 1, bdd_ithvar(a_), kNone);
    goal_g->new_edge(1, 1, bddtrue, kFinal);  // state 1 is F_N; self-loop
    goal_ = nfa_to_mtnfa(goal_g);

    auto tin_g = spot::make_twa_graph(dict_);
    tin_g->register_ap("a");  // idempotent: same var number as a_
    b_ = tin_g->register_ap("b");
    tin_g->new_states(1);
    tin_g->set_init_state(0);
    tin_g->new_edge(0, 0, bddtrue);
    const bdd tin_relation =
        (bdd_ithvar(a_) & bdd_ithvar(b_)) | (bdd_nithvar(a_) & bdd_nithvar(b_));
    t_in_ = std::make_unique<OutputLabeledTransducer>(
        tin_g, std::vector<bdd>{tin_relation}, /*sigma0=*/bdd_ithvar(a_),
        /*sigma1=*/bdd_ithvar(b_));

    auto tout_g = spot::make_twa_graph(dict_);
    tout_g->register_ap("a");  // idempotent
    c_ = tout_g->register_ap("c");
    tout_g->new_states(1);
    tout_g->set_init_state(0);
    tout_g->new_edge(0, 0, bddtrue);
    t_out_ = std::make_unique<OutputLabeledTransducer>(
        tout_g, std::vector<bdd>{bdd_ithvar(c_)}, /*sigma0=*/bdd_ithvar(a_),
        /*sigma1=*/bdd_ithvar(c_));

    vars_ = VariablePartition::split({"a", "b"}, {"c"}, /*governed=*/{"b", "c"});
    taus_ = {t_in_.get(), t_out_.get()};
  }

  bdd Letter(bool a, bool b, bool c) const {
    return (a ? bdd_ithvar(a_) : bdd_nithvar(a_)) &
           (b ? bdd_ithvar(b_) : bdd_nithvar(b_)) &
           (c ? bdd_ithvar(c_) : bdd_nithvar(c_));
  }

  spot::bdd_dict_ptr dict_;
  int a_ = -1, b_ = -1, c_ = -1;
  Mtnfa goal_;
  std::unique_ptr<OutputLabeledTransducer> t_in_, t_out_;
  VariablePartition vars_;
  std::vector<const Transducer*> taus_;
};

TEST_F(MtnfaProductHandFixture, NeverReturnsNullptrAndStatesNonEmpty) {
  const spot::mtdfa_ptr d = mtnfa_product_to_mtdfa(goal_, taus_, vars_);
  ASSERT_NE(d, nullptr);
  EXPECT_FALSE(d->states.empty());
}

// "Novel mechanisms (e)": out->aps = vars.universe(), NOT goal.aps (which is
// only {"a"} here) -- the bug the three-arg signature exists to prevent.
TEST_F(MtnfaProductHandFixture, ApsEqualVarsUniverseEvenThoughGoalSupportIsStrictlySmaller) {
  ASSERT_EQ(ApNameSet(goal_.aps), (std::set<std::string>{"a"}))
      << "fixture precondition: the goal's own support must be strictly "
        "smaller than vars.universe() for this to test anything";
  const spot::mtdfa_ptr d = mtnfa_product_to_mtdfa(goal_, taus_, vars_);
  ASSERT_NE(d, nullptr);
  EXPECT_EQ(ApNameSet(d->aps), vars_.universe());
}

// Determinism (PRD "Test oracles"): no seed, FIFO discovery -- two runs on
// the same inputs must produce BDD-EQUAL rows state-for-state (BuDDy
// canonicalises, so `==` is a semantic + structural check at once).
//
// This fixture's transducers are single-edge, so the (b).3 cartesian product
// has exactly ONE combination here and this test says nothing about its
// iteration order (it previously claimed to "pin" it -- corrected per the
// domain-review D3 finding, docs/prd/mtnfa-product.md).  SECTION A2 below
// carries the out-degree > 1 coverage.
TEST_F(MtnfaProductHandFixture, TwoRunsOnTheSameInputsAreBddEqualStateForState) {
  const spot::mtdfa_ptr d1 = mtnfa_product_to_mtdfa(goal_, taus_, vars_);
  const spot::mtdfa_ptr d2 = mtnfa_product_to_mtdfa(goal_, taus_, vars_);
  ASSERT_NE(d1, nullptr);
  ASSERT_NE(d2, nullptr);
  ASSERT_EQ(d1->states.size(), d2->states.size());
  for (std::size_t s = 0; s < d1->states.size(); ++s) {
    SCOPED_TRACE("state " + std::to_string(s));
    EXPECT_EQ(d1->states[s], d2->states[s]);
  }
}

// Headline hand-computed membership oracle: every case below is derived by
// hand from cons = (b<->a) & c and the goal N's own edges (see the class
// comment), covering cons-passing+goal-alive (accept), cons-passing+
// goal-dead (reject -- the "delta_N undefined on this subset" edge case),
// two independent non-cons axes (b!=a, c=false; both reject), and 2-step
// persistence including re-entering the SAME accepting state.
TEST_F(MtnfaProductHandFixture, MembershipMatchesTheHandDerivedConsAndGoalAnalysis) {
  const spot::mtdfa_ptr d = mtnfa_product_to_mtdfa(goal_, taus_, vars_);
  ASSERT_NE(d, nullptr);

  const bdd TTT = Letter(true, true, true);    // cons-passing, goal alive (a=T)
  const bdd FFT = Letter(false, false, true);  // cons-passing, goal DEAD (a=F, no edge)
  const bdd TFT = Letter(true, false, true);   // non-cons: b != a
  const bdd TTF = Letter(true, true, false);   // non-cons: c = false (t_out disagrees)

  EXPECT_TRUE(MtdfaAccepts(d, {TTT}))
      << "cons-passing + goal reaches its accepting state on a=true";
  EXPECT_FALSE(MtdfaAccepts(d, {FFT}))
      << "cons-passing (b<->a and c both hold at a=b=false) but delta_N(0, "
        "a=false) is UNDEFINED -- the successor subset is empty, structural "
        "bddfalse (PRD Edge cases 'delta_N-undefined on this subset')";
  EXPECT_FALSE(MtdfaAccepts(d, {TFT})) << "non-cons: b != a (t_in disagrees)";
  EXPECT_FALSE(MtdfaAccepts(d, {TTF})) << "non-cons: c = false (t_out disagrees)";

  EXPECT_TRUE(MtdfaAccepts(d, {TTT, TTT}))
      << "state 1's self-loop is unconditional (bddtrue), so the accepting "
        "state persists across a second cons-passing letter";
  EXPECT_TRUE(MtdfaAccepts(d, {TTT, FFT}))
      << "goal state 1's self-loop does not depend on 'a' at all, so even "
        "the second-step 'a=false' letter (which would have killed goal "
        "state 0) keeps state 1 -- and cons still holds at a=b=false, c=true";
  EXPECT_FALSE(MtdfaAccepts(d, {TTT, TFT}))
      << "a non-cons SECOND letter still kills the run, regardless of the "
        "first letter's outcome";
}

// ---------------------------------------------------------------------------
// SECTION A2 -- the ONLY out-degree > 1 coverage in this file.
//
// Every other fixture here (and `trivial_transducer`, which is one state with
// one bddtrue self-loop) gives each transducer state out-degree <= 1, so
// ForEachCombination yields exactly ONE combination and the whole of "Novel
// mechanisms" (b).3 and (d) -- the delta_edges x delta_edges cartesian product,
// the multi-block bdd_ite accumulation, and the disjointness assert -- never
// runs.  Domain-review finding D3 (docs/prd/mtnfa-product.md).
//
// The fixture is the one the (b).3 "Developer comments / PRD disagreements"
// entry describes, so it is also the ONLY state-count-sensitive test in the
// suite: the entry records that the pre-fix over-approximated reachability was
// LANGUAGE-INVARIANT, so product_xor, the cross-method verdicts and the
// metamorphic round-trip all pass either way.  Only the EXPECT_EQ on
// states.size() below can catch a regression in the g-masking of row_set.  If
// that assertion ever fails while the language oracles stay green, the mask in
// src/mtnfa_product.cpp is what regressed -- do NOT relax the count.
//
// Goal N (APs a, b): 0 --a&b--> 1, 0 --a&!b--> 2; 1 and 2 accepting with
// bddtrue self-loops.  t_in: TWO out-edges from state 0, (b -> 1) / (!b -> 2),
// lambda trivially true everywhere (Sigma1 empty), so cons == bddtrue and the
// combination guards are exactly the two delta_edges guards.  t_out trivial.
//
// Reachable product states, by hand: R0 = ({0}, q_in=0, q_out=0); on a&b the
// goal moves to {1} and t_in to 1, giving ({1}, 1, 0); on a&!b, ({2}, 2, 0).
// Nothing else is reachable -- 3 states.  Pre-fix this was 5: Relabel walked
// the UNMASKED row_set for each combination, so the b-combination also
// interned ({2}, 1, 0) and the !b-combination ({1}, 2, 0), neither of which any
// letter reaches.
// ---------------------------------------------------------------------------

TEST(MtnfaProductMultiBlock, OutDegreeTwoTransducerExercisesTheCartesianPathAndStaysReachabilityTight) {
  const spot::bdd_dict_ptr dict = spot::make_bdd_dict();

  auto goal_g = spot::make_twa_graph(dict);
  const int a = goal_g->register_ap("a");
  const int b = goal_g->register_ap("b");
  goal_g->set_buchi();
  goal_g->prop_state_acc(true);
  const spot::acc_cond::mark_t kFinal = {0};
  const spot::acc_cond::mark_t kNone = {};
  goal_g->new_states(3);
  goal_g->set_init_state(0);
  goal_g->new_edge(0, 1, bdd_ithvar(a) & bdd_ithvar(b), kNone);
  goal_g->new_edge(0, 2, bdd_ithvar(a) & bdd_nithvar(b), kNone);
  goal_g->new_edge(1, 1, bddtrue, kFinal);
  goal_g->new_edge(2, 2, bddtrue, kFinal);
  const Mtnfa goal = nfa_to_mtnfa(goal_g);

  // The load-bearing shape: state 0 has TWO delta_edges with disjoint guards.
  auto tin_g = spot::make_twa_graph(dict);
  tin_g->register_ap("a");  // idempotent
  tin_g->register_ap("b");
  tin_g->new_states(3);
  tin_g->set_init_state(0);
  tin_g->new_edge(0, 1, bdd_ithvar(b));
  tin_g->new_edge(0, 2, bdd_nithvar(b));
  tin_g->new_edge(1, 1, bddtrue);
  tin_g->new_edge(2, 2, bddtrue);
  // Sigma1 empty (lambda == bddtrue at every state) => emits_region == bddtrue,
  // so cons does not mask anything and the combination guards ARE the two
  // delta_edges guards -- the point being tested.
  const OutputLabeledTransducer t_in(tin_g, {bddtrue, bddtrue, bddtrue},
                                     /*sigma0=*/bddtrue, /*sigma1=*/bddtrue);

  auto tout_g = spot::make_twa_graph(dict);
  tout_g->new_states(1);
  tout_g->set_init_state(0);
  tout_g->new_edge(0, 0, bddtrue);
  const OutputLabeledTransducer t_out(tout_g, {bddtrue}, /*sigma0=*/bddtrue,
                                      /*sigma1=*/bddtrue);

  const VariablePartition vars =
      VariablePartition::split({"a", "b"}, {"o"}, /*governed=*/{});
  const std::vector<const Transducer*> taus{&t_in, &t_out};

  const spot::mtdfa_ptr d = mtnfa_product_to_mtdfa(goal, taus, vars);
  ASSERT_NE(d, nullptr);

  // THE assertion this test exists for (see the header comment): exactly the
  // reachable states, no dead weight from an unmasked Relabel walk.
  EXPECT_EQ(d->states.size(), 3u)
      << "reachability tightness: 3 = R0 + the two genuinely reachable "
        "(R, q_in, q_out) triples. 5 means row_set is being relabeled "
        "UNMASKED per combination -- restore the bdd_ite(g, row_set, "
        "terminal(0)) mask in src/mtnfa_product.cpp ((b).3)";

  // Language spot-check, so a wrong-but-3-state construction cannot pass:
  // both branches reach an accepting goal state, and !a kills the run.
  const bdd ab = bdd_ithvar(a) & bdd_ithvar(b);
  const bdd anb = bdd_ithvar(a) & bdd_nithvar(b);
  const bdd nab = bdd_nithvar(a) & bdd_ithvar(b);
  EXPECT_TRUE(MtdfaAccepts(d, {ab})) << "goal 0 --a&b--> 1, accepting";
  EXPECT_TRUE(MtdfaAccepts(d, {anb})) << "goal 0 --a&!b--> 2, accepting";
  EXPECT_FALSE(MtdfaAccepts(d, {nab})) << "no goal edge on !a";
  EXPECT_TRUE(MtdfaAccepts(d, {ab, nab}))
      << "goal state 1 self-loops on bddtrue and t_in state 1 likewise, so "
        "the second letter cannot kill an already-accepting run";
}

// The companion the test above cannot be: there, t_in has Sigma1 EMPTY, so
// emits_region == bddtrue and `cons` masks nothing -- the combination guards
// are the bare delta_edges guards.  That checks the cartesian path structurally
// but never with a non-trivial cons, and the MONA corpus cannot cover the gap
// either (its transducers are all out-degree 1), so the primary product_xor
// oracle never reaches the multi-block path at all.  Generic-code-review
// follow-up to D3 (docs/prd/mtnfa-product.md).
//
// Here t_in has out-degree 2 AND a state-dependent lambda, so cons is a proper
// restriction at every state and each combination guard is cons & delta-guard:
//   Sigma0={a}, Sigma1={k}; state 0 commits k<->a and branches a -> 1 / !a -> 2;
//   state 1 commits k, state 2 commits !k, both self-looping.
// Goal N over {a}: 0 --a--> 1, state 1 accepting with a bddtrue self-loop.
// t_out trivial.
//
// By hand: R0 = ({0}, 0, 0).  Combination (a -> 1) has guard (k<->a)&a = a&k,
// under which the goal's successor set is {1} => state ({1}, 1, 0).  Combination
// (!a -> 2) has guard (k<->a)&!a = !a&!k, under which the goal is DEAD (no edge
// on !a) => the empty set => bddfalse, no state.  From ({1},1,0): cons = k, the
// goal self-loops, so it re-enters itself.  Two states total.
//
// This fixture is ALSO a second negative control for the (b).3 mask, and a
// sharper one than the test above: unmasked, the (!a -> 2) combination would
// walk row_set's `a`-branch and intern the spurious ({1}, 2, 0) -- a key with a
// destination transducer state that combination's guard makes unreachable --
// giving 3 states instead of 2.
TEST(MtnfaProductMultiBlock, MultiBlockPathIsCorrectWhenConsAlsoRestrictsEachCombination) {
  const spot::bdd_dict_ptr dict = spot::make_bdd_dict();

  auto goal_g = spot::make_twa_graph(dict);
  const int a = goal_g->register_ap("a");
  goal_g->set_buchi();
  goal_g->prop_state_acc(true);
  goal_g->new_states(2);
  goal_g->set_init_state(0);
  goal_g->new_edge(0, 1, bdd_ithvar(a), {});
  goal_g->new_edge(1, 1, bddtrue, {0});
  const Mtnfa goal = nfa_to_mtnfa(goal_g);

  auto tin_g = spot::make_twa_graph(dict);
  tin_g->register_ap("a");  // idempotent
  const int k = tin_g->register_ap("k");
  tin_g->new_states(3);
  tin_g->set_init_state(0);
  tin_g->new_edge(0, 1, bdd_ithvar(a));   // out-degree 2, disjoint guards
  tin_g->new_edge(0, 2, bdd_nithvar(a));
  tin_g->new_edge(1, 1, bddtrue);
  tin_g->new_edge(2, 2, bddtrue);
  const bdd commits_k_iff_a =
      (bdd_ithvar(a) & bdd_ithvar(k)) | (bdd_nithvar(a) & bdd_nithvar(k));
  const OutputLabeledTransducer t_in(
      tin_g, {commits_k_iff_a, bdd_ithvar(k), bdd_nithvar(k)},
      /*sigma0=*/bdd_ithvar(a), /*sigma1=*/bdd_ithvar(k));

  auto tout_g = spot::make_twa_graph(dict);
  tout_g->new_states(1);
  tout_g->set_init_state(0);
  tout_g->new_edge(0, 0, bddtrue);
  const OutputLabeledTransducer t_out(tout_g, {bddtrue}, /*sigma0=*/bddtrue,
                                      /*sigma1=*/bddtrue);

  const VariablePartition vars =
      VariablePartition::split({"a", "k"}, {"o"}, /*governed=*/{"k"});
  const std::vector<const Transducer*> taus{&t_in, &t_out};

  const spot::mtdfa_ptr d = mtnfa_product_to_mtdfa(goal, taus, vars);
  ASSERT_NE(d, nullptr);

  EXPECT_EQ(d->states.size(), 2u)
      << "reachability tightness under a RESTRICTING cons: 3 means the "
        "(!a -> 2) combination interned ({1}, 2, 0) off row_set's a-branch, "
        "i.e. Relabel is walking row_set UNMASKED ((b).3)";

  // Hand-derived membership; `o` is free so it never affects the verdict.
  const auto letter = [&](bool av, bool kv, bool ov) {
    const int o = dict->varnum(spot::formula::ap("o"));
    return (av ? bdd_ithvar(a) : bdd_nithvar(a)) &
           (kv ? bdd_ithvar(k) : bdd_nithvar(k)) &
           (ov ? bdd_ithvar(o) : bdd_nithvar(o));
  };
  EXPECT_TRUE(MtdfaAccepts(d, {letter(true, true, true)}))
      << "cons holds (k<->a at a=k=true) and the goal reaches its accepting "
        "state on a -- the (a -> 1) combination";
  EXPECT_FALSE(MtdfaAccepts(d, {letter(true, false, true)}))
      << "non-cons: t_in state 0 commits k<->a, so a&!k is filtered out";
  EXPECT_FALSE(MtdfaAccepts(d, {letter(false, false, true)}))
      << "cons HOLDS here (!a&!k) -- this is the (!a -> 2) combination -- but "
        "the goal has no edge on !a, so the successor subset is empty";
  EXPECT_TRUE(MtdfaAccepts(d, {letter(true, true, true), letter(true, true, false)}))
      << "from ({1},1,0) cons is k alone and the goal self-loops on bddtrue";
  EXPECT_FALSE(MtdfaAccepts(d, {letter(true, true, true), letter(true, false, true)}))
      << "t_in state 1 commits k, so a second letter with !k is non-cons and "
        "kills the run even though the goal would have survived";
}

// The (d) disjointness check rejects a NONDETERMINISTIC transducer.  It is a
// THROW, not an assert, so this test runs in release builds too -- which is
// the point: Transducer is a public virtual interface, and under NDEBUG an
// assert would have let a violating subclass through with a silently wrong
// language (generic code-review, 2026-07-27; the check mirrors
// build_product_symbolic's, src/product.cpp).
TEST(MtnfaProductMultiBlock, OverlappingDeltaEdgeGuardsThrowInsteadOfCorruptingTheLanguage) {
  const auto build_and_run = [] {
    const spot::bdd_dict_ptr dict = spot::make_bdd_dict();

    auto goal_g = spot::make_twa_graph(dict);
    const int a = goal_g->register_ap("a");
    goal_g->register_ap("b");
    goal_g->set_buchi();
    goal_g->prop_state_acc(true);
    goal_g->new_states(2);
    goal_g->set_init_state(0);
    goal_g->new_edge(0, 1, bdd_ithvar(a), {});
    goal_g->new_edge(1, 1, bddtrue, {0});
    const Mtnfa goal = nfa_to_mtnfa(goal_g);

    auto tin_g = spot::make_twa_graph(dict);
    tin_g->register_ap("a");
    const int b = tin_g->register_ap("b");
    tin_g->new_states(3);
    tin_g->set_init_state(0);
    tin_g->new_edge(0, 1, bdd_ithvar(b));
    tin_g->new_edge(0, 2, bddtrue);  // OVERLAPS the (b -> 1) edge
    tin_g->new_edge(1, 1, bddtrue);
    tin_g->new_edge(2, 2, bddtrue);
    const OutputLabeledTransducer t_in(tin_g, {bddtrue, bddtrue, bddtrue},
                                       /*sigma0=*/bddtrue, /*sigma1=*/bddtrue);

    auto tout_g = spot::make_twa_graph(dict);
    tout_g->new_states(1);
    tout_g->set_init_state(0);
    tout_g->new_edge(0, 0, bddtrue);
    const OutputLabeledTransducer t_out(tout_g, {bddtrue}, bddtrue, bddtrue);

    const VariablePartition vars =
        VariablePartition::split({"a", "b"}, {"o"}, /*governed=*/{});
    const std::vector<const Transducer*> taus{&t_in, &t_out};
    (void)mtnfa_product_to_mtdfa(goal, taus, vars);
  };
  EXPECT_THROW(build_and_run(), std::runtime_error);
}

// "Cons empty at the initial state" edge case (PRD "Edge cases"):
// emits_region(t_in) == bddfalse identically ==> states[0] == bddfalse
// exactly, not merely non-accepting -- the "cons == bddfalse: push it and
// continue" shortcut in "Novel mechanisms (b).2".
TEST(MtnfaProductEdgeCases, ConsEmptyAtInitialYieldsBddfalseInitialStateAndEmptyLanguage) {
  const spot::bdd_dict_ptr dict = spot::make_bdd_dict();

  // Initial state must be NON-accepting (mtnfa_product_to_mtdfa's F2
  // precondition, "Interfaces & types" preconditions list, shared with
  // mtnfa_to_mtdfa) -- irrelevant to what this test checks either way, since
  // cons blocks every letter before the goal is even consulted.
  auto goal_g = spot::make_twa_graph(dict);
  const int x = goal_g->register_ap("x");
  goal_g->set_buchi();
  goal_g->prop_state_acc(true);
  goal_g->new_states(1);
  goal_g->set_init_state(0);
  goal_g->new_edge(0, 0, bddtrue, {});  // self-loop, no Final mark
  const Mtnfa goal = nfa_to_mtnfa(goal_g);

  auto tin_g = spot::make_twa_graph(dict);
  const int y = tin_g->register_ap("y");
  tin_g->new_states(1);
  tin_g->set_init_state(0);
  tin_g->new_edge(0, 0, bddtrue);
  // lambda undefined EVERYWHERE (bddfalse relation) ==> emits_region ==
  // bddfalse identically, regardless of x.
  const OutputLabeledTransducer t_in(tin_g, {bddfalse}, /*sigma0=*/bdd_ithvar(x),
                                     /*sigma1=*/bdd_ithvar(y));
  const VariablePartition vars =
      VariablePartition::split({"x", "y"}, {}, /*governed=*/{"y"});
  const OutputLabeledTransducer t_out = trivial_transducer(vars, Role::t_out, dict);
  const std::vector<const Transducer*> taus{&t_in, &t_out};

  const spot::mtdfa_ptr d = mtnfa_product_to_mtdfa(goal, taus, vars);
  ASSERT_NE(d, nullptr);
  ASSERT_FALSE(d->states.empty());
  EXPECT_EQ(d->states[0], bddfalse);
  EXPECT_TRUE(d->is_empty());
}

// Distinct code path from the above: cons is NONZERO throughout, but the
// goal itself never reaches an accepting state -- structurally empty
// language via goal death, not the cons==bddfalse shortcut.
TEST(MtnfaProductEdgeCases, GoalWithNoAcceptingStatesEverYieldsEmptyLanguageNeverNullptr) {
  const spot::bdd_dict_ptr dict = spot::make_bdd_dict();

  auto goal_g = spot::make_twa_graph(dict);
  goal_g->set_buchi();
  goal_g->prop_state_acc(true);
  goal_g->new_states(1);
  goal_g->set_init_state(0);
  goal_g->new_edge(0, 0, bddtrue, {});  // non-accepting, self-loops forever
  const Mtnfa goal = nfa_to_mtnfa(goal_g);

  const VariablePartition vars = IoFreeVars();
  const OutputLabeledTransducer t_in = trivial_transducer(vars, Role::t_in, dict);
  const OutputLabeledTransducer t_out = trivial_transducer(vars, Role::t_out, dict);
  const std::vector<const Transducer*> taus{&t_in, &t_out};

  const spot::mtdfa_ptr d = mtnfa_product_to_mtdfa(goal, taus, vars);
  ASSERT_NE(d, nullptr);
  ASSERT_FALSE(d->states.empty());
  EXPECT_TRUE(d->is_empty());
}

// Empty universe (I u O = {}) edge case: Sigma = {bddtrue}, every row is a
// bare terminal with no internal variable node -- the Relabel terminal base
// case and the bdd_ite accumulation both handle it unchanged (PRD "Edge
// cases").
TEST(MtnfaProductEdgeCases, EmptyUniverseYieldsBareTerminalRowsAndCorrectMembership) {
  const spot::bdd_dict_ptr dict = spot::make_bdd_dict();
  const VariablePartition vars;  // all four sets empty

  auto goal_g = spot::make_twa_graph(dict);
  goal_g->set_buchi();
  goal_g->prop_state_acc(true);
  goal_g->new_states(2);
  goal_g->set_init_state(0);
  const spot::acc_cond::mark_t kFinal = {0};
  const spot::acc_cond::mark_t kNone = {};
  goal_g->new_edge(0, 1, bddtrue, kNone);
  goal_g->new_edge(1, 1, bddtrue, kFinal);
  const Mtnfa goal = nfa_to_mtnfa(goal_g);

  const OutputLabeledTransducer t_in = trivial_transducer(vars, Role::t_in, dict);
  const OutputLabeledTransducer t_out = trivial_transducer(vars, Role::t_out, dict);
  const std::vector<const Transducer*> taus{&t_in, &t_out};

  const spot::mtdfa_ptr d = mtnfa_product_to_mtdfa(goal, taus, vars);
  ASSERT_NE(d, nullptr);
  ASSERT_FALSE(d->states.empty());
  EXPECT_TRUE(bdd_is_terminal(d->states[0]) || d->states[0] == bddfalse ||
             d->states[0] == bddtrue)
      << "empty universe: the sole letter is bddtrue, so every row must be a "
        "bare terminal with no internal decision node";
  EXPECT_TRUE(MtdfaAccepts(d, {bddtrue}))
      << "the only possible letter reaches the goal's accepting state 1, and "
        "both transducers are trivial (cons always holds)";
}

// ---------------------------------------------------------------------------
// SECTION B -- make_synthesis_method dispatch. Needs no MONA (just
// constructs the object, never calls synthesize).
// ---------------------------------------------------------------------------

TEST(MakeSynthesisMethod, MtnfaProductFlagBuildsAnMtnfaProduct) {
  std::unique_ptr<Synthesis> method = make_synthesis_method("mtnfa-product");
  ASSERT_NE(method, nullptr);
  EXPECT_NE(dynamic_cast<MtnfaProduct*>(method.get()), nullptr);
}

// ---------------------------------------------------------------------------
// SECTION C -- Generated corpus (MONA-gated throughout: ltlf_to_mtnfa
// inherits ltlf_to_nfa's mona runtime dependency, PRD "Edge cases" "MONA
// absent"). Duplicated in-file per this project's one-file-per-suite
// convention (docs/GLOSSARY.md "Generated corpus").
//
// One fixed TOTAL-by-construction partition/transducer shape (a known input
// k committed as k <-> i, t_out trivial) crossed with random small formulas
// over {"i","k","o"} -- "total transducers only" (PRD "Test oracles"),
// satisfied by construction: single-state self-loop delta (always defined)
// and a lambda relation total over its Sigma0 (always defined).
// ---------------------------------------------------------------------------

constexpr unsigned kMtnfaProductCorpusSeed = 20260727;
constexpr std::size_t kMtnfaProductCorpusCaseCount = 18;
constexpr int kMtnfaProductTreeSizeMin = 1;
constexpr int kMtnfaProductTreeSizeMax = 8;

constexpr unsigned kMtnfaProductIsolatedSeed = 20260728;
constexpr std::size_t kMtnfaProductIsolatedCaseCount = 10;
constexpr int kMtnfaProductIsolatedTreeSizeMax = 6;

VariablePartition MtnfaProductCorpusVars() {
  return VariablePartition::split({"i", "k"}, {"o"}, /*governed=*/{"k"});
}

// t_in commits k <-> i: single self-looping state (delta TOTAL), and the
// relation covers both truth values of i (lambda TOTAL) -- the "random_tin
// is deterministic and total by construction" shape the PRD's cross-method
// oracle requires (docs/BACKLOG.md's materialize_product bug needs a
// PARTIAL transducer to reproduce; this corpus deliberately never builds
// one, see the dedicated "expected divergence" test below instead).
OutputLabeledTransducer MtnfaProductCorpusTin(const spot::bdd_dict_ptr& dict) {
  auto g = spot::make_twa_graph(dict);
  const int i = g->register_ap("i");
  const int k = g->register_ap("k");
  g->new_states(1);
  g->set_init_state(0);
  g->new_edge(0, 0, bddtrue);
  const bdd relation =
      (bdd_ithvar(i) & bdd_ithvar(k)) | (bdd_nithvar(i) & bdd_nithvar(k));
  return OutputLabeledTransducer(g, {relation}, /*sigma0=*/bdd_ithvar(i),
                                 /*sigma1=*/bdd_ithvar(k));
}

// Non-empty random subset of `pool` (retried on the empty draw -- validate_
// product_inputs requires phi's own APs subset of vars.universe(), but an
// empty ap set is also legal; retrying just keeps formulas non-trivial).
std::set<std::string> RandomApSubset(const std::vector<std::string>& pool,
                                     std::mt19937& rng) {
  std::bernoulli_distribution incl(0.6);
  std::set<std::string> chosen;
  while (chosen.empty())
    for (const std::string& name : pool)
      if (incl(rng)) chosen.insert(name);
  return chosen;
}

// Same technique as tests/mtnfa_test.cpp's GenerateRandomFormula (a thin
// wrapper over spot::randltlgenerator); duplicated per this project's
// precedent rather than shared.
spot::formula GenerateRandomFormula(const std::set<std::string>& ap_names,
                                    std::mt19937& rng, int tree_size_max) {
  spot::atomic_prop_set aprops;
  for (const std::string& name : ap_names)
    aprops.insert(spot::default_environment::instance().require(name));

  spot::option_map opts;
  opts.set("output", spot::randltlgenerator::LTL);
  opts.set("tree_size_min", kMtnfaProductTreeSizeMin);
  opts.set("tree_size_max", tree_size_max);
  opts.set("seed", static_cast<int>(rng()));

  std::string priorities_str = "xor=0,M=0";
  std::vector<char> priorities(priorities_str.begin(), priorities_str.end());
  priorities.push_back('\0');

  spot::randltlgenerator rg(aprops, opts, priorities.data());
  const spot::formula phi = rg.next();
  if (!phi)
    throw std::runtime_error(
        "GenerateRandomFormula: randltlgenerator produced no formula");
  return phi;
}

// The MtdfaProduct-shaped reference construction for the PRIMARY oracle
// (PRD "Test oracles" #1), built inline (no ltlf_ek::ltlf_to_mtdfa wrapper
// exists, mirrors src/mtdfa_product.cpp's own body exactly).
spot::mtdfa_ptr IndependentMtdfaProductReference(const spot::formula& phi,
                                                 const spot::bdd_dict_ptr& dict,
                                                 const Transducer& t_in,
                                                 const Transducer& t_out) {
  const spot::mtdfa_ptr phi_mtdfa = spot::ltlf_to_mtdfa(phi, dict);
  const spot::mtdfa_ptr in_agreement = spot::twadfa_to_mtdfa(emits_dfa(t_in, dict));
  const spot::mtdfa_ptr out_agreement = spot::twadfa_to_mtdfa(emits_dfa(t_out, dict));
  return spot::product(spot::product(phi_mtdfa, in_agreement), out_agreement);
}

// Combined loop: PRIMARY cross-representation oracle (#1), cross-method
// realizability verdicts over total transducers (#4), and the metamorphic
// round-trip (#7) -- combined to avoid re-running MONA four times per case
// in three separate loops.
//
// Honest limit (PRD "Test oracles" #1): both the mtnfa_product_to_mtdfa
// route and the MtdfaProduct-shaped reference share the SAME skip=reject
// convention (Behaviour §3), so a shared semantic error in that convention
// would agree here -- this checks the CONSTRUCTION, not that convention.
// /theory-review and the expected-divergence test below are what would catch
// a convention-level error.
TEST(MtnfaProductGeneratedCorpus,
    PrimaryOracleCrossMethodVerdictsAndMetamorphicRoundTripAgree) {
#ifndef MONA_FOUND
  GTEST_SKIP() << "mona not found (CMake find_program(mona)); skipping the "
                  "MtnfaProduct generated-corpus oracle";
#else
  const std::vector<std::string> pool{"i", "k", "o"};
  std::mt19937 rng(kMtnfaProductCorpusSeed);
  for (std::size_t case_idx = 0; case_idx < kMtnfaProductCorpusCaseCount;
      ++case_idx) {
    const std::set<std::string> ap_names = RandomApSubset(pool, rng);
    const spot::formula phi =
        GenerateRandomFormula(ap_names, rng, kMtnfaProductTreeSizeMax);
    SCOPED_TRACE("case " + std::to_string(case_idx) + ": phi=" + FormulaStr(phi));

    const spot::bdd_dict_ptr dict = spot::make_bdd_dict();
    const VariablePartition vars = MtnfaProductCorpusVars();
    register_turn_order_aps(vars, dict);
    const OutputLabeledTransducer t_in = MtnfaProductCorpusTin(dict);
    const OutputLabeledTransducer t_out = trivial_transducer(vars, Role::t_out, dict);
    const std::vector<const Transducer*> taus{&t_in, &t_out};

    // --- #1: primary, exact cross-representation oracle ---
    const Mtnfa goal = ltlf_to_mtnfa(phi, dict);
    const spot::mtdfa_ptr got = mtnfa_product_to_mtdfa(goal, taus, vars);
    ASSERT_NE(got, nullptr);
    const spot::mtdfa_ptr want =
        IndependentMtdfaProductReference(phi, dict, t_in, t_out);
    EXPECT_TRUE(spot::product_xor(got, want)->is_empty())
        << "cross-representation product_xor is non-empty for phi="
        << FormulaStr(phi);

    // --- #4: cross-method realizability verdicts, total transducers only ---
    NfaProduct nfa_method;
    DfaProduct dfa_method;
    MtdfaProduct mtdfa_method;
    MtnfaProduct mtnfa_method;
    const bool nfa_realizable =
        nfa_method.synthesize(phi, vars, t_in, t_out).has_value();
    const bool dfa_realizable =
        dfa_method.synthesize(phi, vars, t_in, t_out).has_value();
    const bool mtdfa_realizable =
        mtdfa_method.synthesize(phi, vars, t_in, t_out).has_value();
    const std::optional<Controller> mtnfa_result =
        mtnfa_method.synthesize(phi, vars, t_in, t_out);

    EXPECT_EQ(mtnfa_result.has_value(), nfa_realizable)
        << "MtnfaProduct disagrees with NfaProduct (representation axis, "
          "same method) for phi=" << FormulaStr(phi);
    EXPECT_EQ(mtnfa_result.has_value(), dfa_realizable)
        << "MtnfaProduct disagrees with DfaProduct (method axis) for phi="
        << FormulaStr(phi);
    EXPECT_EQ(mtnfa_result.has_value(), mtdfa_realizable)
        << "MtnfaProduct disagrees with MtdfaProduct (method axis) for phi="
        << FormulaStr(phi);

    // --- #7: metamorphic round-trip ---
    if (mtnfa_result.has_value())
      EXPECT_TRUE(verify_controller(phi, vars, t_in, t_out, *mtnfa_result).ok)
          << "verify_controller rejected MtnfaProduct's own controller for "
            "phi=" << FormulaStr(phi);
  }
#endif
}

// Required negative control (mtdfa-product Phase-0/Q1 lesson): the SAME
// oracle comparison used above, fed a KNOWN-DIFFERENT formula pair under one
// taus, must give a NON-empty product_xor, proving the primary oracle is not
// vacuously true.
TEST(MtnfaProductGeneratedCorpus,
    NegativeControlPrimaryOracleDetectsMismatchedGvsFFormulas) {
#ifndef MONA_FOUND
  GTEST_SKIP() << "mona not found (CMake find_program(mona)); skipping the "
                  "primary-oracle negative control";
#else
  const spot::bdd_dict_ptr dict = spot::make_bdd_dict();
  const VariablePartition vars = MtnfaProductCorpusVars();
  register_turn_order_aps(vars, dict);
  const OutputLabeledTransducer t_in = MtnfaProductCorpusTin(dict);
  const OutputLabeledTransducer t_out = trivial_transducer(vars, Role::t_out, dict);
  const std::vector<const Transducer*> taus{&t_in, &t_out};

  const spot::formula g_o = Phi("G(o)");
  const spot::formula f_o = Phi("F(o)");

  const Mtnfa goal_g = ltlf_to_mtnfa(g_o, dict);
  const spot::mtdfa_ptr got = mtnfa_product_to_mtdfa(goal_g, taus, vars);
  const spot::mtdfa_ptr want =
      IndependentMtdfaProductReference(f_o, dict, t_in, t_out);

  EXPECT_FALSE(spot::product_xor(got, want)->is_empty())
      << "negative control: G(o) and F(o) denote different languages under "
        "the same taus -- a vacuous (empty) product_xor here means the "
        "cross-representation oracle cannot discriminate";
#endif
}

// PRD "Test oracles" #2: isolated, transducer-free oracle. With trivial
// transducers the product degenerates to the goal alone, separating a
// product bug from a determinization bug -- checked against Spot's
// spot::ltlf_to_mtdfa directly (fully independent of emits_dfa/product).
TEST(MtnfaProductGeneratedCorpus, IsolatedTrivialTransducerOracleDegeneratesToTheGoalAlone) {
#ifndef MONA_FOUND
  GTEST_SKIP() << "mona not found (CMake find_program(mona)); skipping the "
                  "isolated trivial-transducer oracle";
#else
  const std::vector<std::string> pool{"i", "o"};
  std::mt19937 rng(kMtnfaProductIsolatedSeed);
  for (std::size_t case_idx = 0; case_idx < kMtnfaProductIsolatedCaseCount;
      ++case_idx) {
    const std::set<std::string> ap_names = RandomApSubset(pool, rng);
    const spot::formula phi =
        GenerateRandomFormula(ap_names, rng, kMtnfaProductIsolatedTreeSizeMax);
    SCOPED_TRACE("case " + std::to_string(case_idx) + ": phi=" + FormulaStr(phi));

    const spot::bdd_dict_ptr dict = spot::make_bdd_dict();
    const VariablePartition vars = IoFreeVars();
    const OutputLabeledTransducer t_in = trivial_transducer(vars, Role::t_in, dict);
    const OutputLabeledTransducer t_out = trivial_transducer(vars, Role::t_out, dict);
    const std::vector<const Transducer*> taus{&t_in, &t_out};

    const Mtnfa goal = ltlf_to_mtnfa(phi, dict);
    const spot::mtdfa_ptr got = mtnfa_product_to_mtdfa(goal, taus, vars);
    ASSERT_NE(got, nullptr);
    const spot::mtdfa_ptr want = spot::ltlf_to_mtdfa(phi, dict);

    EXPECT_TRUE(spot::product_xor(got, want)->is_empty())
        << "isolated (trivial-transducer) product_xor is non-empty for phi="
        << FormulaStr(phi);
  }
#endif
}

// ---------------------------------------------------------------------------
// SECTION D -- AP-lifetime regression (docs/prd/mtnfa.md "Developer
// comments", entry 3, applied verbatim to mtnfa_product_to_mtdfa per PRD
// "Novel mechanisms (e)"): call the free function on a TEMPORARY Mtnfa,
// discard it, then do an unrelated register_ap / spot::ltlf_to_mtdfa on the
// same dict, and check the product's language is still right.
// ---------------------------------------------------------------------------

TEST(MtnfaProductApLifetime, DiscardingTheMtnfaTemporaryKeepsTheProductLanguageCorrect) {
#ifndef MONA_FOUND
  GTEST_SKIP() << "mona not found (CMake find_program(mona)); skipping the "
                  "bdd_dict lifetime regression test";
#else
  const spot::bdd_dict_ptr dict = spot::make_bdd_dict();
  const VariablePartition vars = MtnfaProductCorpusVars();
  register_turn_order_aps(vars, dict);
  const OutputLabeledTransducer t_in = MtnfaProductCorpusTin(dict);
  const OutputLabeledTransducer t_out = trivial_transducer(vars, Role::t_out, dict);
  const std::vector<const Transducer*> taus{&t_in, &t_out};
  const spot::formula phi = Phi("G(k -> o)");

  // Direct call: the Mtnfa temporary is discarded, only the mtdfa_ptr
  // survives -- the natural calling pattern the public interface invites,
  // and the exact shape that exposed the analogous mtnfa_to_mtdfa bug.
  const spot::mtdfa_ptr got =
      mtnfa_product_to_mtdfa(ltlf_to_mtnfa(phi, dict), taus, vars);
  ASSERT_NE(got, nullptr);

  // Built AFTER `got` returns, on the SAME dict -- exactly the ordering that
  // would expose a variable-number leak if mtnfa_product_to_mtdfa did not
  // register its own AP stake ("Novel mechanisms (e)").
  const spot::mtdfa_ptr want =
      IndependentMtdfaProductReference(phi, dict, t_in, t_out);

  EXPECT_TRUE(spot::product_xor(got, want)->is_empty())
      << "AP-lifetime regression: mtnfa_product_to_mtdfa(phi, dict) called "
        "directly with the Mtnfa temporary discarded, then an unrelated "
        "spot::ltlf_to_mtdfa/emits_dfa build on the SAME dict, must still "
        "agree -- a non-empty XOR here means the returned mtdfa_ptr does not "
        "keep its own AP variables registered";
#endif
}

// ---------------------------------------------------------------------------
// SECTION E -- The expected-divergence test (REQUIRED, PRD "Test oracles").
//
// Reproduction (docs/BACKLOG.md "materialize_product drops F_P on an
// edgeless accepting product state"): phi=b, Ofree={b}, a delta-dead t_in
// state. vars: input_free={} (empty), input_known={"a"}, output_free={"b"},
// output_known={}. t_in: state 0 --(bddtrue)--> state 1, committing a=true
// unconditionally (Sigma0=Ifree=empty, so this is a constant relation);
// state 1 has NO out-edges at all (delta-dead).
//
// A length-1 trace with b=true satisfies phi="b" outright (LTLf: only the
// FIRST letter matters). The only cons-passing letter at the initial move
// is a=true (t_in's sole commitment); the controller is free to pick
// b=true there. The goal automaton reaches an ACCEPTING state on that
// letter, and t_in's state 1 is delta-dead, so the product has NO further
// legal moves from there -- a legitimate finite-trace win
// (def:consistency's partiality clause), NOT an unrealizable dead end.
//
//   - MtnfaProduct puts the acceptance bit on the INCOMING terminal
//     (Relabel, "Novel mechanisms (c)"), so this accepting dead-end keeps
//     its mark regardless of having zero outgoing edges --> predicted
//     REALIZABLE (the PRD's "Known divergence" claim).
//   - NfaProduct materializes the product via materialize_product
//     (src/product.cpp:341), which only attaches F_P INSIDE its
//     per-destination guard loop; an edgeless accepting state emits no
//     edges, so Spot's state_is_accepting (reading a state's FIRST
//     out-edge) reads the mark back as false --> UNREALIZABLE. This is the
//     KNOWN, PRE-EXISTING bug docs/BACKLOG.md records (not introduced by
//     NfaProduct or MtnfaProduct) -- this assertion PINS the bug, it does
//     NOT assert correct behaviour of NfaProduct.
//   - MtdfaProduct was PREDICTED REALIZABLE by the same argument, and that
//     prediction was WRONG -- established by this very fixture, on first
//     run, 2026-07-27.  It reports UNREALIZABLE, for a SECOND INSTANCE of
//     exactly the bug class above: emits_dfa (src/emits_dfa.cpp:49) attaches
//     its acceptance mark only INSIDE the per-edge loop, so a delta-dead
//     transducer state still gets a state (via discover(d)) but ZERO edges
//     and ZERO marks; spot::twadfa_to_mtdfa then reads it back
//     non-accepting, and the intersection rejects.  Confirmed empirically:
//     adding the defensive bddfalse-guarded self-loop that nfa_to_dfa.cpp:105
//     already uses makes this test pass (but breaks two emits_dfa tests, one
//     of which deliberately pins the current edgeless shape -- so the fix is
//     NOT a drive-by).  Fix deliberately DEFERRED, see below.
//
// WHY MtnfaProduct ESCAPES and MtdfaProduct does not -- the load-bearing
// distinction, worth not re-deriving: MtnfaProduct computes the acceptance
// bit as `any(goal.accepting[s] for s in S)` straight off Mtnfa::accepting
// (Relabel), so acceptance NEVER passes through a twa_graph.  MtdfaProduct
// launders it through emits_dfa -> twadfa_to_mtdfa, i.e. through
// state_is_accepting's read-off-the-first-out-edge.  The PRD's "sink-both is
// sound in the mtdfa representation" argument is therefore CONFIRMED where it
// is actually about the mtdfa terminal encoding; what was wrong was
// extrapolating it to a route that round-trips through an explicit automaton.
// The PRD's Behaviour §3 claim stands; only its aside about MtdfaProduct did
// not.
//
// IMPORTANT (PRD "Test oracles", "the divergence direction is predicted,
// not observed"): if MtnfaProduct ever reports UNREALIZABLE here, do NOT
// flip its assertion to match -- that WOULD falsify the sink-both argument
// (Behaviour §3, "Open theory questions touched").  Report it and route it to
// /theory-review.  That warning is specifically about the MtnfaProduct line;
// the MtdfaProduct line below was flipped only because the mechanism was
// identified first, which is the bar for flipping.
//
// WHEN THE CLASS IS FIXED (docs/BACKLOG.md "acceptance mark lost on an
// edgeless accepting state" -- three sites: materialize_product
// src/product.cpp:341, emits_dfa src/emits_dfa.cpp:49, while nfa_to_dfa and
// reverse_dfa_to_nfa defend correctly): flip BOTH the NfaProduct and the
// MtdfaProduct expectations below to EXPECT_TRUE, so all four methods agree,
// and update this comment.
// ---------------------------------------------------------------------------

TEST(MtnfaProductExpectedDivergence,
    MaterializeProductBugMakesNfaProductWronglyUnrealizableButMtnfaAndMtdfaProductAreCorrect) {
#ifndef MONA_FOUND
  GTEST_SKIP() << "mona not found (CMake find_program(mona)); skipping the "
                  "expected-divergence partial-transducer test";
#else
  const spot::bdd_dict_ptr dict = spot::make_bdd_dict();
  const VariablePartition vars =
      VariablePartition::split({"a"}, {"b"}, /*governed=*/{"a"});
  register_turn_order_aps(vars, dict);

  auto tin_g = spot::make_twa_graph(dict);
  const int a = tin_g->register_ap("a");
  tin_g->new_states(2);
  tin_g->set_init_state(0);
  tin_g->new_edge(0, 1, bddtrue);
  // State 1: NO out-edges at all -- delta-dead, the exact repro shape.
  const OutputLabeledTransducer t_in(tin_g, {bdd_ithvar(a), bddfalse},
                                     /*sigma0=*/bddtrue, /*sigma1=*/bdd_ithvar(a));
  const OutputLabeledTransducer t_out = trivial_transducer(vars, Role::t_out, dict);

  const spot::formula phi = Phi("b");

  MtnfaProduct mtnfa_method;
  NfaProduct nfa_method;
  MtdfaProduct mtdfa_method;

  EXPECT_TRUE(mtnfa_method.synthesize(phi, vars, t_in, t_out).has_value())
      << "MtnfaProduct: predicted REALIZABLE (transition-based acceptance "
        "survives the delta-dead t_in state). If this is FALSE, see the "
        "IMPORTANT block above this test: do NOT flip, report prominently "
        "instead -- it would falsify the PRD's sink-both soundness claim";
  EXPECT_FALSE(nfa_method.synthesize(phi, vars, t_in, t_out).has_value())
      << "NfaProduct: KNOWN BUG (docs/BACKLOG.md, materialize_product drops "
        "F_P on an edgeless accepting product state) -- this PINS the bug, "
        "not correct behaviour; flip to EXPECT_TRUE once it is fixed";
  EXPECT_FALSE(mtdfa_method.synthesize(phi, vars, t_in, t_out).has_value())
      << "MtdfaProduct: SAME KNOWN BUG CLASS as NfaProduct above, SECOND SITE "
        "(emits_dfa.cpp:49) -- this PINS the bug, not correct behaviour; flip "
        "to EXPECT_TRUE once the class is fixed";
#endif
}

// ---------------------------------------------------------------------------
// SECTION F -- Structural free-rider: canonical BenchTimer stages, no nested
// "determinize" sub-span (PRD "Benchmarking": the fused cons+determinize
// pass has no intermediate product to time, unlike NfaProduct).
// ---------------------------------------------------------------------------

TEST(BenchScopeIntegration,
    MtnfaProductEmitsCanonicalStagesOnceEachInOrderWithNoNestedDeterminizeSpan) {
#ifndef MONA_FOUND
  GTEST_SKIP() << "mona not found (CMake find_program(mona)); MtnfaProduct "
                  "needs it via ltlf_to_mtnfa";
#endif
  const spot::bdd_dict_ptr dict = spot::make_bdd_dict();
  const VariablePartition vars = IoFreeVars();
  register_turn_order_aps(vars, dict);
  const OutputLabeledTransducer t_in = trivial_transducer(vars, Role::t_in, dict);
  const OutputLabeledTransducer t_out = trivial_transducer(vars, Role::t_out, dict);
  MtnfaProduct method;

  BenchReport report;
  {
    BenchScope scope;
    const std::optional<Controller> controller =
        method.synthesize(Phi("G(i -> o)"), vars, t_in, t_out);
    ASSERT_TRUE(controller.has_value())
        << "fixture must stay realizable for the integration oracle to mean "
          "anything";
    report = scope.report();
  }

  ASSERT_EQ(report.roots.size(), 3u)
      << "expected exactly automaton_construction, product_construction, "
        "game_solving -- the fused pass has no intermediate product to time, "
        "so it must not spill a fourth root";
  const std::vector<Stage> expected_order = {
      Stage::automaton_construction, Stage::product_construction,
      Stage::game_solving};
  for (std::size_t i = 0; i < expected_order.size(); ++i) {
    SCOPED_TRACE("root index " + std::to_string(i));
    EXPECT_EQ(report.roots[i].label,
             std::string(stage_name(expected_order[i])));
    EXPECT_TRUE(report.roots[i].canonical);
    EXPECT_TRUE(report.roots[i].children.empty())
        << "no nested sub-span anywhere -- in particular no 'determinize' "
          "span under product_construction (PRD 'Benchmarking', unlike "
          "NfaProduct)";
  }
  EXPECT_GT(report.total.count(), 0);
}

}  // namespace
