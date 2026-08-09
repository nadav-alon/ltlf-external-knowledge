#include <set>
#include <string>
#include <vector>

#include <bddx.h>
#include <gtest/gtest.h>
#include <spot/twa/acc.hh>
#include <spot/twa/bdddict.hh>
#include <spot/twa/twagraph.hh>
#include <spot/twaalgos/isdet.hh>

#include "ltlf_ek/detail/reverse_dfa_to_nfa.hpp"

// Unit fixtures for ltlf_ek::detail::reverse_dfa_to_nfa (docs/prd/ltlf-to-nfa.md
// Phase 3, main.tex S160-169 alg:ltlftonfa:reverse). Exercises the function in
// isolation on small hand-built DFAs -- no MONA, no phi -- checking the
// *generic* automaton fact the construction is built on: for any deterministic,
// complete, state-based-accepting D, L(reverse_dfa_to_nfa(D)) = reverse(L(D)).
// That property holds for an arbitrary D (not just one obtained as
// past_ltlf_to_dfa(mirror(phi))), so it is checked directly here rather than
// via the generated-corpus phi oracle (tests/ltlfsynt_oracle_test.cpp's
// GeneratedCorpus.LtlfToNfa* additions), which is Phase 3's PRD-mandated
// checkpoint oracle at the (phi, ltlf_to_dfa) granularity, not this function's.
namespace {

using ltlf_ek::detail::reverse_dfa_to_nfa;

// --- D-fixture builder ------------------------------------------------------

// One transition of a hand-built D: src --cond--> dst.
struct DEdge {
  unsigned src;
  bdd cond;
  unsigned dst;
};

// Builds a deterministic, complete, state-based-accepting spot::twa_graph over
// `aps`, matching the mark-on-out-edge convention reverse_dfa_to_nfa itself
// depends on (src/mona_dfa.cpp, src/ltlf_to_dfa.cpp): an edge is marked Final
// iff its SOURCE is in `accepting`. Callers are responsible for supplying a
// deterministic + complete edge set (not enforced here) -- reverse_dfa_to_nfa
// documents that precondition, it does not defend against violating it.
spot::twa_graph_ptr BuildDfa(const spot::bdd_dict_ptr& dict,
                             const std::vector<std::string>& aps,
                             unsigned num_states, unsigned init,
                             const std::set<unsigned>& accepting,
                             const std::vector<DEdge>& edges) {
  spot::twa_graph_ptr d = spot::make_twa_graph(dict);
  for (const std::string& ap : aps) d->register_ap(ap);
  d->set_buchi();
  d->prop_state_acc(true);
  d->new_states(num_states);
  d->set_init_state(init);
  const spot::acc_cond::mark_t kFinal = {0};
  const spot::acc_cond::mark_t kNone = {};
  for (const DEdge& e : edges)
    d->new_edge(e.src, e.dst, e.cond, accepting.count(e.src) ? kFinal : kNone);
  return d;
}

// --- Deterministic D traversal / nondeterministic N traversal --------------

// Deterministic run of `d` from its init state over `word`; a missing
// out-edge (should not happen for a genuinely complete D) rejects rather than
// throwing.
bool DAccepts(const spot::twa_graph_ptr& d, const std::vector<bdd>& word) {
  unsigned s = d->get_init_state_number();
  for (const bdd& v : word) {
    bool moved = false;
    for (const auto& e : d->out(s)) {
      if ((v & e.cond) != bddfalse) {
        s = e.dst;
        moved = true;
        break;
      }
    }
    if (!moved) return false;
  }
  return d->state_is_accepting(s);
}

// Subset-construction membership for the (possibly nondeterministic, possibly
// partial) N: tracks the *set* of states reachable on `word` and accepts iff
// any survivor is accepting. An empty survivor set (a stuck NFA -- expected,
// since N is deliberately not completed) rejects.
bool NfaAccepts(const spot::twa_graph_ptr& n, const std::vector<bdd>& word) {
  std::set<unsigned> current{n->get_init_state_number()};
  for (const bdd& v : word) {
    std::set<unsigned> next;
    for (unsigned s : current)
      for (const auto& e : n->out(s))
        if ((v & e.cond) != bddfalse) next.insert(e.dst);
    current = std::move(next);
    if (current.empty()) return false;
  }
  for (unsigned s : current)
    if (n->state_is_accepting(s)) return true;
  return false;
}

std::vector<bdd> Reversed(std::vector<bdd> word) {
  std::vector<bdd> out(word.rbegin(), word.rend());
  return out;
}

unsigned NumAcceptingStates(const spot::twa_graph_ptr& n) {
  unsigned count = 0;
  for (unsigned s = 0; s < n->num_states(); ++s)
    if (n->state_is_accepting(s)) ++count;
  return count;
}

std::set<std::string> ApNames(const spot::twa_graph_ptr& g) {
  std::set<std::string> names;
  for (const spot::formula& ap : g->ap()) names.insert(ap.ap_name());
  return names;
}

// Every word of length 0..max_len over a single Boolean AP `a`, as {a, !a}^*
// cubes -- small enough (2^0 + ... + 2^max_len) to enumerate exhaustively.
std::vector<std::vector<bdd>> AllWordsOverA(int a_var, unsigned max_len) {
  std::vector<std::vector<bdd>> words{{}};  // length 0 (the empty word).
  for (unsigned len = 1; len <= max_len; ++len) {
    std::vector<std::vector<bdd>> next;
    for (const std::vector<bdd>& w : words) {
      if (w.size() != len - 1) continue;
      std::vector<bdd> w_true = w, w_false = w;
      w_true.push_back(bdd_ithvar(a_var));
      w_false.push_back(bdd_nithvar(a_var));
      next.push_back(w_true);
      next.push_back(w_false);
    }
    words.insert(words.end(), next.begin(), next.end());
  }
  return words;
}

// --- Fixture 1: "last letter is a" (2 states, genuinely nondeterministic
// reversal) -------------------------------------------------------------
//
// D: 0 (init, non-accepting), 1 (accepting); 0-a->1, 0-!a->0, 1-a->1, 1-!a->0.
// D's next state depends only on the letter just read (a -> 1, !a -> 0), so
// L(D) = { w : w's LAST letter has a=true }. Two different D-states (0 and 1)
// both transition to D-state 1 on 'a' -- when reversed, N's state "1" gets
// TWO outgoing edges on the same guard 'a' (to 0 and to 1): this is exactly
// where reverse_dfa_to_nfa produces a genuinely nondeterministic N (PRD
// invariant 3: "N is nondeterministic ... not required deterministic").
class ReverseDfaToNfaLastLetter : public ::testing::Test {
 protected:
  void SetUp() override {
    dict_ = spot::make_bdd_dict();
    spot::twa_graph_ptr registrar = spot::make_twa_graph(dict_);
    a_ = registrar->register_ap("a");
    d_ = BuildDfa(dict_, {"a"}, /*num_states=*/2, /*init=*/0,
                  /*accepting=*/{1},
                  {
                      {0, bdd_ithvar(a_), 1},
                      {0, bdd_nithvar(a_), 0},
                      {1, bdd_ithvar(a_), 1},
                      {1, bdd_nithvar(a_), 0},
                  });
    n_ = reverse_dfa_to_nfa(d_);
  }

