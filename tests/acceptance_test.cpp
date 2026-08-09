#include <bddx.h>
#include <gtest/gtest.h>
#include <spot/twa/acc.hh>
#include <spot/twa/bdddict.hh>
#include <spot/twa/twagraph.hh>

#include "ltlf_ek/detail/acceptance.hpp"

// O4 (docs/prd/acceptance-mark-on-edgeless-states.md "Test oracles"): one
// test per pinned behaviour of ensure_acceptance_readable ("Interfaces &
// types"). `ensure_acceptance_readable` deliberately gets NO
// docs/GLOSSARY.md entry (the PRD says so explicitly, "BuDDy/Spot
// representation mechanics, not a domain concept") -- this file is its only
// test coverage.
namespace {

using ltlf_ek::detail::ensure_acceptance_readable;

// A one-state graph, state-based Buchi acceptance (the precondition the
// helper's doc comment pins: "g uses state-based acceptance
// (prop_state_acc(true))"), mirroring the shape every adopting call site
// (materialize_product / emits_dfa / nfa_to_dfa / reverse_dfa_to_nfa) already
// sets up before calling the helper.
spot::twa_graph_ptr MakeGraph(unsigned n_states) {
  spot::bdd_dict_ptr dict = spot::make_bdd_dict();
  spot::twa_graph_ptr g = spot::make_twa_graph(dict);
  g->new_states(n_states);
  g->set_init_state(0);
  g->set_buchi();
  g->prop_state_acc(true);
  return g;
}

const spot::acc_cond::mark_t kMark = {0};
const spot::acc_cond::mark_t kNoMark = {};

TEST(EnsureAcceptanceReadable, NoOpWhenOutEdgesAlreadyExist) {
  spot::twa_graph_ptr g = MakeGraph(1);
  g->new_edge(0, 0, bddtrue, kNoMark);
  ensure_acceptance_readable(g, 0, kMark);
  const auto out = g->out(0);
  auto it = out.begin();
  ASSERT_NE(it, out.end());
  EXPECT_EQ(it->cond, bddtrue) << "the existing out-edge must not be touched";
  ++it;
  EXPECT_EQ(it, out.end())
      << "an out-edge already exists: no second edge should be added";
}

TEST(EnsureAcceptanceReadable, NoOpWhenMarkIsEmpty) {
  spot::twa_graph_ptr g = MakeGraph(1);  // zero out-edges.
  ensure_acceptance_readable(g, 0, kNoMark);
  const auto out = g->out(0);
  EXPECT_EQ(out.begin(), out.end())
      << "a non-accepting edgeless state needs no carrier";
}

TEST(EnsureAcceptanceReadable, AddsExactlyOneBddfalseSelfLoopOtherwise) {
  spot::twa_graph_ptr g = MakeGraph(1);  // zero out-edges.
  ensure_acceptance_readable(g, 0, kMark);
  const auto out = g->out(0);
  auto it = out.begin();
  ASSERT_NE(it, out.end());
  EXPECT_EQ(it->src, 0u);
  EXPECT_EQ(it->dst, 0u) << "self-loop";
  EXPECT_EQ(it->cond, bddfalse) << "never taken by any real letter";
  EXPECT_EQ(it->acc, kMark);
  ++it;
  EXPECT_EQ(it, out.end()) << "exactly one out-edge should have been added";
}

TEST(EnsureAcceptanceReadable, IdempotentUnderASecondCall) {
  spot::twa_graph_ptr g = MakeGraph(1);  // zero out-edges.
  ensure_acceptance_readable(g, 0, kMark);
  ensure_acceptance_readable(g, 0, kMark);  // the first call already left an edge.
  const auto out = g->out(0);
  auto it = out.begin();
  ASSERT_NE(it, out.end());
  ++it;
  EXPECT_EQ(it, out.end())
      << "a second call must not add a second self-loop";
}

TEST(EnsureAcceptanceReadable,
    StateIsAcceptingReadsTrueAfterwardsWhereItReadFalseBefore) {
  spot::twa_graph_ptr g = MakeGraph(1);  // zero out-edges.
  EXPECT_FALSE(g->state_is_accepting(0u))
      << "zero out-edges: state_is_accepting reads the (nonexistent) first "
         "out-edge's mark, so it reads false before the fix -- this is "
         "exactly the bug the PRD repairs";
  ensure_acceptance_readable(g, 0, kMark);
  EXPECT_TRUE(g->state_is_accepting(0u));
}

}  // namespace
