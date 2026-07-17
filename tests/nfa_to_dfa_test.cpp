#include <optional>
#include <vector>

#include <bddx.h>
#include <gtest/gtest.h>
#include <spot/twa/acc.hh>
#include <spot/twa/bdddict.hh>
#include <spot/twa/twagraph.hh>
#include <spot/twaalgos/isdet.hh>

#include "ltlf_ek/nfa_to_dfa.hpp"

// Structural unit fixtures for ltlf_ek::nfa_to_dfa (docs/prd/nfa-product.md
// Phase 1, "Test oracles": "structural (no MONA)"; main.tex
// alg:nfa_product:determinize). Hand-built NFAs, no MONA, no phi -- the
// generic explicit-subset-construction fact nfa_to_dfa is built on: for ANY
// (possibly nondeterministic, possibly incomplete) N, delta_D(R, v) = union
// over s in R of N's delta(s, v), the empty subset is skipped, and R is
// accepting iff it intersects N's final states. The independent, MONA-gated
// corpus oracle (nfa_to_dfa(ltlf_to_nfa(phi)) vs ltlf_to_dfa(phi)) lives in
// tests/ltlfsynt_oracle_test.cpp alongside the existing ltlf_to_nfa language
// oracles it reuses.
namespace {

using ltlf_ek::nfa_to_dfa;

const spot::acc_cond::mark_t kFinal = {0};
const spot::acc_cond::mark_t kNone = {};

// The unique out-edge of `src` whose guard `letter` satisfies, or nullopt if
// none does -- a deterministic-modulo-skip read, mirroring
// OutputLabeledTransducer::delta's own "at most one matching guard" idiom.
// ADD_FAILUREs (rather than throwing) if more than one edge matches, so a
// determinism regression shows up as a normal test failure.
std::optional<unsigned> EdgeTarget(const spot::twa_graph_ptr& g, unsigned src,
                                   bdd letter) {
  std::optional<unsigned> dst;
  for (const auto& e : g->out(src)) {
    if ((letter & e.cond) != bddfalse) {
      if (dst.has_value())
        ADD_FAILURE() << "state " << src
                      << " has more than one edge matching this letter";
      dst = e.dst;
    }
  }
  return dst;
}

unsigned OutDegree(const spot::twa_graph_ptr& g, unsigned s) {
  unsigned n = 0;
  for (const auto& e : g->out(s)) ++n;
  return n;
}

// --- Merge fixture: nondeterministic branch + ∅-skip + accepting rule ------
//
// N (over ap 'a'), 4 states:
//   n0 (init, non-acc) --!a--> n3
//   n0                 --a-->  n1
//   n0                 --a-->  n2   (nondeterministic: SAME guard 'a', two
//                                    distinct destinations -- the PRD's
//                                    "a nondeterministic NFA whose two
//                                    successors merge into one subset")
//   n1 (acc)            --a-->  n1  (no !a edge)
//   n2 (non-acc)                    (ZERO outgoing edges -- a total dead end)
//   n3 (non-acc)        --a-->  n1  (no !a edge)
//
// Hand-traced subset construction (minterms enumerated [!a, a], LSB-first --
// nfa_to_dfa.cpp's all_minterms order -- and BFS discovery order, both fully
// deterministic per the header doc-comment, so exact output ids are pinned):
//   R0 = {n0}      -> id 0 (the seeded initial subset)
//   R0 --!a--> {n3}    -> id 1 (new)
//   R0 --a-->  {n1,n2} -> id 2 (new; the nondeterministic-branch merge)
//   R1={n3} --!a--> empty (SKIP)
//   R1={n3} --a-->  {n1}     -> id 3 (new)
//   R2={n1,n2} --!a--> empty (SKIP: neither n1 nor n2 has a !a-edge, and n2
//                              has no edges at all)
//   R2={n1,n2} --a-->  {n1}  -> REUSES id 3 (interning collapses this
//                              independently-reached subset onto the same
//                              output state R1's 'a'-successor already
//                              created -- a second flavour of "merge")
//   R3={n1}    --!a--> empty (SKIP)
//   R3={n1}    --a-->  {n1}  -> self (id 3)
class NfaToDfaMergeFixture : public ::testing::Test {
 protected:
  void SetUp() override {
    dict_ = spot::make_bdd_dict();
    spot::twa_graph_ptr registrar = spot::make_twa_graph(dict_);
    a_ = registrar->register_ap("a");

    n_ = spot::make_twa_graph(dict_);
    n_->register_ap("a");
    n_->set_buchi();
    n_->prop_state_acc(true);
    n_->new_states(4);
    n_->set_init_state(0);
    // n0 = 0 (non-acc), n1 = 1 (acc), n2 = 2 (non-acc, dead end), n3 = 3
    // (non-acc).
    n_->new_edge(0, 3, bdd_nithvar(a_), kNone);
    n_->new_edge(0, 1, bdd_ithvar(a_), kNone);
    n_->new_edge(0, 2, bdd_ithvar(a_), kNone);
    n_->new_edge(1, 1, bdd_ithvar(a_), kFinal);
    n_->new_edge(3, 1, bdd_ithvar(a_), kNone);
    // n2: no edges at all.

    d_ = nfa_to_dfa(n_);
  }