  spot::bdd_dict_ptr dict_;
  spot::twa_graph_ptr d_;
  spot::twa_graph_ptr n_;
  int a_ = -1;
};

TEST_F(ReverseDfaToNfaLastLetter, AlphabetMatchesD) {
  EXPECT_EQ(ApNames(n_), ApNames(d_));
}

TEST_F(ReverseDfaToNfaLastLetter, HasExactlyOneAcceptingState) {
  // PRD invariant 2 / "Structural free-riders": F_N is the single state
  // s_{D,0}.
  EXPECT_EQ(NumAcceptingStates(n_), 1u);
}

TEST_F(ReverseDfaToNfaLastLetter, IsGenuinelyNondeterministic) {
  // PRD invariant 3: N is NOT required deterministic. This fixture's reversal
  // structurally produces two edges out of one N-state on the same guard 'a'
  // (D-states 0 and 1 both transition to D-state 1 on 'a') -- confirmed via
  // Spot's own determinism check, not asserted by construction alone.
  EXPECT_FALSE(spot::is_deterministic(n_));
}

TEST_F(ReverseDfaToNfaLastLetter, LanguageIsReverseOfDsLanguage) {
  // The generic fact reverse_dfa_to_nfa is built on: L(N) = reverse(L(D)),
  // for ANY deterministic complete state-based-accepting D -- not just one
  // arising from a real phi's mirror. Checked exhaustively over every word of
  // length 0..5 on the single AP.
  for (const std::vector<bdd>& w : AllWordsOverA(a_, /*max_len=*/5)) {
    SCOPED_TRACE("word length " + std::to_string(w.size()));
    EXPECT_EQ(NfaAccepts(n_, w), DAccepts(d_, Reversed(w)))
        << "L(reverse_dfa_to_nfa(D)) != reverse(L(D))";
  }
}

TEST_F(ReverseDfaToNfaLastLetter, RejectsEmptyWord) {
  // Consequence of the language-reversal property above (0 not accepting in
  // D), stated as its own small assertion since the empty-word/non-empty-
  // trace boundary is a load-bearing PRD edge case.
  EXPECT_FALSE(NfaAccepts(n_, {}));
}

// --- Fixture 2: accepting dead-end s_{D,0} ----------------------------------
//
// D: 0 (init, NON-accepting in D -- matching the real pipeline precondition
// that past_ltlf_to_dfa's D never accepts the empty word, so this fixture
// stays comparable via the reversal-language check below), 1 (accepting),
// 2 (non-accepting sink); edges 0-a->1, 0-!a->2, 1-a->2, 1-!a->2, 2-a->2,
// 2-!a->2. NO D-edge targets state 0 (from anywhere, including itself) --
// so under reversal, s_{D,0} = N-state 0 gets ZERO real outgoing edges from
// the main reversal loop: exactly the "accepting dead-end s_{D,0}" shape
// flagged in the PRD dev comment (the analogous Phase-2 test-oracle bug
// surfaced on phi=X[!]G!p1's shape) -- note F_N is ALWAYS {s_{D,0}}
// regardless of whether s_{D,0} itself is in F_D (main.tex S160-169 does not
// special-case it). D-state 0's own out-edge targets the OTHER accepting
// state (1), so s_{N,0} (fresh init) still reaches N-state 0 via that edge --
// s_{D,0} is reachable but has no real successor, so it survives
// purge_unreachable_states/purge_dead_states only via the defensive
// bddfalse self-loop documented in reverse_dfa_to_nfa.cpp.
class ReverseDfaToNfaAcceptingDeadEnd : public ::testing::Test {
 protected:
  void SetUp() override {
    dict_ = spot::make_bdd_dict();
    spot::twa_graph_ptr registrar = spot::make_twa_graph(dict_);
    a_ = registrar->register_ap("a");
    // accepting = {1} only (state 0 -- s_{D,0} -- is NOT in F_D; see class
    // comment). No D-edge targets state 0 anywhere -- the accepting
    // dead-end shape.
    d_ = BuildDfa(dict_, {"a"}, /*num_states=*/3, /*init=*/0,
                  /*accepting=*/{1},
                  {
                      {0, bdd_ithvar(a_), 1},
                      {0, bdd_nithvar(a_), 2},
                      {1, bdd_ithvar(a_), 2},
                      {1, bdd_nithvar(a_), 2},
                      {2, bdd_ithvar(a_), 2},
                      {2, bdd_nithvar(a_), 2},
                  });
    n_ = reverse_dfa_to_nfa(d_);
  }

