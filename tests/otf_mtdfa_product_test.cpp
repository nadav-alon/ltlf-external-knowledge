#include <algorithm>
#include <cstdint>
#include <deque>
#include <map>
#include <memory>
#include <optional>
#include <random>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

#include <bddx.h>
#include <gtest/gtest.h>
#include <spot/misc/optionmap.hh>
#include <spot/tl/defaultenv.hh>
#include <spot/tl/formula.hh>
#include <spot/tl/parse.hh>
#include <spot/tl/randomltl.hh>
#include <spot/twa/bdddict.hh>
#include <spot/twa/twagraph.hh>
#include <spot/twaalgos/ltlf2dfa.hh>

#include "ltlf_ek/bench.hpp"
#include "ltlf_ek/cli.hpp"
#include "ltlf_ek/otf_mtdfa_product.hpp"
#include "ltlf_ek/output_labeled_transducer.hpp"
#include "ltlf_ek/progression.hpp"
#include "ltlf_ek/role.hpp"
#include "ltlf_ek/synthesis.hpp"
#include "ltlf_ek/transducer.hpp"
#include "ltlf_ek/turn_order.hpp"
#include "ltlf_ek/variables.hpp"

#include "support/fixtures.hpp"

// Full suite for docs/prd/otf-mtdfa-product.md PHASE 1 ONLY: ForwardProgression
// (progression.hpp/.cpp) and otf_product_to_mtdfa / OtfMtdfaProduct
// (otf_mtdfa_product.hpp/.cpp).  SEQUENTIAL workflow: both landed already
// (uncommitted on master), so this file compiles and runs against the real
// code today, unlike the concurrent-workflow test files elsewhere in this
// project.
//
// Phase 2 (otf_solve_fused, --otf-solve) does not exist and is OUT OF SCOPE
// here; see OtfMtdfaProduct(true) below for its Phase-1 stub behaviour
// (throws std::logic_error), which IS in scope.
//
// The cross-method-vs-{MtdfaProduct,DfaProduct,NfaProduct} generated-corpus
// oracle, the synthesize->verify_controller metamorphic round-trip, and the
// --otf-mtdfa-product vs ltlfsynt corpus differential all live in
// tests/ltlfsynt_oracle_test.cpp instead (the "generated corpus" -- run_corpus
// / GeneratedCase -- is defined there, and MtdfaProduct's own analogous
// extensions already live there rather than in tests/mtdfa_product_test.cpp;
// this file follows that precedent rather than re-implementing run_corpus).
namespace {

using ltlf_ek::Controller;
using ltlf_ek::ForwardProgression;
using ltlf_ek::make_synthesis_method;
using ltlf_ek::OtfMtdfaProduct;
using ltlf_ek::otf_product_to_mtdfa;
using ltlf_ek::OutputLabeledTransducer;
using ltlf_ek::register_turn_order_aps;
using ltlf_ek::Role;
using ltlf_ek::BenchReport;
using ltlf_ek::BenchScope;
using ltlf_ek::BenchTimer;
using ltlf_ek::Stage;
using ltlf_ek::stage_name;
using ltlf_ek::Synthesis;
using ltlf_ek::Transducer;
using ltlf_ek::trivial_transducer;
using ltlf_ek::VariablePartition;

using ltlf_ek::test_support::IoFreeVars;
using ltlf_ek::test_support::Phi;

std::string FormulaStr(const spot::formula& phi) {
  std::ostringstream os;
  os << phi;
  return os.str();
}

// ---------------------------------------------------------------------------
// SECTION A -- ForwardProgression unit fixtures (PRD "Test oracles" "Unit --
// ForwardProgression"): the five per-letter fixtures taken from the probe run
// while writing the PRD, "already verified against the linked libspot".  Each
// derives the per-letter form exactly as progression.hpp documents it must be
// derived: decode(bdd_get_terminal(bdd_restrict(progress_row(psi), v))), with
// the two constant leaves (bddfalse/bddtrue) handled by the caller first --
// there is no per-letter API to call directly.
// ---------------------------------------------------------------------------

TEST(ForwardProgressionUnit, WeakDisjunctionIsSatisfiedOutrightWhenTheEagerDisjunctHolds) {
  // FP(b | Xc, {b}) -> bddtrue: the "b" disjunct is satisfied at this letter
  // regardless of Xc, so progression collapses to the accepting sink (I5)
  // without ever branching on 'c'.
  const spot::bdd_dict_ptr dict = spot::make_bdd_dict();
  auto registrar = spot::make_twa_graph(dict);
  const int b = registrar->register_ap("b");
  registrar->register_ap("c");
  ForwardProgression fp(dict);

  const bdd row = fp.progress_row(Phi("b | Xc"));
  const bdd leaf = bdd_restrict(row, bdd_ithvar(b));
  EXPECT_EQ(leaf, bddtrue);
}

TEST(ForwardProgressionUnit, BarePropositionIsViolatedOutrightWhenFalseAtThisLetter) {
  // FP(a, {!a}) -> bddfalse: a plain proposition is a one-step check, no
  // deferred obligation, so a=false kills it immediately (I5's rejecting
  // sink), never minting a terminal.
  const spot::bdd_dict_ptr dict = spot::make_bdd_dict();
  auto registrar = spot::make_twa_graph(dict);
  const int a = registrar->register_ap("a");
  ForwardProgression fp(dict);

  const bdd row = fp.progress_row(Phi("a"));
  const bdd leaf = bdd_restrict(row, bdd_nithvar(a));
  EXPECT_EQ(leaf, bddfalse);
}

TEST(ForwardProgressionUnit, WeakNextDefersToBAtTheNextPositionRegardlessOfCurrentB) {
  // FP(Xb, {!b}) -> (b, TRUE): the committed weak-X reading (memory
  // ltlf-weak-x-and-termination-semantics) -- Xb progresses to a genuine
  // terminal (not a constant sink), decoding to psi'=b with the acceptance
  // bit TRUE (stopping right after this letter satisfies weak Xb vacuously).
  const spot::bdd_dict_ptr dict = spot::make_bdd_dict();
  auto registrar = spot::make_twa_graph(dict);
  const int b = registrar->register_ap("b");
  ForwardProgression fp(dict);

  const bdd row = fp.progress_row(Phi("Xb"));
  const bdd leaf = bdd_restrict(row, bdd_nithvar(b));
  ASSERT_TRUE(bdd_is_terminal(leaf))
      << "Xb must progress to a genuine terminal, not a constant sink";
  const auto [psi_prime, bit] = fp.decode(bdd_get_terminal(leaf));
  EXPECT_EQ(psi_prime, Phi("b"));
  EXPECT_TRUE(bit);
}

// The I4 discriminating pair (PRD "Behaviour" I4, "Test oracles"): SAME
// successor formula, DIFFERENT acceptance bit, depending on whether the
// PARENT formula was already inside the G or was one weak-next step away
// from it.  This is the pair that a regression to state-KEYED acceptance
// (alg:otfdfa_product:final_insert, the deviation this project's mtdfa
// terminal encoding avoids) could not tell apart -- assert exactly this, so
// such a regression cannot pass.
TEST(ForwardProgressionUnit, DiscriminatingPairSameSuccessorDifferentAcceptanceBit) {
  const spot::bdd_dict_ptr dict = spot::make_bdd_dict();
  auto registrar = spot::make_twa_graph(dict);
  const int a = registrar->register_ap("a");
  const int b = registrar->register_ap("b");
  ForwardProgression fp(dict);
  const bdd letter = bdd_nithvar(a) & bdd_nithvar(b);

  // FP(G(a -> Xb), {!a,!b}) -> (G(a -> Xb), TRUE): a=false vacuously
  // discharges the implication this step, so G's own obligation holds NOW
  // and the formula self-loops with an ACCEPTING transition.
  const bdd row_g = fp.progress_row(Phi("G(a -> Xb)"));
  const bdd leaf_g = bdd_restrict(row_g, letter);
  ASSERT_TRUE(bdd_is_terminal(leaf_g));
  const auto [psi_g, bit_g] = fp.decode(bdd_get_terminal(leaf_g));
  EXPECT_EQ(psi_g, Phi("G(a -> Xb)"));
  EXPECT_TRUE(bit_g);

  // FP(X[!] G(a -> Xb), {!a,!b}) -> (G(a -> Xb), FALSE): X[!] defers
  // entirely to the next position (letter-independent), so this SAME letter
  // decodes to the SAME successor formula, but the transition itself is NOT
  // accepting -- stopping right here does not satisfy "start checking
  // G(a->Xb) one step from now".
  const bdd row_xg = fp.progress_row(Phi("X[!] G(a -> Xb)"));
  const bdd leaf_xg = bdd_restrict(row_xg, letter);
  ASSERT_TRUE(bdd_is_terminal(leaf_xg));
  const auto [psi_xg, bit_xg] = fp.decode(bdd_get_terminal(leaf_xg));
  EXPECT_EQ(psi_xg, psi_g)
      << "I4: the two source formulas must progress to the IDENTICAL "
        "successor under this letter -- that is what makes the acceptance "
        "bit the only discriminator";
  EXPECT_FALSE(bit_xg);
}

// ---------------------------------------------------------------------------
// SECTION B -- Isolated oracle (PRD "Test oracles": "drive progress_row alone
// in a plain worklist to rebuild the whole Goal MTDFA of phi (no transducers,
// no cons), and assert language-equivalence with spot::ltlf_to_mtdfa(phi,
// dict)").  A from-scratch BFS over ForwardProgression ONLY -- deliberately
// NOT calling otf_product_to_mtdfa (that would not isolate a progression bug
// from a product bug) -- narrower than but structurally the same shape as
// otf_product_to_mtdfa's own BFS (a single degenerate "combination" covering
// the whole alphabet, no taus dimension in the Key).
// ---------------------------------------------------------------------------

// Key holds a LIVE bdd handle (not a bare int id), same reason
// src/otf_mtdfa_product.cpp's own Key does: BuDDy recycles a node id once its
// last handle is released (docs/prd/mtnfa.md F4), so keying on a bare int
// while letting the underlying handle die would be unsound, not merely
// non-canonical. bdd's own operator< is a BDD-implication operator, not a
// total order, hence the explicit comparator on .id().
struct IsolatedKey {
  bdd row;
};

bool operator<(const IsolatedKey& x, const IsolatedKey& y) {
  return x.row.id() < y.row.id();
}

bdd RelabelIsolated(const bdd& node, ForwardProgression& fp,
                    std::map<IsolatedKey, unsigned>& state_index,
                    std::deque<IsolatedKey>& pending,
                    std::unordered_map<int, bdd>& memo) {
  if (auto it = memo.find(node.id()); it != memo.end()) return it->second;

  bdd result;
  if (node == bddfalse) {
    result = bddfalse;
  } else if (node == bddtrue) {
    result = bddtrue;
  } else if (bdd_is_terminal(node)) {
    const auto [psi_prime, bit] = fp.decode(bdd_get_terminal(node));
    const IsolatedKey key{fp.progress_row(psi_prime)};
    unsigned j;
    if (auto found = state_index.find(key); found != state_index.end()) {
      j = found->second;
    } else {
      j = static_cast<unsigned>(state_index.size());
      state_index.emplace(key, j);
      pending.push_back(key);
    }
    result = bdd_terminalpp(static_cast<int>(2 * j + (bit ? 1 : 0)));
  } else {
    const int v = bdd_var(node);
    result = bdd_ite(bdd_ithvarpp(v),
                     RelabelIsolated(bdd_high(node), fp, state_index, pending, memo),
                     RelabelIsolated(bdd_low(node), fp, state_index, pending, memo));
  }
  memo.emplace(node.id(), result);
  return result;
}

// Rebuilds the whole Goal MTDFA of `phi` from ForwardProgression alone --
// register phi's own APs on `out` first (mirrors otf_product_to_mtdfa's own
// AP-ownership step, so `out` outlives the local ForwardProgression's
// translator, whose destructor unregisters ITS registrations -- the
// AP-lifetime hazard mtnfa_product_test.cpp SECTION D exercises for the real
// product function).
spot::mtdfa_ptr BuildViaForwardProgressionOnly(const spot::formula& phi,
                                               const spot::bdd_dict_ptr& dict) {
  auto out = std::make_shared<spot::mtdfa>(dict);
  for (const std::string& name : ltlf_ek::collect_aps(phi))
    out->aps.push_back(spot::formula::ap(name));
  std::sort(out->aps.begin(), out->aps.end());
  for (const spot::formula& ap : out->aps) dict->register_proposition(ap, out.get());

  ForwardProgression fp(dict);
  std::map<IsolatedKey, unsigned> state_index;
  std::deque<IsolatedKey> pending;
  const IsolatedKey k0{fp.progress_row(phi)};
  state_index.emplace(k0, 0u);
  pending.push_back(k0);

  while (!pending.empty()) {
    const IsolatedKey key = pending.front();
    pending.pop_front();
    std::unordered_map<int, bdd> memo;
    out->states.push_back(RelabelIsolated(key.row, fp, state_index, pending, memo));
  }
  return out;
}

spot::formula GenerateRandomFormula(const std::set<std::string>& ap_names,
                                    std::mt19937& rng, int tree_size_max) {
  spot::atomic_prop_set aprops;
  for (const std::string& name : ap_names)
    aprops.insert(spot::default_environment::instance().require(name));

  spot::option_map opts;
  opts.set("output", spot::randltlgenerator::LTL);
  opts.set("tree_size_min", 1);
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

constexpr unsigned kIsolatedCorpusSeed = 20260728;
constexpr std::size_t kIsolatedCorpusCaseCount = 12;
constexpr int kIsolatedTreeSizeMax = 8;

// Needs no mona: ForwardProgression is spot::ltlf_translator directly (no
// determinization/NFA route), same as spot::ltlf_to_mtdfa itself.
TEST(ForwardProgressionIsolatedOracle, ProgressionOnlyBuildAgreesWithSpotLtlfToMtdfa) {
  const std::vector<std::string> pool{"p", "q", "r"};
  std::mt19937 rng(kIsolatedCorpusSeed);
  std::bernoulli_distribution incl(0.6);
  for (std::size_t case_idx = 0; case_idx < kIsolatedCorpusCaseCount; ++case_idx) {
    std::set<std::string> ap_names;
    while (ap_names.empty())
      for (const std::string& name : pool)
        if (incl(rng)) ap_names.insert(name);
    const spot::formula phi = GenerateRandomFormula(ap_names, rng, kIsolatedTreeSizeMax);
    SCOPED_TRACE("case " + std::to_string(case_idx) + ": phi=" + FormulaStr(phi));

    const spot::bdd_dict_ptr dict = spot::make_bdd_dict();
    const spot::mtdfa_ptr got = BuildViaForwardProgressionOnly(phi, dict);
    ASSERT_NE(got, nullptr);
    const spot::mtdfa_ptr want = spot::ltlf_to_mtdfa(phi, dict);

    EXPECT_TRUE(spot::product_xor(got, want)->is_empty())
        << "progression-only build disagrees with spot::ltlf_to_mtdfa for phi="
        << FormulaStr(phi);
  }
}

// Required negative control (mtdfa-product Phase-0/Q1 lesson): a
// G(a)-vs-F(a) mismatch fed through the SAME comparison must be non-empty,
// proving the oracle actually discriminates.
TEST(ForwardProgressionIsolatedOracle, NegativeControlDetectsMismatchedGvsFFormulas) {
  const spot::bdd_dict_ptr dict = spot::make_bdd_dict();
  const spot::formula g_a = Phi("G(a)");
  const spot::formula f_a = Phi("F(a)");

  const spot::mtdfa_ptr got = BuildViaForwardProgressionOnly(g_a, dict);
  const spot::mtdfa_ptr want = spot::ltlf_to_mtdfa(f_a, dict);

  EXPECT_FALSE(spot::product_xor(got, want)->is_empty())
      << "negative control: G(a) and F(a) denote different languages; a "
        "vacuous (empty) product_xor here means the oracle cannot "
        "discriminate";
}

// ---------------------------------------------------------------------------
// SECTION C -- otf_product_to_mtdfa unit fixtures (PRD "Test oracles" "Unit --
// otf_product_to_mtdfa"): state 0 initial, out->aps == vars.universe()
// sorted, a bddfalse row for a cons-dead state, the overlapping-delta_edges
// throw, and a fusing assertion.  No turn order needed: otf_product_to_mtdfa
// itself does NOT check the Turn order contract (PRD "Interfaces & types"),
// so these call the free function directly, mirroring
// tests/mtnfa_product_test.cpp SECTION A's own choice not to register it.
// ---------------------------------------------------------------------------

// A spot::mtdfa row's leaf CAN be the literal bddfalse/bddtrue sink; the
// descent must check identity against both explicitly (duplicated from
// tests/mtnfa_product_test.cpp per this project's one-file-per-suite
// duplication norm).
bdd DescendMtdfaRow(bdd node, const bdd& letter) {
  while (!bdd_is_terminal(node) && node != bddfalse && node != bddtrue) {
    const int v = bdd_var(node);
    node = ((letter & bdd_ithvar(v)) != bddfalse) ? bdd_high(node) : bdd_low(node);
  }
  return node;
}

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

TEST(OtfProductToMtdfa, NeverReturnsNullptrAndStateZeroIsInitial) {
  const spot::bdd_dict_ptr dict = spot::make_bdd_dict();
  const VariablePartition vars = IoFreeVars();
  const OutputLabeledTransducer t_in = trivial_transducer(vars, Role::t_in, dict);
  const OutputLabeledTransducer t_out = trivial_transducer(vars, Role::t_out, dict);
  const std::vector<const Transducer*> taus{&t_in, &t_out};

  const spot::mtdfa_ptr d = otf_product_to_mtdfa(Phi("G(i -> o)"), taus, vars, dict);
  ASSERT_NE(d, nullptr);
  ASSERT_FALSE(d->states.empty());
  // Index 0 must be the initial state (PRD step 1): with trivial taus and a
  // realizable goal, the initial state must not itself be the dead sink.
  EXPECT_NE(d->states[0], bddfalse);
}

TEST(OtfProductToMtdfa, ApsEqualVarsUniverseSortedByFormulaId) {
  const spot::bdd_dict_ptr dict = spot::make_bdd_dict();
  // No governed variables here (trivial_transducer requires an empty known
  // set for its role) -- irrelevant to what this test checks, since it is
  // purely about out->aps versus vars.universe(), not about known-input
  // semantics; "k" just widens the universe past phi's own bare support.
  const VariablePartition vars =
      VariablePartition::split({"i", "k"}, {"o"}, /*governed=*/{});
  const OutputLabeledTransducer t_in = trivial_transducer(vars, Role::t_in, dict);
  const OutputLabeledTransducer t_out = trivial_transducer(vars, Role::t_out, dict);
  const std::vector<const Transducer*> taus{&t_in, &t_out};

  const spot::mtdfa_ptr d = otf_product_to_mtdfa(Phi("G(i)"), taus, vars, dict);
  ASSERT_NE(d, nullptr);
  EXPECT_EQ(ApNameSet(d->aps), vars.universe());
  std::vector<spot::formula> sorted_aps = d->aps;
  std::sort(sorted_aps.begin(), sorted_aps.end());
  EXPECT_EQ(d->aps, sorted_aps) << "out->aps must be sorted by formula id";
}

// Cons dead at the initial state (PRD "Edge cases"): lambda undefined
// EVERYWHERE (bddfalse relation) => emits_region == bddfalse identically =>
// state 0's row must be EXACTLY bddfalse, not merely non-accepting (the
// "cons == bddfalse: push it and continue" shortcut).
TEST(OtfProductToMtdfa, ConsDeadAtInitialYieldsBddfalseInitialStateAndEmptyLanguage) {
  const spot::bdd_dict_ptr dict = spot::make_bdd_dict();
  const VariablePartition vars =
      VariablePartition::split({"x"}, {}, /*governed=*/{"y"});
  auto tin_g = spot::make_twa_graph(dict);
  const int x = tin_g->register_ap("x");
  const int y = tin_g->register_ap("y");
  (void)y;
  tin_g->new_states(1);
  tin_g->set_init_state(0);
  tin_g->new_edge(0, 0, bddtrue);
  // lambda undefined everywhere (bddfalse relation) => emits_region ==
  // bddfalse identically, regardless of x.
  const OutputLabeledTransducer t_in(tin_g, {bddfalse}, /*sigma0=*/bdd_ithvar(x),
                                     /*sigma1=*/bdd_ithvar(y));
  const OutputLabeledTransducer t_out = trivial_transducer(vars, Role::t_out, dict);
  const std::vector<const Transducer*> taus{&t_in, &t_out};

  const spot::mtdfa_ptr d = otf_product_to_mtdfa(Phi("G(x)"), taus, vars, dict);
  ASSERT_NE(d, nullptr);
  ASSERT_FALSE(d->states.empty());
  EXPECT_EQ(d->states[0], bddfalse);
  EXPECT_TRUE(d->is_empty());
}

// The (d) disjointness check rejects a NONDETERMINISTIC transducer -- a
// THROW, not an assert, so it fires in release builds too (Transducer is a
// public virtual interface; under NDEBUG an assert would let a violating
// subclass through with a silently wrong language, mirroring
// mtnfa_product_to_mtdfa / build_product_symbolic's own check).
TEST(OtfProductToMtdfa, OverlappingDeltaEdgeGuardsThrowInsteadOfCorruptingTheLanguage) {
  const auto build_and_run = [] {
    const spot::bdd_dict_ptr dict = spot::make_bdd_dict();
    const VariablePartition vars = VariablePartition::split({"a", "b"}, {}, {});

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

    const std::vector<const Transducer*> taus{&t_in, &t_out};
    (void)otf_product_to_mtdfa(Phi("a"), taus, vars, dict);
  };
  EXPECT_THROW(build_and_run(), std::runtime_error);
}

// ---------------------------------------------------------------------------
// SECTION C2 -- the product-state fusing assertion (PRD "Test oracles": "two
// goal formulas with equal rows and equal transducer states occupy ONE
// product state") AND the determinism assertion (PRD "Behaviour" /
// "Implementation phases" "Determinism": "the same (phi, vars, t_in, t_out)
// on the same bdd_dict must produce a byte-identical spot::mtdfa across
// runs -- assert this rather than assuming it").
//
// Both need an out-degree > 1 transducer (a single-combination fixture says
// nothing about iteration order or genuine interning, per the
// docs/prd/mtnfa-product.md D3 lesson mtnfa_product_test.cpp SECTION A2
// already applied to mtnfa_product_to_mtdfa), so ONE shared diamond fixture
// serves both:
//
//   phi = G(a): FP(G(a), {a}) = (G(a), TRUE) [self-loop, the degenerate case
//   of the validated "G(a -> Xb)" row with no Xb consequent -- the textbook
//   LTLf progression base case for G(literal)]; FP(G(a), {!a}) = bddfalse
//   [G(a) violated].
//
//   t_in (selector "s", Sigma1 EMPTY so cons == bddtrue everywhere and never
//   masks anything -- isolating the fuse/determinism mechanism from cons):
//   q0 --(s)--> q1, q0 --(!s)--> q2 [out-degree 2, disjoint guards, the fork];
//   q1 --(true)--> q3, q2 --(true)--> q3 [the SAME destination from BOTH
//   branches, the merge]; q3 self-loops.
//
// By hand: state 0 = (row(G(a)), [q0,0]).  Combination s->q1 masks row(G(a))
// to its s=true region (row(G(a)) itself does not branch on 's' at all, so
// masking changes nothing but the guard); at a=true this decodes to
// (G(a), TRUE), producing Key{row(G(a)), [q1,0]} = state 1.  Combination
// !s->q2 likewise produces Key{row(G(a)), [q2,0]} = state 2 -- DIFFERENT from
// state 1 (different transducer destination), so NOT fused yet.
//
// From state 1 ([q1,0]): q1's single edge goes to q3 (guard bddtrue); at
// a=true this decodes to (G(a), TRUE) again, producing Key{row(G(a)),
// [q3,0]} = state 3 (freshly discovered).
// From state 2 ([q2,0]): q2's single edge ALSO goes to q3; at a=true this
// decodes to the SAME (G(a), TRUE), producing the SAME Key{row(G(a)), [q3,0]}
// -- state_index already has this key from state 1's processing, so THIS is
// the fuse point: it must reuse index 3, not mint a 5th state.
//
// Total: exactly 4 states (0,1,2,3), not 5.  State 3 self-loops on itself
// thereafter (q3 -> q3, same row), so nothing further is discovered.
// ---------------------------------------------------------------------------

class OtfProductDiamondFixture : public ::testing::Test {
 protected:
  void SetUp() override {
    dict_ = spot::make_bdd_dict();

    auto tin_g = spot::make_twa_graph(dict_);
    s_ = tin_g->register_ap("s");
    tin_g->new_states(4);  // q0, q1, q2, q3
    tin_g->set_init_state(0);
    tin_g->new_edge(0, 1, bdd_ithvar(s_));
    tin_g->new_edge(0, 2, bdd_nithvar(s_));
    tin_g->new_edge(1, 3, bddtrue);
    tin_g->new_edge(2, 3, bddtrue);
    tin_g->new_edge(3, 3, bddtrue);
    // Sigma1 EMPTY (lambda == bddtrue at every state) => emits_region ==
    // bddtrue, so cons never masks anything and the combination guards are
    // exactly the delta_edges guards -- isolating the fuse from cons.
    t_in_ = std::make_unique<OutputLabeledTransducer>(
        tin_g, std::vector<bdd>{bddtrue, bddtrue, bddtrue, bddtrue},
        /*sigma0=*/bddtrue, /*sigma1=*/bddtrue);

    auto tout_g = spot::make_twa_graph(dict_);
    tout_g->new_states(1);
    tout_g->set_init_state(0);
    tout_g->new_edge(0, 0, bddtrue);
    t_out_ = std::make_unique<OutputLabeledTransducer>(
        tout_g, std::vector<bdd>{bddtrue}, /*sigma0=*/bddtrue,
        /*sigma1=*/bddtrue);

    vars_ = VariablePartition::split({"a", "s"}, {}, /*governed=*/{});
    taus_ = {t_in_.get(), t_out_.get()};
    phi_ = Phi("G(a)");
  }

  bdd Letter(bool a, bool s) const {
    const int a_var = dict_->varnum(spot::formula::ap("a"));
    return (a ? bdd_ithvar(a_var) : bdd_nithvar(a_var)) &
           (s ? bdd_ithvar(s_) : bdd_nithvar(s_));
  }

  spot::bdd_dict_ptr dict_;
  int s_ = -1;
  std::unique_ptr<OutputLabeledTransducer> t_in_, t_out_;
  VariablePartition vars_;
  std::vector<const Transducer*> taus_;
  spot::formula phi_;
};

TEST_F(OtfProductDiamondFixture, DiamondForkThenMergeFusesToExactlyFourStates) {
  const spot::mtdfa_ptr d = otf_product_to_mtdfa(phi_, taus_, vars_, dict_);
  ASSERT_NE(d, nullptr);
  EXPECT_EQ(d->states.size(), 4u)
      << "component-wise interning on (row, q) (PRD I7.3): 5 means the two "
        "diamond branches (destination q1 and q2, both continuing to q3) "
        "were interned as TWO separate states instead of fusing at q3 -- the "
        "state_index lookup in Relabel is not deduplicating correctly";

  // Language spot-check, so a wrong-but-4-state construction cannot pass.
  EXPECT_TRUE(MtdfaAccepts(d, {Letter(/*a=*/true, /*s=*/true)}))
      << "G(a) self-loops accepting at a=true regardless of the fork branch";
  EXPECT_TRUE(MtdfaAccepts(d, {Letter(true, false)}))
      << "the OTHER fork branch (!s -> q2) must accept identically";
  EXPECT_FALSE(MtdfaAccepts(d, {Letter(false, true)}))
      << "G(a) is violated outright at a=false, on either fork branch";

  EXPECT_TRUE(MtdfaAccepts(d, {Letter(true, true), Letter(true, true)}))
      << "both fork branches persist to q3 with the goal self-looping true";
  EXPECT_TRUE(MtdfaAccepts(d, {Letter(true, false), Letter(true, true)}))
      << "the !s branch (q2) merges with the s branch (q1) at q3 -- this "
        "trace takes the !s fork then the SAME q3 state the s fork would "
        "have reached";
  EXPECT_FALSE(MtdfaAccepts(d, {Letter(true, true), Letter(false, true)}))
      << "a non-cons-irrelevant but goal-violating SECOND letter still kills "
        "the run after either fork branch";
}

TEST_F(OtfProductDiamondFixture, TwoRunsOnTheSameInputsAreBddEqualStateForState) {
  // Determinism (PRD "Behaviour"/"Implementation phases"): no randomness
  // anywhere -- FIFO worklist, std::map total order on (row.id(), q),
  // delta_edges' fixed order, canonical BDD ops -- so two independent builds
  // of the SAME (phi, taus, vars) on the SAME dict must be BDD-EQUAL
  // state-for-state (BuDDy canonicalises, so == is a semantic+structural
  // check at once). This fixture's out-degree-2 transducer is exactly what
  // the mtnfa-product D3 lesson says a single-combination fixture cannot
  // exercise (iteration order over >1 combination per state).
  const spot::mtdfa_ptr d1 = otf_product_to_mtdfa(phi_, taus_, vars_, dict_);
  const spot::mtdfa_ptr d2 = otf_product_to_mtdfa(phi_, taus_, vars_, dict_);
  ASSERT_NE(d1, nullptr);
  ASSERT_NE(d2, nullptr);
  ASSERT_EQ(d1->states.size(), d2->states.size());
  for (std::size_t i = 0; i < d1->states.size(); ++i) {
    SCOPED_TRACE("state " + std::to_string(i));
    EXPECT_EQ(d1->states[i], d2->states[i]);
  }
}

// ---------------------------------------------------------------------------
// SECTION D -- OtfMtdfaProduct::synthesize level.
// ---------------------------------------------------------------------------

// Phase-1 knob behaviour (PRD "Implementation phases" "Phase 1 behaviour of
// the knob"): OtfMtdfaProduct(true) must throw std::logic_error, NEVER
// silently fall back to build-then-solve -- a silent fallback would make
// Phase 2's differential oracle vacuously pass. The throw fires before
// anything is built (first line of synthesize), so even minimal/trivial
// arguments are enough to observe it.
TEST(OtfMtdfaProductSynthesize, OtfSolveKnobThrowsLogicErrorRatherThanSilentlyFallingBack) {
  const spot::bdd_dict_ptr dict = spot::make_bdd_dict();
  const VariablePartition vars = IoFreeVars();
  register_turn_order_aps(vars, dict);
  const OutputLabeledTransducer t_in = trivial_transducer(vars, Role::t_in, dict);
  const OutputLabeledTransducer t_out = trivial_transducer(vars, Role::t_out, dict);

  OtfMtdfaProduct method(/*otf_solve=*/true);
  EXPECT_THROW(method.synthesize(Phi("G(i -> o)"), vars, t_in, t_out),
              std::logic_error)
      << "Phase 2 (otf_solve_fused) is not implemented; the knob must throw, "
        "never silently run build-then-solve instead";
}

// Explicit default-false confirmation: the knob is OFF by default (PRD
// "Interfaces & types": "Default OFF"), so a default-constructed
// OtfMtdfaProduct must NOT throw and must behave like the plain
// build-then-solve path.
TEST(OtfMtdfaProductSynthesize, DefaultConstructedOtfSolveIsFalseAndDoesNotThrow) {
  const spot::bdd_dict_ptr dict = spot::make_bdd_dict();
  const VariablePartition vars = IoFreeVars();
  register_turn_order_aps(vars, dict);
  const OutputLabeledTransducer t_in = trivial_transducer(vars, Role::t_in, dict);
  const OutputLabeledTransducer t_out = trivial_transducer(vars, Role::t_out, dict);

  OtfMtdfaProduct method;
  std::optional<Controller> controller;
  EXPECT_NO_THROW(controller = method.synthesize(Phi("G(i -> o)"), vars, t_in, t_out));
  EXPECT_TRUE(controller.has_value());
}

// Turn-order violation (PRD "Edge cases", "Interfaces & types" I8): a bad AP
// order established on the dict BEFORE synthesize runs must throw
// std::invalid_argument, never silently misreport. Mirrors
// tests/turn_order_test.cpp's MtdfaProduct analogue exactly, retargeted at
// OtfMtdfaProduct.
TEST(OtfMtdfaProductSynthesize, TurnOrderViolationThrowsInvalidArgumentInsteadOfMisreporting) {
  const VariablePartition vars =
      VariablePartition::split({"z", "b"}, {"a"}, /*governed=*/{"b"});
  const spot::bdd_dict_ptr dict = spot::make_bdd_dict();
  // Bad order: 'b' (controllable) registered above 'z' (Ifree) -- the
  // spurious-unrealizable shape Phase 0/Q2 probed for MtdfaProduct.
  auto registrar = spot::make_twa_graph(dict);
  registrar->register_ap("b");
  registrar->register_ap("z");
  registrar->register_ap("a");

  auto tin_g = spot::make_twa_graph(dict);
  const int z = tin_g->register_ap("z");
  const int b = tin_g->register_ap("b");
  tin_g->new_states(1);
  tin_g->set_init_state(0);
  tin_g->new_edge(0, 0, bddtrue);
  const bdd copy_z_into_b =
      (bdd_ithvar(z) & bdd_ithvar(b)) | (bdd_nithvar(z) & bdd_nithvar(b));
  const OutputLabeledTransducer t_in(tin_g, {copy_z_into_b}, /*sigma0=*/bdd_ithvar(z),
                                     /*sigma1=*/bdd_ithvar(b));
  const OutputLabeledTransducer t_out = trivial_transducer(vars, Role::t_out, dict);

  OtfMtdfaProduct method;
  EXPECT_THROW(method.synthesize(Phi("G(a <-> b)"), vars, t_in, t_out),
              std::invalid_argument)
      << "OtfMtdfaProduct must call require_turn_order_aps FIRST (PRD I8) "
        "and fail loudly on a bad AP order, never silently return a wrong "
        "(un)realizability verdict";
}

// phi irrevocably satisfied at once (PRD "Edge cases"): progress_row(phi) ==
// bddtrue for phi=1, so state 0's row is the cons region relabelled to
// bddtrue -- realizable iff some letter is cons-consistent, which holds here
// (trivial taus).
TEST(OtfMtdfaProductSynthesize, FormulaOneCollapsesToTheAcceptingSinkAndIsRealizable) {
  const spot::bdd_dict_ptr dict = spot::make_bdd_dict();
  const VariablePartition vars = IoFreeVars();
  register_turn_order_aps(vars, dict);
  const OutputLabeledTransducer t_in = trivial_transducer(vars, Role::t_in, dict);
  const OutputLabeledTransducer t_out = trivial_transducer(vars, Role::t_out, dict);
  OtfMtdfaProduct method;
  EXPECT_TRUE(method.synthesize(Phi("1"), vars, t_in, t_out).has_value());
}

// phi unsatisfiable (PRD "Edge cases"): progress_row(phi) == bddfalse for
// phi=0, so state 0's row is bddfalse and solve_mtdfa reports unrealizable.
TEST(OtfMtdfaProductSynthesize, FormulaZeroCollapsesToTheRejectingSinkAndIsUnrealizable) {
  const spot::bdd_dict_ptr dict = spot::make_bdd_dict();
  const VariablePartition vars = IoFreeVars();
  register_turn_order_aps(vars, dict);
  const OutputLabeledTransducer t_in = trivial_transducer(vars, Role::t_in, dict);
  const OutputLabeledTransducer t_out = trivial_transducer(vars, Role::t_out, dict);
  OtfMtdfaProduct method;
  EXPECT_FALSE(method.synthesize(Phi("0"), vars, t_in, t_out).has_value());
}

// Controller shape (mirrors tests/mtdfa_product_test.cpp
// RealizableControllerCarriesAStrategy): a realizable case's Controller
// carries a non-null strategy with at least one state.
TEST(OtfMtdfaProductSynthesize, RealizableControllerCarriesAStrategy) {
  const spot::bdd_dict_ptr dict = spot::make_bdd_dict();
  const VariablePartition vars = IoFreeVars();
  register_turn_order_aps(vars, dict);
  const OutputLabeledTransducer t_in = trivial_transducer(vars, Role::t_in, dict);
  const OutputLabeledTransducer t_out = trivial_transducer(vars, Role::t_out, dict);
  OtfMtdfaProduct method;
  const std::optional<Controller> controller =
      method.synthesize(Phi("G(i -> o)"), vars, t_in, t_out);
  ASSERT_TRUE(controller.has_value());
  ASSERT_NE(controller->strategy, nullptr);
  EXPECT_GE(controller->strategy->num_states(), 1u);
}

// ---------------------------------------------------------------------------
// SECTION E -- make_synthesis_method dispatch (CLI wiring, PRD "Interfaces &
// types" "CLI wiring").
// ---------------------------------------------------------------------------

TEST(MakeSynthesisMethod, OtfMtdfaProductFlagBuildsAnOtfMtdfaProduct) {
  std::unique_ptr<Synthesis> method = make_synthesis_method("otf-mtdfa-product");
  ASSERT_NE(method, nullptr);
  EXPECT_NE(dynamic_cast<OtfMtdfaProduct*>(method.get()), nullptr);
}

// ---------------------------------------------------------------------------
// SECTION F -- Bench-span shape (PRD "Test oracles": "exactly
// product_construction + game_solving, and no automaton_construction").
// Unlike MtnfaProduct/MtdfaProduct/DfaProduct, OtfMtdfaProduct never builds a
// separate Goal automaton object at all (the whole point of the method), so
// there is no automaton_construction span to emit -- exactly TWO roots, not
// three.
// ---------------------------------------------------------------------------

TEST(BenchScopeIntegration, OtfMtdfaProductEmitsExactlyProductConstructionAndGameSolving) {
  const spot::bdd_dict_ptr dict = spot::make_bdd_dict();
  const VariablePartition vars = IoFreeVars();
  register_turn_order_aps(vars, dict);
  const OutputLabeledTransducer t_in = trivial_transducer(vars, Role::t_in, dict);
  const OutputLabeledTransducer t_out = trivial_transducer(vars, Role::t_out, dict);
  OtfMtdfaProduct method;

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

  ASSERT_EQ(report.roots.size(), 2u)
      << "expected exactly product_construction + game_solving -- "
        "OtfMtdfaProduct never builds a separate Goal automaton, so a third "
        "root (in particular automaton_construction) means the fused "
        "on-the-fly construction regressed to a two-phase build";
  const std::vector<Stage> expected_order = {Stage::product_construction,
                                             Stage::game_solving};
  for (std::size_t i = 0; i < expected_order.size(); ++i) {
    SCOPED_TRACE("root index " + std::to_string(i));
    EXPECT_EQ(report.roots[i].label, std::string(stage_name(expected_order[i])));
    EXPECT_TRUE(report.roots[i].canonical);
    EXPECT_TRUE(report.roots[i].children.empty())
        << "no nested sub-span anywhere under either root";
  }
  EXPECT_GT(report.total.count(), 0);
}

}  // namespace