  spot::bdd_dict_ptr dict_;
  spot::twa_graph_ptr n_;
  spot::twa_graph_ptr d_;
  int a_ = -1;
};

TEST_F(NfaToDfaMergeFixture, InitialSubsetIsStateZero) {
  // "states[0] is {init}" (PRD Phase 1 structural checkpoint) -- solve_dfa's
  // expectation, checked directly rather than inferred from num_states.
  EXPECT_EQ(d_->get_init_state_number(), 0u);
}

TEST_F(NfaToDfaMergeFixture, HasExactlyFourReachableSubsets) {
  // {n0}, {n3}, {n1,n2}, {n1} -- no more, no fewer (confirms the two
  // "merge" points above did not each spawn a spurious extra state).
  ASSERT_NE(d_, nullptr);
  EXPECT_EQ(d_->num_states(), 4u);
}

TEST_F(NfaToDfaMergeFixture, EmptySkipDropsMissingSuccessorEdges) {
  // {n3} (id 1), {n1,n2} (id 2), and {n1} (id 3) each have NO out-edge for
  // !a (their !a-successor subset is empty) -- ∅-skip: no edge, not a sink
  // edge.
  const bdd not_a = bdd_nithvar(a_);
  EXPECT_EQ(OutDegree(d_, 1), 1u) << "id 1 ({n3}) should have only its 'a' edge";
  EXPECT_FALSE(EdgeTarget(d_, 1, not_a).has_value());
  EXPECT_EQ(OutDegree(d_, 2), 1u) << "id 2 ({n1,n2}) should have only its 'a' edge";
  EXPECT_FALSE(EdgeTarget(d_, 2, not_a).has_value());
  EXPECT_EQ(OutDegree(d_, 3), 1u) << "id 3 ({n1}) should have only its 'a' edge";
  EXPECT_FALSE(EdgeTarget(d_, 3, not_a).has_value());
}

TEST_F(NfaToDfaMergeFixture, AcceptingIffIntersectsFinals) {
  // {n0} and {n3} contain no N-accepting state -- non-accepting; {n1,n2} and
  // {n1} both contain n1 (accepting) -- accepting, regardless of n2's
  // (non-accepting) membership alongside it.
  EXPECT_FALSE(d_->state_is_accepting(0u)) << "{n0}";
  EXPECT_FALSE(d_->state_is_accepting(1u)) << "{n3}";
  EXPECT_TRUE(d_->state_is_accepting(2u)) << "{n1,n2}";
  EXPECT_TRUE(d_->state_is_accepting(3u)) << "{n1}";
}

TEST_F(NfaToDfaMergeFixture, NondeterministicBranchMergesIntoOneAcceptingSubset) {
  // The point of the method (PRD "Edge cases": "a letter whose two goal
  // successors are both in R merges into one R'"): n0's two DIFFERENT
  // out-edges on the SAME guard 'a' (to n1 and n2) become ONE output DFA
  // state (id 2), not two -- and that single state is accepting because n1,
  // one of its two merged members, is.
  const bdd a_true = bdd_ithvar(a_);
  const std::optional<unsigned> merged = EdgeTarget(d_, 0, a_true);
  ASSERT_TRUE(merged.has_value());
  EXPECT_EQ(*merged, 2u);
  EXPECT_TRUE(d_->state_is_accepting(*merged));
}

TEST_F(NfaToDfaMergeFixture, InterningCollapsesIndependentlyReachedSubsets) {
  // A second flavour of merge: id 1 ({n3}) and id 2 ({n1,n2}) both reach
  // {n1} via 'a' -- id 3 was created once (from id 1's edge) and REUSED (not
  // recreated) when id 2 independently computed the identical subset.
  const bdd a_true = bdd_ithvar(a_);
  const std::optional<unsigned> from_id1 = EdgeTarget(d_, 1, a_true);
  const std::optional<unsigned> from_id2 = EdgeTarget(d_, 2, a_true);
  ASSERT_TRUE(from_id1.has_value());
  ASSERT_TRUE(from_id2.has_value());
  EXPECT_EQ(*from_id1, *from_id2)
      << "two different predecessors reaching the SAME subset {n1} must "
         "land on the SAME output state, not duplicate it";
  EXPECT_EQ(*from_id1, 3u);
}

TEST_F(NfaToDfaMergeFixture, OutputIsDeterministicButNotComplete) {
  // "determinism modulo skip": the output DFA never has two edges out of one
  // state matching the same letter (spot::is_deterministic holds regardless
  // of completeness), yet it IS incomplete (the ∅-skip edges above are
  // genuinely missing, not sink edges) -- confirmed both ways rather than
  // just inferred from the per-state edge counts above.
  EXPECT_TRUE(spot::is_deterministic(d_));
  EXPECT_FALSE(spot::is_complete(d_));
}

// --- phi=0 shape -------------------------------------------------------

// PRD "Edge cases": phi=0 (ff) yields L(N)=empty, so N (mirroring what
// ltlf_to_nfa(0) itself produces) is a single non-accepting state with no
// outgoing edges and no registered APs (collect_aps(0) = empty). nfa_to_dfa
// must reproduce that shape verbatim (one non-accepting state, no edges) and
// must not crash or return nullptr on an empty-AP input.
TEST(NfaToDfa, PhiFalseShapeSingleNonAcceptingInitialNoEdges) {
  spot::bdd_dict_ptr dict = spot::make_bdd_dict();
  spot::twa_graph_ptr n = spot::make_twa_graph(dict);
  n->set_buchi();
  n->prop_state_acc(true);
  n->new_states(1);
  n->set_init_state(0);
  // No edges, no registered APs.

  const spot::twa_graph_ptr d = nfa_to_dfa(n);
  ASSERT_NE(d, nullptr);
  EXPECT_EQ(d->num_states(), 1u);
  EXPECT_EQ(d->get_init_state_number(), 0u);
  EXPECT_FALSE(d->state_is_accepting(0u));
  EXPECT_EQ(OutDegree(d, 0), 0u);
}

// --- Accepting dead-end (PRD "Edge cases", mirroring
// tests/reverse_dfa_to_nfa_test.cpp's AcceptingDeadEnd fixture one level up)
// -------------------------------------------------------------------------

// N: a single ACCEPTING state whose ONLY out-edge is itself a defensive
// bddfalse-guarded self-loop (carrying the Final mark) -- the same fix
// reverse_dfa_to_nfa applies one level down, needed here for the SAME
// reason: spot::twa_graph::state_is_accepting reads a state-based mark off
// the state's FIRST out-edge, so a genuinely edgeless state could never
// read back as accepting even in N itself. Because that edge's guard is
// bddfalse, it never satisfies any real minterm (v & bddfalse == bddfalse
// for every v), so both minterms (!a, a) are still ∅-skipped from this
// state's perspective -- the subset {that state} (R0, the initial subset)
// gets no REAL out-edge in D. nfa_to_dfa must therefore add ITS OWN
// defensive bddfalse self-loop carrying the Final mark one level up, or this
// accepting subset would silently read back non-accepting in D.
TEST(NfaToDfa, AcceptingDeadEndGetsDefensiveBddfalseSelfLoopCarryingFinalMark) {
  spot::bdd_dict_ptr dict = spot::make_bdd_dict();
  spot::twa_graph_ptr registrar = spot::make_twa_graph(dict);
  const int a = registrar->register_ap("a");

  spot::twa_graph_ptr n = spot::make_twa_graph(dict);
  n->register_ap("a");
  n->set_buchi();
  n->prop_state_acc(true);
  n->new_states(1);
  n->set_init_state(0);
  n->new_edge(0, 0, bddfalse, spot::acc_cond::mark_t({0}));
  ASSERT_TRUE(n->state_is_accepting(0u))
      << "fixture sanity: N's own accepting mark must actually be readable, "
         "or this test exercises nothing";

  const spot::twa_graph_ptr d = nfa_to_dfa(n);
  ASSERT_NE(d, nullptr);
  EXPECT_EQ(d->num_states(), 1u);
  EXPECT_TRUE(d->state_is_accepting(0u))
      << "the defensive self-loop must carry the Final mark, or "
         "state_is_accepting reads back false with no real out-edge to "
         "consult";
  EXPECT_EQ(OutDegree(d, 0), 1u)
      << "exactly the one defensive self-loop, no real successor";
  const auto edges = d->out(0);
  auto it = edges.begin();
  ASSERT_NE(it, edges.end());
  EXPECT_EQ(it->cond, bddfalse)
      << "the defensive self-loop must never be taken by any real letter";
  EXPECT_EQ(it->dst, 0u);
  ++it;
  EXPECT_EQ(it, edges.end()) << "no other out-edge";
  (void)a;
}

}  // namespace
