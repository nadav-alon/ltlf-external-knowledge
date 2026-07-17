#include <algorithm>
#include <vector>

#include <bddx.h>
#include <gtest/gtest.h>
#include <spot/twa/bdddict.hh>
#include <spot/twa/twagraph.hh>

#include "ltlf_ek/detail/state_set_pool.hpp"

// Phase 1 smoke tests for docs/prd/mtnfa.md's StateSetPool (glossary:
// "MTNFA" -- detail::StateSetPool is the un-entried set-terminal
// substrate).  Full unit + oracle coverage (idempotence/commutativity, the
// >64-element set, leak/bdd_varnum sanity) is /test-writer's job; this file
// only proves the union apply is wired correctly end to end.
namespace {

using ltlf_ek::detail::StateSetPool;

// Registers one AP on a throwaway twa_graph, the same pattern
// reverse_dfa_to_nfa_test.cpp uses to get a raw bdd variable index.
class StateSetPoolTest : public ::testing::Test {
 protected:
  void SetUp() override {
    dict_ = spot::make_bdd_dict();
    spot::twa_graph_ptr registrar = spot::make_twa_graph(dict_);
    x_ = registrar->register_ap("x");
  }

  spot::bdd_dict_ptr dict_;
  int x_ = -1;
};

TEST_F(StateSetPoolTest, EmptySetInternsAtIndexZero) {
  StateSetPool pool;
  EXPECT_TRUE(pool.set_of(0).empty());
}

TEST_F(StateSetPoolTest, InternIsCanonical) {
  StateSetPool pool;
  const unsigned i1 = pool.intern({1, 2, 3});
  const unsigned i2 = pool.intern({1, 2, 3});
  EXPECT_EQ(i1, i2);
}

TEST_F(StateSetPoolTest, GuardedSingletonBranches) {
  StateSetPool pool;
  const bdd g = pool.guarded_singleton(bdd_ithvar(x_), 5);
  ASSERT_FALSE(bdd_is_terminal(g));
  EXPECT_EQ(pool.set_of(bdd_get_terminal(bdd_high(g))), std::vector<unsigned>{5});
  EXPECT_TRUE(pool.set_of(bdd_get_terminal(bdd_low(g))).empty());
}

// PRD "Phase 1" checkpoint example: set_union(ite(x,{1},{2}),
// ite(x,{2},{3})) == ite(x,{1,2},{2,3}); walked via bdd_get_terminal +
// set_of rather than structural `==` (the PRD-pinned comparison method).
TEST_F(StateSetPoolTest, UnionMergesOverlappingBranches) {
  StateSetPool pool;
  // ite(x,{1},{2}) and ite(x,{2},{3}) -- built directly with bdd_ite/
  // bdd_terminalpp, since `|` is plain BDD-or and not terminal-aware.
  const bdd lhs = bdd_ite(bdd_ithvar(x_), bdd_terminalpp(static_cast<int>(pool.intern({1}))),
                          bdd_terminalpp(static_cast<int>(pool.intern({2}))));
  const bdd rhs = bdd_ite(bdd_ithvar(x_), bdd_terminalpp(static_cast<int>(pool.intern({2}))),
                          bdd_terminalpp(static_cast<int>(pool.intern({3}))));

  const bdd u = pool.set_union(lhs, rhs);
  ASSERT_FALSE(bdd_is_terminal(u));
  EXPECT_EQ(pool.set_of(bdd_get_terminal(bdd_high(u))), (std::vector<unsigned>{1, 2}));
  EXPECT_EQ(pool.set_of(bdd_get_terminal(bdd_low(u))), (std::vector<unsigned>{2, 3}));
}

TEST_F(StateSetPoolTest, UnionIsIdempotent) {
  StateSetPool pool;
  const bdd a = bdd_ite(bdd_ithvar(x_), bdd_terminalpp(static_cast<int>(pool.intern({1}))),
                        bdd_terminalpp(static_cast<int>(pool.intern({2}))));
  EXPECT_TRUE(pool.set_union(a, a) == a);
}

TEST_F(StateSetPoolTest, EmptyTerminalIsUnionIdentity) {
  StateSetPool pool;
  const bdd a = bdd_ite(bdd_ithvar(x_), bdd_terminalpp(static_cast<int>(pool.intern({1}))),
                        bdd_terminalpp(static_cast<int>(pool.intern({2}))));
  const bdd empty = bdd_terminalpp(0);
  const bdd u = pool.set_union(a, empty);
  ASSERT_FALSE(bdd_is_terminal(u));
  EXPECT_EQ(pool.set_of(bdd_get_terminal(bdd_high(u))), (std::vector<unsigned>{1}));
  EXPECT_EQ(pool.set_of(bdd_get_terminal(bdd_low(u))), (std::vector<unsigned>{2}));
}

// --- Phase-1 gaps the smoke tests above do not cover (PRD "Test oracles"
// / "Implementation phases" Phase-1 checkpoint) ------------------------------

// Edge case "Set grows past machine-word width": the interned index is a
// StateSetPool-assigned unsigned, not a bitmask-in-an-int -- a set with an
// element index >= 64 could not be represented at all by a 64-bit-word
// bitmask encoding, so faithfully round-tripping one here is a real
// assertion against that rejected design, not a style preference.
TEST_F(StateSetPoolTest, InternsSetPast64ElementsWithoutBitmaskCorruption) {
  StateSetPool pool;
  std::vector<unsigned> big;
  for (unsigned i = 0; i < 130; ++i) big.push_back(i);  // includes indices >= 64
  const unsigned idx = pool.intern(big);
  EXPECT_EQ(pool.set_of(idx), big);
  // Re-interning the identical (large) set is canonical: same index.
  EXPECT_EQ(pool.intern(big), idx);

  // The union apply must also merge correctly past the 64-element boundary:
  // two large sets sharing only their tail overlap into one MTBDD via the
  // same ite(x, .., ..) shape as UnionMergesOverlappingBranches above.
  std::vector<unsigned> other;
  for (unsigned i = 100; i < 200; ++i) other.push_back(i);
  const bdd lhs = bdd_ite(bdd_ithvar(x_), bdd_terminalpp(static_cast<int>(idx)),
                          bdd_terminalpp(static_cast<int>(pool.intern({1}))));
  const bdd rhs = bdd_ite(bdd_ithvar(x_), bdd_terminalpp(static_cast<int>(pool.intern(other))),
                          bdd_terminalpp(static_cast<int>(pool.intern({1}))));
  const bdd u = pool.set_union(lhs, rhs);
  ASSERT_FALSE(bdd_is_terminal(u));
  // Union of {0..129} and {100..199}: sorted, de-duplicated {0..199}.
  std::vector<unsigned> want_high = big;
  want_high.insert(want_high.end(), other.begin(), other.end());
  std::sort(want_high.begin(), want_high.end());
  want_high.erase(std::unique(want_high.begin(), want_high.end()), want_high.end());
  EXPECT_EQ(pool.set_of(bdd_get_terminal(bdd_high(u))), want_high);
  EXPECT_EQ(pool.set_of(bdd_get_terminal(bdd_low(u))), (std::vector<unsigned>{1}));
}

// "Novel mechanisms (b)": set_union is commutative; BuDDy canonicalizes
// equal MTBDDs to the same physical node, so this is checked with exact `==`
// (structural/id equality), not a walk-and-compare.
TEST_F(StateSetPoolTest, UnionIsCommutative) {
  StateSetPool pool;
  const bdd a = bdd_ite(bdd_ithvar(x_), bdd_terminalpp(static_cast<int>(pool.intern({1, 5}))),
                        bdd_terminalpp(static_cast<int>(pool.intern({2}))));
  const bdd b = bdd_ite(bdd_ithvar(x_), bdd_terminalpp(static_cast<int>(pool.intern({5, 9}))),
                        bdd_terminalpp(static_cast<int>(pool.intern({2, 3}))));
  EXPECT_TRUE(pool.set_union(a, b) == pool.set_union(b, a));
}

// No-leak / stable-bdd_varnum sanity pass (PRD Phase-1 checkpoint): the pool
// never registers a new BDD *variable* -- guarded_singleton/set_union only
// ever consume the caller-supplied guard's existing variable(s) and mint new
// *terminal* nodes, which live outside the variable table. A long run of
// interning + unioning must not grow bdd_varnum(), and re-interning an
// already-canonical set must not grow the pool's storage (no duplicate
// leaked entries for equal sets).
TEST_F(StateSetPoolTest, ManyInternsAndUnionsLeaveBddVarnumAndInterningStable) {
  StateSetPool pool;
  const int varnum_before = bdd_varnum();

  bdd row = bdd_terminalpp(0);
  for (unsigned i = 0; i < 300; ++i)
    row = pool.set_union(row, pool.guarded_singleton(bdd_ithvar(x_), i % 80));
  EXPECT_EQ(bdd_varnum(), varnum_before)
      << "StateSetPool operations must not mint new BDD variables";

  // Re-interning the same 80-element set (built above, index-canonical)
  // repeatedly must keep returning the same index, not grow sets_.
  std::vector<unsigned> eighty;
  for (unsigned i = 0; i < 80; ++i) eighty.push_back(i);
  const unsigned first = pool.intern(eighty);
  for (int i = 0; i < 50; ++i) EXPECT_EQ(pool.intern(eighty), first);
}

}  // namespace