  spot::bdd_dict_ptr dict_;
  spot::twa_graph_ptr d_;
  spot::twa_graph_ptr n_;
  int a_ = -1;
};

TEST_F(ReverseDfaToNfaAcceptingDeadEnd, LanguageIsReverseOfDsLanguage) {
  // Same generic property as the other fixture, but now covering the
  // accepting-dead-end/self-loop-purge interaction: reverse_dfa_to_nfa must
  // still get the LANGUAGE right even though s_{D,0} ends up with only the
  // defensive bddfalse self-loop as its sole outgoing edge.
  for (const std::vector<bdd>& w : AllWordsOverA(a_, /*max_len=*/4)) {
    SCOPED_TRACE("word length " + std::to_string(w.size()));
    EXPECT_EQ(NfaAccepts(n_, w), DAccepts(d_, Reversed(w)))
        << "L(reverse_dfa_to_nfa(D)) != reverse(L(D)) on the accepting "
           "dead-end fixture";
  }
}

TEST_F(ReverseDfaToNfaAcceptingDeadEnd, AcceptsExactlyTheOneLetterAWord) {
  // Hand-derived from the fixture (see class comment): L(D) = {"a"} (the
  // single letter with a=true; nothing else, since state 2 is a
  // non-accepting sink and 0/1 only stay accepting for one more step at
  // most). reverse({"a"}) = {"a"} (a length-1 word reverses to itself), so
  // L(N) = {"a"} too -- stated as its own explicit, non-exhaustive assertion
  // for readability alongside the exhaustive sweep above.
  EXPECT_TRUE(NfaAccepts(n_, {bdd_ithvar(a_)}));
  EXPECT_FALSE(NfaAccepts(n_, {bdd_nithvar(a_)}));
  EXPECT_FALSE(NfaAccepts(n_, {bdd_ithvar(a_), bdd_ithvar(a_)}));
  EXPECT_FALSE(NfaAccepts(n_, {}));
}

TEST_F(ReverseDfaToNfaAcceptingDeadEnd, DeadEndAcceptingStateSurvivesPurge) {
  // The load-bearing fact itself: exactly one accepting state (s_{D,0})
  // survives purge_unreachable_states/purge_dead_states, and it is NOT
  // dropped just because it has no real successor (only the defensive
  // bddfalse self-loop) -- confirming the header doc-comment's account of
  // spot::twa_graph::purge_dead_states's "sole outgoing edge" exception.
  EXPECT_EQ(NumAcceptingStates(n_), 1u);
}

TEST_F(ReverseDfaToNfaAcceptingDeadEnd, NIsNotCompleted) {
  // PRD invariant 3: Reverse must not complete N. This fixture makes it
  // concrete -- the accepting dead-end state has no real (non-bddfalse)
  // outgoing edge at all, so N is certainly not complete.
  EXPECT_FALSE(spot::is_complete(n_));
}

// --- Small standalone structural checks -------------------------------------

// A trivial single-state D (init == accepting, self-loop) exercises the
// smallest possible reversal: N gets D's one state plus a fresh init, with
// F_N = {that one state} and a self-loop back onto itself. D's own init
// state being accepting (so ""/epsilon in L(D)) is deliberately used here to
// pin down invariant 4 (non-empty-trace semantics): main.tex S160-169 does
// NOT special-case F_N to also mark the fresh s_{N,0} accepting when
// s_{D,0} in F_D, so N still correctly rejects the empty word even though D
// itself accepts it -- confirmed structurally, not just inherited from the
// real pipeline's separate guarantee that past_ltlf_to_dfa's D never
// accepts epsilon in the first place.
TEST(ReverseDfaToNfa, SingleStateSelfLoopAcceptingDfaStillRejectsEmptyWord) {
  spot::bdd_dict_ptr dict = spot::make_bdd_dict();
  spot::twa_graph_ptr d = spot::make_twa_graph(dict);
  d->register_ap("a");
  d->set_buchi();
  d->prop_state_acc(true);
  d->new_states(1);
  d->set_init_state(0);
  d->new_edge(0, 0, bddtrue, spot::acc_cond::mark_t({0}));  // accepting={0}.

  const spot::twa_graph_ptr n = reverse_dfa_to_nfa(d);
  EXPECT_EQ(ApNames(n), ApNames(d));
  EXPECT_EQ(NumAcceptingStates(n), 1u);
  // s_{N,0} (fresh, distinct from D's single state) is never itself marked
  // accepting -- the empty word is rejected regardless of D's own F_D.
  EXPECT_FALSE(NfaAccepts(n, {}));
  // Every non-empty word reaches D's single (accepting) state, which is
  // F_N -- accepted at any positive length.
  EXPECT_TRUE(NfaAccepts(n, {bddtrue}));
  EXPECT_TRUE(NfaAccepts(n, {bddtrue, bddtrue, bddtrue}));
}

// Two APs: confirms the AP-copying loop (`for (ap : d->ap()) register_ap`)
// carries every AP, not just a single one, and in no particular assumed
// order.
TEST(ReverseDfaToNfa, AlphabetCarriesEveryApNotJustOne) {
  spot::bdd_dict_ptr dict = spot::make_bdd_dict();
  spot::twa_graph_ptr d = spot::make_twa_graph(dict);
  const int a = d->register_ap("a");
  const int b = d->register_ap("b");
  d->set_buchi();
  d->prop_state_acc(true);
  d->new_states(1);
  d->set_init_state(0);
  d->new_edge(0, 0, bdd_ithvar(a) | bdd_ithvar(b), spot::acc_cond::mark_t({0}));
  d->new_edge(0, 0, bdd_nithvar(a) & bdd_nithvar(b), spot::acc_cond::mark_t({0}));

  const spot::twa_graph_ptr n = reverse_dfa_to_nfa(d);
  EXPECT_EQ(ApNames(n), (std::set<std::string>{"a", "b"}));
}

// --- The conditional-vs-unconditional self-loop equivalence -----------------
//
// docs/prd/acceptance-mark-on-edgeless-states.md adopted
// detail::ensure_acceptance_readable here, which adds the bddfalse self-loop on
// s_{D,0} only when s_{D,0} is edgeless, replacing an UNCONDITIONAL
// new_edge(s0, s0, bddfalse, kFinal). The /code-reviewer pass asked whether
// that is really "the same final graph either way". These tests answer it by
// measurement rather than by argument: they rebuild the pre-adoption body
// verbatim and compare the two graphs edge for edge.
//
// The mechanism, from spot/twa/twagraph.cc: purge_dead_states() is a
// NO-SUCCESSOR purge, not a Buchi-liveness purge -- acceptance marks play no
// part in it, so a kFinal self-loop cannot keep a state alive by putting it on
// an "accepting cycle". Its documented exception keeps a bddfalse self-loop
// only when it is the state's FIRST edge with no next_succ, i.e. its sole
// outgoing edge. An unconditionally APPENDED self-loop is therefore erased
// whenever s_{D,0} already has a real out-edge -- exactly the case in which
// ensure_acceptance_readable declines to add it.

// The pre-adoption reverse_dfa_to_nfa body, verbatim except that the defensive
// self-loop is added unconditionally (as it was before the helper).
spot::twa_graph_ptr ReverseWithUnconditionalSelfLoop(
    const spot::twa_graph_ptr& d) {
  spot::twa_graph_ptr n = spot::make_twa_graph(d->get_dict());
  for (const spot::formula& ap : d->ap()) n->register_ap(ap.ap_name());
  n->set_buchi();
  n->prop_state_acc(true);
  const unsigned num_d_states = d->num_states();
  n->new_states(num_d_states + 1);
  const unsigned fresh_init = num_d_states;
  n->set_init_state(fresh_init);
  const spot::acc_cond::mark_t kFinal = {0};
  const spot::acc_cond::mark_t kNone = {};
  const unsigned s0 = d->get_init_state_number();
  for (unsigned s = 0; s < num_d_states; ++s)
    for (const auto& e : d->out(s)) {
      n->new_edge(e.dst, s, e.cond, e.dst == s0 ? kFinal : kNone);
      if (d->state_is_accepting(e.dst)) n->new_edge(fresh_init, s, e.cond, kNone);
    }
  n->new_edge(s0, s0, bddfalse, kFinal);
  n->purge_unreachable_states();
  n->purge_dead_states();
  return n;
}

// Structural equality of two twa_graphs: same states, same init, and the same
// (src, cond, dst, mark) edge multiset in the same per-state order.
::testing::AssertionResult SameGraph(const spot::twa_graph_ptr& x,
                                     const spot::twa_graph_ptr& y) {
  if (x->num_states() != y->num_states())
    return ::testing::AssertionFailure()
           << "state count " << x->num_states() << " != " << y->num_states();
  if (x->get_init_state_number() != y->get_init_state_number())
    return ::testing::AssertionFailure() << "different initial state";
  for (unsigned s = 0; s < x->num_states(); ++s) {
    auto xi = x->out(s).begin();
    auto yi = y->out(s).begin();
    for (;; ++xi, ++yi) {
      const bool xend = !(xi != x->out(s).end());
      const bool yend = !(yi != y->out(s).end());
      if (xend != yend)
        return ::testing::AssertionFailure()
               << "different out-degree at state " << s;
      if (xend) break;
      if (xi->dst != yi->dst || xi->cond != yi->cond || xi->acc != yi->acc)
        return ::testing::AssertionFailure() << "different edge at state " << s;
    }
  }
  return ::testing::AssertionSuccess();
}

// D with a state UNREACHABLE from s_{D,0} whose edge targets s_{D,0}: the one
// shape in which s_{D,0} gains a real out-edge in N without lying on any cycle,
// so it is where a Buchi-liveness reading would predict the two constructions to
// diverge. They do not.
TEST(ReverseDfaToNfaSelfLoopEquivalence, UnreachablePredecessorOfInitState) {
  spot::bdd_dict_ptr dict = spot::make_bdd_dict();
  spot::twa_graph_ptr registrar = spot::make_twa_graph(dict);
  const int a = registrar->register_ap("a");
  // 0 = s_{D,0}, 1 accepting, 2 unreachable from 0 with 2 --a--> 0.
  const std::vector<DEdge> edges = {
      {0, bdd_ithvar(a), 1},  {0, bdd_nithvar(a), 0},
      {1, bdd_ithvar(a), 1},  {1, bdd_nithvar(a), 0},
      {2, bdd_ithvar(a), 0},  {2, bdd_nithvar(a), 2},
  };
  EXPECT_TRUE(SameGraph(
      reverse_dfa_to_nfa(BuildDfa(dict, {"a"}, 3, 0, {1}, edges)),
      ReverseWithUnconditionalSelfLoop(BuildDfa(dict, {"a"}, 3, 0, {1}, edges))));
}

// Sharper: s_{D,0}'s sole N-out-edge leads to a state with no successors, so
// s_{D,0} is itself purged. The unconditional self-loop does NOT rescue it --
// which is the direct evidence that the purge is not liveness-based. (Both
// constructions lose the accepting state here; that is a genuine precondition
// on D -- all states reachable from s_{D,0} -- and it predates this PRD.)
TEST(ReverseDfaToNfaSelfLoopEquivalence, UnreachablePredecessorThatIsAlsoADeadEnd) {
  spot::bdd_dict_ptr dict = spot::make_bdd_dict();
  spot::twa_graph_ptr registrar = spot::make_twa_graph(dict);
  const int a = registrar->register_ap("a");
  const std::vector<DEdge> edges = {
      {0, bdd_ithvar(a), 1},
      {1, bdd_ithvar(a), 1},
      {2, bdd_ithvar(a), 0},  // 2 unreachable from 0, sole predecessor of 0
  };
  EXPECT_TRUE(SameGraph(
      reverse_dfa_to_nfa(BuildDfa(dict, {"a"}, 3, 0, {1}, edges)),
      ReverseWithUnconditionalSelfLoop(BuildDfa(dict, {"a"}, 3, 0, {1}, edges))));
}

// And the two shapes the production pipeline actually produces: s_{D,0} with
// real out-edges (fixture 1) and the accepting dead-end (fixture 2).
TEST(ReverseDfaToNfaSelfLoopEquivalence, ProductionShapes) {
  spot::bdd_dict_ptr dict = spot::make_bdd_dict();
  spot::twa_graph_ptr registrar = spot::make_twa_graph(dict);
  const int a = registrar->register_ap("a");
  const std::vector<DEdge> last_letter = {
      {0, bdd_ithvar(a), 1},
      {0, bdd_nithvar(a), 0},
      {1, bdd_ithvar(a), 1},
      {1, bdd_nithvar(a), 0},
  };
  EXPECT_TRUE(SameGraph(
      reverse_dfa_to_nfa(BuildDfa(dict, {"a"}, 2, 0, {1}, last_letter)),
      ReverseWithUnconditionalSelfLoop(
          BuildDfa(dict, {"a"}, 2, 0, {1}, last_letter))));

  const std::vector<DEdge> dead_end = {
      {0, bdd_ithvar(a), 1}, {0, bdd_nithvar(a), 2}, {1, bdd_ithvar(a), 2},
      {1, bdd_nithvar(a), 2}, {2, bdd_ithvar(a), 2}, {2, bdd_nithvar(a), 2},
  };
  EXPECT_TRUE(SameGraph(
      reverse_dfa_to_nfa(BuildDfa(dict, {"a"}, 3, 0, {1}, dead_end)),
      ReverseWithUnconditionalSelfLoop(
          BuildDfa(dict, {"a"}, 3, 0, {1}, dead_end))));
}

}  // namespace
