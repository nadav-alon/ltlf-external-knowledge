#include <map>
#include <sstream>
#include <string>
#include <vector>

#include <gtest/gtest.h>
#include <bddx.h>
#include <spot/tl/parse.hh>
#include <spot/twa/bdddict.hh>
#include <spot/twa/twagraph.hh>

#include "ltlf_ek/ltlf_to_dfa.hpp"
#include "ltlf_ek/output_labeled_transducer.hpp"
#include "ltlf_ek/product.hpp"
#include "ltlf_ek/role.hpp"
#include "ltlf_ek/transducer.hpp"
#include "ltlf_ek/variables.hpp"

// Build-equivalence metamorphic oracle, dedicated unit fixtures
// (docs/prd/symbolic-dfa-product.md "Test oracles" #2, the linchpin for the
// symbolic-DFA-product rewrite): for the same (goal, taus, init),
// build_product_symbolic(...) must equal to_guard_map(build_product(...),
// alphabet) --- identical reachable ProductState set, identical acc flag per
// state, and a BDD-equal (==, BuDDy canonicalises so this is semantic
// equality) guard per <src, dst>.  This is a metamorphic oracle: the
// per-letter build supplies its own ground truth, so no fixture below needs a
// hand-computed expected ProductGuards --- only a spread of non-trivial
// (goal, taus) shapes past the trivial-input blind spot (PRD "Push the
// oracles past trivial inputs"): partial delta/lambda (skip-not-sink, the
// per-letter analogue already covered by dfa_product_test.cpp's skip-not-sink
// fixtures), empty V, a zero-transducer product, and a wider partition with
// several Iknown vars and a deeper mixed-operator Goal formula.
//
// The wire-into-the-generated-corpus half of this oracle lives in
// tests/ltlfsynt_oracle_test.cpp (GeneratedCorpus.BuildEquivalence), run over
// every (phi, partition, Tin) case as a cheap, library-only, self-labeling
// body alongside the existing corpus bodies.
namespace {

using ltlf_ek::build_product;
using ltlf_ek::build_product_symbolic;
using ltlf_ek::LetterAlphabet;
using ltlf_ek::OutputLabeledTransducer;
using ltlf_ek::ProductGuards;
using ltlf_ek::ProductState;
using ltlf_ek::Role;
using ltlf_ek::to_guard_map;
using ltlf_ek::Transducer;
using ltlf_ek::trivial_transducer;
using ltlf_ek::VariablePartition;

// --- Shared oracle machinery -------------------------------------------

std::string DescribeProductState(const ProductState& s) {
  std::ostringstream os;
  os << "<goal=" << s.goal << ", taus=[";
  for (std::size_t i = 0; i < s.taus.size(); ++i) {
    if (i) os << ",";
    os << s.taus[i];
  }
  os << "]>";
  return os.str();
}

// The oracle's actual comparison (PRD "Test oracles" #2): identical
// reachable-state sets, identical acc flags, BDD-equal guards per <src,dst>.
// Both maps compare in the same order (ProductState::operator< on both
// sides), so a size match plus a one-directional lookup (reference -> found
// in symbolic) is a genuine set-equality check, not a false positive on
// extras --- mirrored per-destination below.
void ExpectProductGuardsEqual(const ProductGuards& symbolic,
                              const ProductGuards& reference) {
  ASSERT_EQ(symbolic.nodes.size(), reference.nodes.size())
      << "different number of reachable ProductStates between the symbolic "
         "and per-letter (reference) builds";
  for (const auto& [state, ref_entry] : reference.nodes) {
    SCOPED_TRACE("state " + DescribeProductState(state));
    ASSERT_TRUE(symbolic.nodes.count(state))
        << "symbolic build is missing a ProductState the reference build "
           "reached";
    const auto& [ref_acc, ref_guards] = ref_entry;
    const auto& [sym_acc, sym_guards] = symbolic.nodes.at(state);
    EXPECT_EQ(sym_acc, ref_acc) << "acc flag differs";
    ASSERT_EQ(sym_guards.size(), ref_guards.size())
        << "different number of outgoing destinations from this state";
    for (const auto& [dst, ref_guard] : ref_guards) {
      SCOPED_TRACE("dst " + DescribeProductState(dst));
      ASSERT_TRUE(sym_guards.count(dst))
          << "symbolic build is missing an edge to this destination";
      EXPECT_EQ(sym_guards.at(dst), ref_guard)
          << "guard BDD differs (BuDDy canonicalises, so == is semantic "
             "equality)";
    }
  }
}

ProductState MakeInit(const spot::twa_graph_ptr& goal,
                      const std::vector<const Transducer*>& taus) {
  std::vector<unsigned> tau_states;
  tau_states.reserve(taus.size());
  for (const Transducer* t : taus) tau_states.push_back(t->initial_state());
  return ProductState{goal->get_init_state_number(), std::move(tau_states)};
}

// Runs both builds on (phi, vars, taus, dict) --- goal = ltlf_to_dfa(phi,
// dict), the exact construction DfaProduct::synthesize uses (src/dfa_product.
// cpp), so invariant 3 (Goal completeness) always holds without a
// hand-crafted goal automaton --- and asserts they agree.
void ExpectBuildsAgree(const std::string& phi_str, const VariablePartition& vars,
                       const std::vector<const Transducer*>& taus,
                       const spot::bdd_dict_ptr& dict) {
  SCOPED_TRACE("phi = " + phi_str);
  const spot::formula phi = spot::parse_formula(phi_str);
  const spot::twa_graph_ptr goal = ltlf_ek::ltlf_to_dfa(phi, dict);
  const ProductState init = MakeInit(goal, taus);

  const LetterAlphabet alphabet(vars, goal);
  const ProductGuards symbolic = build_product_symbolic(goal, taus, init);
  const std::map<ProductState, ltlf_ek::ProductNode> graph =
      build_product(goal, taus, init, alphabet, /*goal_must_be_complete=*/true);
  const ProductGuards reference = to_guard_map(graph, alphabet);

  ExpectProductGuardsEqual(symbolic, reference);
}

// --- Fixture 1: empty V (trivial transducers), a deeper mixed formula ------
//
// PRD "Edge cases": "Empty V ... trivial transducers: emits_region is
// bddtrue, delta_edges a single self-loop; P collapses to A".  Pushed past a
// depth-<=2, two-operator formula (PRD "Push the oracles past trivial
// inputs"): nesting depth 3, mixed X[!]/U/F/G.

TEST(ProductBuildEquivalence, EmptyKnowledgeTrivialTransducersDeeperFormula) {
  auto dict = spot::make_bdd_dict();
  auto vars = VariablePartition::split({"i"}, {"o"}, /*governed=*/{});
  auto t_in = trivial_transducer(vars, Role::t_in, dict);
  auto t_out = trivial_transducer(vars, Role::t_out, dict);
  const std::vector<const Transducer*> taus{&t_in, &t_out};

  ExpectBuildsAgree("G(i -> X[!] o) & F(o U i) & (o R i)", vars, taus, dict);
}

// --- Fixture 2: zero transducers ---------------------------------------
//
// The N=0 boundary of combine_taus's recursion (i == effective_edges.size()
// == 0 immediately) --- P collapses to the Goal automaton alone, mirroring
// product_test.cpp's BuildProduct.EmptyTausProductIsTheGoalAutomatonAlone but
// through the symbolic path.

TEST(ProductBuildEquivalence, ZeroTransducersProductIsGoalAlone) {
  auto dict = spot::make_bdd_dict();
  auto vars = VariablePartition::split({"i"}, {"o"}, /*governed=*/{});
  const std::vector<const Transducer*> taus;  // empty.

  ExpectBuildsAgree("G(i -> o) & F(o)", vars, taus, dict);
}

// --- Fixture 3: partial delta AND partial lambda (skip-not-sink) -----------
//
// The likeliest bug class this PRD calls out ("lost/mis-grouped
// transitions"): a transducer whose delta is undefined at some state (no
// delta_edges guard covers a letter there) and whose lambda is undefined at
// another (emits_region == bddfalse there), paired with a real Goal DFA ---
// dfa_product_test.cpp's skip-not-sink fixtures are the per-letter-level
// reference for this shape.

struct PartialVars {
  int a, b, c;
};

// delta: s0 --a--> s1, s0 --!a--> s0, s1 --a--> s2, s2 (no edges: undefined).
// lambda: state 0 = (a<->b), state 1 = b, state 2 = bddfalse (undefined).
OutputLabeledTransducer MakePartialFixture(PartialVars* v,
                                           const spot::bdd_dict_ptr& dict) {
  auto aut = spot::make_twa_graph(dict);
  v->a = aut->register_ap("a");
  v->b = aut->register_ap("b");
  v->c = aut->register_ap("c");
  const bdd av = bdd_ithvar(v->a);
  const bdd bv = bdd_ithvar(v->b);

  aut->new_states(3);
  aut->set_init_state(0);
  aut->new_edge(0, 1, av);
  aut->new_edge(0, 0, bdd_nithvar(v->a));
  aut->new_edge(1, 2, av);
  // state 2: no outgoing edges -> delta always undefined there.

  std::vector<bdd> lambda_by_state = {
      bdd_biimp(av, bv),
      bv,
      bddfalse,
  };
  return OutputLabeledTransducer(aut, std::move(lambda_by_state), av, bv);
}

TEST(ProductBuildEquivalence, PartialDeltaAndLambdaTransducerSkipsNotSinks) {
  auto dict = spot::make_bdd_dict();
  PartialVars pv;
  OutputLabeledTransducer t_partial = MakePartialFixture(&pv, dict);
  auto vars = VariablePartition::split({"a", "c"}, {"b"}, /*governed=*/{});
  auto t_out = trivial_transducer(vars, Role::t_out, dict);
  const std::vector<const Transducer*> taus{&t_partial, &t_out};

  ExpectBuildsAgree("G(a -> X[!] b) | F(c)", vars, taus, dict);
}

// --- Fixture 4: wider partition, several Iknown vars, deeper formula -------
//
// PRD "Push the oracles past trivial inputs": "wider partitions/deeper
// formulas" and "multiple known inputs, exercised Oknown, empty Ofree, larger
// |I∪O|" (generalised to |I∪O| here since Oknown itself stays empty per this
// PRD's scope, matching src/dfa_product.cpp's own Tout usage). I = {a,b} free,
// {k1,k2} known; O = {o1,o2} free. t_in is a single-state, totally-defined
// function of (a,b) committing (k1,k2), so lambda's region argument is
// genuinely non-trivial (not a constant, unlike TinAlwaysI).

TEST(ProductBuildEquivalence, WiderPartitionMultipleKnownInputsDeeperFormula) {
  auto dict = spot::make_bdd_dict();
  auto g = spot::make_twa_graph(dict);
  const int av = g->register_ap("a");
  const int bv = g->register_ap("b");
  const int k1v = g->register_ap("k1");
  const int k2v = g->register_ap("k2");
  g->register_ap("o1");
  g->register_ap("o2");
  g->new_states(1);
  g->set_init_state(0);
  g->new_edge(0, 0, bddtrue);  // delta total: single self-loop.

  // k1 <-> (a & !b), k2 <-> !a --- a genuine (non-constant) function of a, b.
  const bdd sigma0 = bdd_ithvar(av) & bdd_ithvar(bv);
  const bdd sigma1 = bdd_ithvar(k1v) & bdd_ithvar(k2v);
  const bdd lambda0 = bdd_biimp(bdd_ithvar(k1v), bdd_ithvar(av) & !bdd_ithvar(bv)) &
                      bdd_biimp(bdd_ithvar(k2v), !bdd_ithvar(av));
  OutputLabeledTransducer t_in(g, {lambda0}, sigma0, sigma1);

  auto vars = VariablePartition::split({"a", "b", "k1", "k2"}, {"o1", "o2"},
                                       /*governed=*/{"k1", "k2"});
  auto t_out = trivial_transducer(vars, Role::t_out, dict);
  const std::vector<const Transducer*> taus{&t_in, &t_out};

  ExpectBuildsAgree(
      "G(k1 -> X[!] o1) & (o2 U k2) & F(a & b) & G(o1 | o2 | !k1)", vars,
      taus, dict);
}

}  // namespace
