#include <gtest/gtest.h>
#include <bddx.h>
#include <spot/twa/bdddict.hh>
#include <spot/twa/twagraph.hh>

#include "ltlf_ek/product.hpp"

// minimized_state_count: Moore minimization of an explicit product, the
// completion sink counted on neither side (docs/GLOSSARY.md "Minimized product
// states"). Products are hand-built over one letter variable `a`.
namespace {

using ltlf_ek::minimized_state_count;

struct Product {
  spot::bdd_dict_ptr dict = spot::make_bdd_dict();
  spot::twa_graph_ptr g = spot::make_twa_graph(dict);
  bdd a;

  Product() {
    g->set_buchi();
    g->prop_state_acc(true);
    a = bdd_ithvar(g->register_ap("a"));
  }
  // State-based acceptance: every out-edge of a state carries its mark.
  void edge(unsigned s, unsigned d, bdd guard, bool src_accepting) {
    g->new_edge(s, d, guard,
                src_accepting ? spot::acc_cond::mark_t{0} : spot::acc_cond::mark_t{});
  }
};

// init --a--> x, init --!a--> y; x and y both accept and loop on every letter,
// so they have the same language but are two states.
TEST(MinimizedStateCount, MergesTwoLanguageEquivalentUnmergedStates) {
  Product p;
  const unsigned init = p.g->new_state(), x = p.g->new_state(), y = p.g->new_state();
  p.g->set_init_state(init);
  p.edge(init, x, p.a, false);
  p.edge(init, y, !p.a, false);
  p.edge(x, x, bddtrue, true);
  p.edge(y, y, bddtrue, true);
  ASSERT_EQ(p.g->num_states(), 3u);
  EXPECT_EQ(minimized_state_count(p.g), 2u);
}

TEST(MinimizedStateCount, KeepsStatesWithDifferentLanguages) {
  Product p;
  const unsigned init = p.g->new_state(), x = p.g->new_state(), y = p.g->new_state();
  p.g->set_init_state(init);
  p.edge(init, x, p.a, false);
  p.edge(init, y, !p.a, false);
  p.edge(x, x, bddtrue, true);
  p.edge(y, init, bddtrue, true);  // y can leave the accepting loop
  EXPECT_EQ(minimized_state_count(p.g), 3u);
}

// The same product with and without an explicit rejecting sink (and a
// letter with no edge at all) counts the same: the completion sink is counted
// on neither side.
TEST(MinimizedStateCount, CompletionSinkIsCountedOnNeitherSide) {
  Product partial;
  {
    const unsigned s0 = partial.g->new_state(), s1 = partial.g->new_state();
    partial.g->set_init_state(s0);
    partial.edge(s0, s1, partial.a, false);  // !a: no edge
    partial.edge(s1, s1, bddtrue, true);
  }
  Product complete;
  {
    const unsigned s0 = complete.g->new_state(), s1 = complete.g->new_state();
    const unsigned sink = complete.g->new_state();
    complete.g->set_init_state(s0);
    complete.edge(s0, s1, complete.a, false);
    complete.edge(s0, sink, !complete.a, false);
    complete.edge(s1, s1, bddtrue, true);
    complete.edge(sink, sink, bddtrue, false);
  }
  EXPECT_EQ(minimized_state_count(partial.g), 2u);
  EXPECT_EQ(minimized_state_count(complete.g), minimized_state_count(partial.g));
}

TEST(MinimizedStateCount, ProductThatNeverAcceptsCountsZero) {
  Product p;
  const unsigned s = p.g->new_state();
  p.g->set_init_state(s);
  p.edge(s, s, bddtrue, false);
  EXPECT_EQ(minimized_state_count(p.g), 0u);
}

}  // namespace
