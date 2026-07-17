#include <algorithm>
#include <cstdint>
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

#include "ltlf_ek/mtnfa.hpp"
#include "ltlf_ek/variables.hpp"

// Full Phase-1 + Phase-2 suite for docs/prd/mtnfa.md, bound to the
// RE-FROZEN 2026-07-17 "Interfaces & types" block (Mtnfa::pool mutable,
// Mtnfa::source_nfa present, three signatures unchanged). Phase-1's own
// StateSetPool unit fixtures live in tests/state_set_pool_test.cpp; this
// file covers Mtnfa + nfa_to_mtnfa + ltlf_to_mtnfa + mtnfa_to_mtdfa.
namespace {

using ltlf_ek::Mtnfa;
using ltlf_ek::ltlf_to_mtnfa;
using ltlf_ek::nfa_to_mtnfa;
using ltlf_ek::mtnfa_to_mtdfa;

// ---------------------------------------------------------------------------
// Shared MTBDD-walking helpers.
// ---------------------------------------------------------------------------

// An Mtnfa state row's leaf is always a StateSetPool terminal (PRD "Novel
// mechanisms (a)": bddfalse/bddtrue never appear inside an Mtnfa's states[]),
// so a plain bdd_is_terminal descent is safe here.
unsigned EvalMtnfaTerminal(bdd node, const bdd& letter) {
  while (!bdd_is_terminal(node)) {
    const int v = bdd_var(node);
    node = ((letter & bdd_ithvar(v)) != bddfalse) ? bdd_high(node) : bdd_low(node);
  }
  return static_cast<unsigned>(bdd_get_terminal(node));
}

// A spot::mtdfa row's leaf CAN be the literal bddfalse/bddtrue sink (not a
// StateSetPool-style int terminal), and empirically bdd_is_terminal(bddfalse)
// and bdd_is_terminal(bddtrue) are BOTH false (verified against libspot
// directly -- bddfalse/bddtrue are BuDDy's own reserved leaves, distinct
// from the integer-terminal extension bdd_is_terminal recognizes), so the
// descent must check identity against both sinks explicitly or it would
// never terminate / call bdd_var on a sink node.
bdd DescendMtdfaRow(bdd node, const bdd& letter) {
  while (!bdd_is_terminal(node) && node != bddfalse && node != bddtrue) {
    const int v = bdd_var(node);
    node = ((letter & bdd_ithvar(v)) != bddfalse) ? bdd_high(node) : bdd_low(node);
  }
  return node;
}

// Walks `dfa` from states[0] over `word` (a sequence of full-letter cubes),
// using Spot's own mtdfa terminal convention (ltlf2dfa.hh "mtdfa"):
// bddfalse = rejecting sink, bddtrue = accepting sink, else terminal 2*d+b
// names destination state `d` with this transition's accepting bit `b`.
// Acceptance is the LAST transition's bit (transition-based, finite-word
// semantics) -- matches the empty word always rejecting (PRD "Behaviour" #3).
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

// Independent, mtnfa-machinery-free oracle for a hand-built (possibly
// nondeterministic, possibly partial) spot::twa_graph NFA: tracks the SET of
// reachable NFA states across `word`, accepting iff some survivor is
// accepting (mirrors tests/reverse_dfa_to_nfa_test.cpp's NfaAccepts). Used
// below to check mtnfa_to_mtdfa's subset construction against the textbook
// definition directly, with no Spot/MONA involvement at all.
bool NfaAcceptsSubset(const spot::twa_graph_ptr& nfa, const std::vector<bdd>& word) {
  std::set<unsigned> current{nfa->get_init_state_number()};
  for (const bdd& v : word) {
    std::set<unsigned> next;
    for (unsigned s : current)
      for (const auto& e : nfa->out(s))
        if ((v & e.cond) != bddfalse) next.insert(e.dst);
    current = std::move(next);
    if (current.empty()) return false;
  }
  for (unsigned s : current)
    if (nfa->state_is_accepting(s)) return true;
  return false;
}

std::set<std::string> ApNameSet(const std::vector<spot::formula>& aps) {
  std::set<std::string> names;
  for (const spot::formula& ap : aps) names.insert(ap.ap_name());
  return names;
}

// Registers `phi`'s AP names on `dict` (idempotent if already registered by
// ltlf_to_mtnfa / spot::ltlf_to_mtdfa) and returns their bdd variable ids --
// same "throwaway registrar" idiom as tests/state_set_pool_test.cpp /
// tests/reverse_dfa_to_nfa_test.cpp.
std::vector<int> ApVarsForFormula(const spot::bdd_dict_ptr& dict,
                                  const spot::formula& phi) {
  spot::twa_graph_ptr registrar = spot::make_twa_graph(dict);
  std::vector<int> vars;
  for (const std::string& name : ltlf_ek::collect_aps(phi))
    vars.push_back(registrar->register_ap(name));
  return vars;
}

bdd RandomLetter(const std::vector<int>& ap_vars, std::mt19937& rng) {
  std::bernoulli_distribution bit(0.5);
  bdd letter = bddtrue;
  for (int v : ap_vars) letter &= bit(rng) ? bdd_ithvar(v) : bdd_nithvar(v);
  return letter;
}

std::vector<bdd> RandomWord(const std::vector<int>& ap_vars, std::size_t length,
                            std::mt19937& rng) {
  std::vector<bdd> word;
  word.reserve(length);
  for (std::size_t t = 0; t < length; ++t) word.push_back(RandomLetter(ap_vars, rng));
  return word;
}

// ---------------------------------------------------------------------------
// Construction structural free-riders + hand-built determinize oracle
// (PRD "Test oracles": "nfa_to_mtnfa / mtnfa_to_mtdfa on a hand-built
// twa_graph need no mona and always run (ungated)"). No MONA anywhere below.
// ---------------------------------------------------------------------------

// N: 3 states, 2 APs (a, b), deliberately OVERLAPPING guards out of state 0
// (guards "a" and "a|b" both fire on any letter with a=true) -- pins the
// union-merge-on-overlap = nondeterminism behaviour ("Interfaces & types",
// nfa_to_mtnfa doc). State 0 also leaves the letter !a&!b UNCOVERED (partial
// delta_N, "Edge cases" "Uncovered letter"). State 1 (accepting) has a
// self-loop so its Final mark is representable; state 2 forwards to 1 on
// every letter.
//   delta_N(0, a&b)   = {1,2}      delta_N(0, a&!b)  = {1,2}
//   delta_N(0, !a&b)  = {2}        delta_N(0, !a&!b) = {}      (uncovered)
//   delta_N(1, *)     = {1}        delta_N(2, *)     = {1}
class MtnfaOverlappingGuardsFixture : public ::testing::Test {
 protected:
  void SetUp() override {
    dict_ = spot::make_bdd_dict();
    n_ = spot::make_twa_graph(dict_);
    a_ = n_->register_ap("a");
    b_ = n_->register_ap("b");
    n_->set_buchi();
    n_->prop_state_acc(true);
    n_->new_states(3);
    n_->set_init_state(0);
    const spot::acc_cond::mark_t kFinal = {0};
    const spot::acc_cond::mark_t kNone = {};
    n_->new_edge(0, 1, bdd_ithvar(a_), kNone);
    n_->new_edge(0, 2, bdd_ithvar(a_) | bdd_ithvar(b_), kNone);
    n_->new_edge(1, 1, bddtrue, kFinal);  // state 1 is F_N; self-loop
    n_->new_edge(2, 1, bddtrue, kNone);
    letters_ = {bdd_ithvar(a_) & bdd_ithvar(b_), bdd_ithvar(a_) & bdd_nithvar(b_),
               bdd_nithvar(a_) & bdd_ithvar(b_), bdd_nithvar(a_) & bdd_nithvar(b_)};
  }

  // Every word of length 0..max_len over {a, b}: 4^0 + 4^1 + ... + 4^max_len
  // words, cheap to enumerate exhaustively for a small max_len.
  std::vector<std::vector<bdd>> AllWordsOverAb(unsigned max_len) const {
    std::vector<std::vector<bdd>> words{{}};
    for (unsigned len = 1; len <= max_len; ++len) {
      std::vector<std::vector<bdd>> next;
      for (const std::vector<bdd>& w : words) {
        if (w.size() != len - 1) continue;
        for (const bdd& l : letters_) {
          std::vector<bdd> nw = w;
          nw.push_back(l);
          next.push_back(nw);
        }
      }
      words.insert(words.end(), next.begin(), next.end());
    }
    return words;
  }

  spot::bdd_dict_ptr dict_;
  spot::twa_graph_ptr n_;
  int a_ = -1, b_ = -1;
  std::vector<bdd> letters_;  // {a&b, a&!b, !a&b, !a&!b}
};

TEST_F(MtnfaOverlappingGuardsFixture, StatesSizeMatchesNfaNumStates) {
  const Mtnfa mtnfa = nfa_to_mtnfa(n_);
  EXPECT_EQ(mtnfa.states.size(), n_->num_states());
}

TEST_F(MtnfaOverlappingGuardsFixture, AcceptingAndInitialMatchTheNfa) {
  const Mtnfa mtnfa = nfa_to_mtnfa(n_);
  ASSERT_EQ(mtnfa.accepting.size(), 3u);
  EXPECT_FALSE(mtnfa.accepting[0]);
  EXPECT_TRUE(mtnfa.accepting[1]);
  EXPECT_FALSE(mtnfa.accepting[2]);
  EXPECT_EQ(mtnfa.initial, n_->get_init_state_number());
}

TEST_F(MtnfaOverlappingGuardsFixture,
      TerminalSetsEqualDeltaNOnSampledLettersIncludingOverlapAndUncovered) {
  const Mtnfa mtnfa = nfa_to_mtnfa(n_);
  const bdd ab = letters_[0], a_nb = letters_[1], na_b = letters_[2], na_nb = letters_[3];

  // State 0: overlapping guards merge; the fourth letter is uncovered ->
  // empty set (terminal index 0), never a sink state.
  EXPECT_EQ(mtnfa.pool.set_of(EvalMtnfaTerminal(mtnfa.states[0], ab)),
           (std::vector<unsigned>{1, 2}));
  EXPECT_EQ(mtnfa.pool.set_of(EvalMtnfaTerminal(mtnfa.states[0], a_nb)),
           (std::vector<unsigned>{1, 2}));
  EXPECT_EQ(mtnfa.pool.set_of(EvalMtnfaTerminal(mtnfa.states[0], na_b)),
           (std::vector<unsigned>{2}));
  EXPECT_TRUE(mtnfa.pool.set_of(EvalMtnfaTerminal(mtnfa.states[0], na_nb)).empty());

  // States 1, 2: every letter forwards to the same fixed singleton.
  for (const bdd& letter : letters_) {
    EXPECT_EQ(mtnfa.pool.set_of(EvalMtnfaTerminal(mtnfa.states[1], letter)),
             (std::vector<unsigned>{1}));
    EXPECT_EQ(mtnfa.pool.set_of(EvalMtnfaTerminal(mtnfa.states[2], letter)),
             (std::vector<unsigned>{1}));
  }
}

// Headline ungated determinize oracle: mtnfa_to_mtdfa's subset construction,
// checked directly against the textbook NfaToDfa definition (main.tex
// alg:nfa_product:determinize) simulated independently on the SAME hand-built
// N, with no shared machinery. Exhaustive over every word of length 0..4
// (341 words), so this also structurally pins "states[0] is the initial
// subset R0={nfa.initial}" -- MtdfaAccepts always starts at states[0], and
// every length->=1 word (including length 1) must match NfaAcceptsSubset
// starting at {n.initial} for this to hold, at every prefix.
TEST_F(MtnfaOverlappingGuardsFixture, MtdfaLanguageMatchesDirectSubsetSimulation) {
  const Mtnfa mtnfa = nfa_to_mtnfa(n_);
  const spot::mtdfa_ptr d = mtnfa_to_mtdfa(mtnfa);
  ASSERT_NE(d, nullptr);
  for (const std::vector<bdd>& w : AllWordsOverAb(/*max_len=*/4)) {
    SCOPED_TRACE("word length " + std::to_string(w.size()));
    EXPECT_EQ(MtdfaAccepts(d, w), NfaAcceptsSubset(n_, w))
        << "L(mtnfa_to_mtdfa(nfa_to_mtnfa(N))) != L(N) (direct subset sim)";
  }
}

TEST_F(MtnfaOverlappingGuardsFixture, RejectsEmptyWord) {
  const Mtnfa mtnfa = nfa_to_mtnfa(n_);
  const spot::mtdfa_ptr d = mtnfa_to_mtdfa(mtnfa);
  EXPECT_FALSE(MtdfaAccepts(d, {}));
}

TEST_F(MtnfaOverlappingGuardsFixture, NeverReturnsNullptr) {
  const Mtnfa mtnfa = nfa_to_mtnfa(n_);
  EXPECT_NE(mtnfa_to_mtdfa(mtnfa), nullptr);
}

// Ungated negative control (mtdfa-product Phase-0/Q1 lesson: a naive
// language-equivalence oracle can be silently non-discriminating): a
// deliberately-broken determinization -- flipping the source N's OWN
// accepting flags before lifting -- must disagree with the untouched N's
// real language on at least one word. Proves NfaAcceptsSubset-vs-MtdfaAccepts
// actually discriminates, with no Spot/MONA involved at all.
TEST_F(MtnfaOverlappingGuardsFixture,
      NegativeControlDetectsBrokenAcceptingFlagsWithNoMona) {
  spot::twa_graph_ptr broken = spot::make_twa_graph(dict_);
  broken->register_ap("a");
  broken->register_ap("b");
  broken->set_buchi();
  broken->prop_state_acc(true);
  broken->new_states(3);
  broken->set_init_state(0);
  const spot::acc_cond::mark_t kFinal = {0};
  const spot::acc_cond::mark_t kNone = {};
  // Same edges, but accepting is now {2} instead of {1} -- a corrupted
  // construction, not a legitimate alternative NFA for the same language.
  broken->new_edge(0, 1, bdd_ithvar(a_), kNone);
  broken->new_edge(0, 2, bdd_ithvar(a_) | bdd_ithvar(b_), kNone);
  broken->new_edge(1, 1, bddtrue, kNone);
  broken->new_edge(2, 1, bddtrue, kFinal);  // WRONG: source 2 is not F_N here

  const Mtnfa broken_mtnfa = nfa_to_mtnfa(broken);
  const spot::mtdfa_ptr broken_d = mtnfa_to_mtdfa(broken_mtnfa);
  // Word "!a&b": ONLY edge (0,2,a|b) fires (a is false), so the reached
  // subset is {2} alone -- unlike "a&!b"/"a&b", which always co-reach {1,2}
  // together in this fixture (guard "a" implies guard "a|b"), so swapping
  // Final between 1 and 2 leaves any({1,2})'s OR unchanged there. On the
  // singleton subset {2}, the swap is directly observable: real N has
  // accepting[2]=false (reject); the broken graph has accepting[2]=true
  // (accept).
  const std::vector<bdd> w{letters_[2]};
  EXPECT_NE(MtdfaAccepts(broken_d, w), NfaAcceptsSubset(n_, w))
      << "negative control: swapping which state's out-edges carry Final "
         "must desync the determinized language from N's real language, "
         "else this oracle cannot discriminate a broken determinization";
}

// ---------------------------------------------------------------------------
// Generated corpus (docs/prd/generated-corpus-oracle.md generators, reused
// in-file per this project's one-file-per-suite style -- mirrors
// tests/ltlfsynt_oracle_test.cpp's own precedent of duplicating its
// subprocess harness rather than sharing across translation units). Trimmed
// to exactly what mtnfa needs: a random LTLf formula over a random small AP
// set, no VariablePartition/Tin (mtnfa has no I/O role semantics -- it is
// the Goal automaton alone). MONA-gated throughout (ltlf_to_mtnfa inherits
// ltlf_to_nfa's mona runtime dependency).
// ---------------------------------------------------------------------------

constexpr unsigned kMtnfaCorpusSeed = 20260717;
constexpr std::size_t kMtnfaCorpusCaseCount = 60;
constexpr int kMtnfaCorpusTreeSizeMin = 1;
constexpr int kMtnfaCorpusTreeSizeMax = 10;
constexpr int kMtnfaCorpusMaxAps = 4;

constexpr unsigned kMtnfaMembershipCorpusSeed = 20260718;
constexpr std::size_t kMtnfaMembershipCaseCount = 30;
constexpr int kMtnfaMembershipTreeSizeMax = 20;  // deliberately larger: "too
                                                 // large to XOR-compare
                                                 // exactly" (PRD "Test
                                                 // oracles", membership fuzz).
constexpr int kMtnfaMembershipMaxAps = 5;
constexpr int kMtnfaTracesPerCase = 6;
constexpr std::size_t kMtnfaMaxTraceLength = 8;

std::set<std::string> RandomApNames(std::mt19937& rng, int max_aps) {
  std::uniform_int_distribution<int> count_dist(1, max_aps);
  const int n = count_dist(rng);
  std::set<std::string> names;
  for (int i = 0; i < n; ++i) names.insert("p" + std::to_string(i));
  return names;
}

// Same technique as tests/ltlfsynt_oracle_test.cpp's generate_random_formula:
// a thin wrapper over spot::randltlgenerator (the `randltl` binary's own
// class), APs restricted to `ap_names`, xor/M (strong release) disabled --
// duplicated here rather than shared, per this project's precedent.
spot::formula GenerateRandomFormula(const std::set<std::string>& ap_names,
                                    std::mt19937& rng, int tree_size_max) {
  spot::atomic_prop_set aprops;
  for (const std::string& name : ap_names)
    aprops.insert(spot::default_environment::instance().require(name));

  spot::option_map opts;
  opts.set("output", spot::randltlgenerator::LTL);
  opts.set("tree_size_min", kMtnfaCorpusTreeSizeMin);
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

std::string FormulaStr(const spot::formula& phi) {
  std::ostringstream os;
  os << phi;
  return os.str();
}

// Phase-2 primary oracle (PRD "Test oracles" / "Implementation phases" Phase
// 2 checkpoint): product_xor(mtnfa_to_mtdfa(ltlf_to_mtnfa(phi)),
// spot::ltlf_to_mtdfa(phi)).is_empty() over a fixed-seed generated corpus,
// each case on its own private dict (both constructions share it, required
// for product_xor to compare a common variable numbering). Also carries the
// "well-formed over exactly phi's support APs" structural free-rider.
TEST(MtnfaGeneratedCorpus, DeterminizedMtnfaAgreesWithSpotLtlfToMtdfa) {
#ifndef MONA_FOUND
  GTEST_SKIP() << "mona not found (CMake find_program(mona)); skipping the "
                  "mtnfa_to_mtdfa isolated determinization oracle";
#else
  std::mt19937 rng(kMtnfaCorpusSeed);
  for (std::size_t i = 0; i < kMtnfaCorpusCaseCount; ++i) {
    const std::set<std::string> ap_names = RandomApNames(rng, kMtnfaCorpusMaxAps);
    const spot::formula phi =
        GenerateRandomFormula(ap_names, rng, kMtnfaCorpusTreeSizeMax);
    SCOPED_TRACE("case " + std::to_string(i) + ": phi=" + FormulaStr(phi));

    const spot::bdd_dict_ptr dict = spot::make_bdd_dict();
    const Mtnfa mtnfa = ltlf_to_mtnfa(phi, dict);
    const spot::mtdfa_ptr got = mtnfa_to_mtdfa(mtnfa);
    ASSERT_NE(got, nullptr);
    const spot::mtdfa_ptr want = spot::ltlf_to_mtdfa(phi, dict);

    EXPECT_EQ(ApNameSet(got->aps), ltlf_ek::collect_aps(phi))
        << "mtnfa_to_mtdfa's aps must be exactly phi's support";

    const spot::mtdfa_ptr xor_result = spot::product_xor(got, want);
    EXPECT_TRUE(xor_result->is_empty())
        << "product_xor(mtnfa_to_mtdfa(ltlf_to_mtnfa(phi)), "
           "spot::ltlf_to_mtdfa(phi)) is non-empty for phi=" << FormulaStr(phi);
  }
#endif
}

// Membership fuzz "at scale" (PRD "Test oracles": "catches edge letters on
// formulas too large to XOR-compare exactly"): a SEPARATE, larger-formula
// corpus (bigger tree_size_max / AP count) where an exact product_xor would
// be comparatively expensive; only random non-empty traces are checked.
TEST(MtnfaGeneratedCorpus, MembershipFuzzAgreesWithSpotLtlfToMtdfaAtScale) {
#ifndef MONA_FOUND
  GTEST_SKIP() << "mona not found (CMake find_program(mona)); skipping the "
                  "mtnfa_to_mtdfa membership-fuzz oracle";
#else
  std::mt19937 rng(kMtnfaMembershipCorpusSeed);
  for (std::size_t i = 0; i < kMtnfaMembershipCaseCount; ++i) {
    const std::set<std::string> ap_names =
        RandomApNames(rng, kMtnfaMembershipMaxAps);
    const spot::formula phi =
        GenerateRandomFormula(ap_names, rng, kMtnfaMembershipTreeSizeMax);
    SCOPED_TRACE("case " + std::to_string(i) + ": phi=" + FormulaStr(phi));

    const spot::bdd_dict_ptr dict = spot::make_bdd_dict();
    const Mtnfa mtnfa = ltlf_to_mtnfa(phi, dict);
    const spot::mtdfa_ptr got = mtnfa_to_mtdfa(mtnfa);
    const spot::mtdfa_ptr want = spot::ltlf_to_mtdfa(phi, dict);
    const std::vector<int> ap_vars = ApVarsForFormula(dict, phi);

    std::mt19937 word_rng(kMtnfaMembershipCorpusSeed + 1000003u * static_cast<unsigned>(i));
    std::uniform_int_distribution<std::size_t> len_dist(1, kMtnfaMaxTraceLength);
    for (int t = 0; t < kMtnfaTracesPerCase; ++t) {
      const std::vector<bdd> w = RandomWord(ap_vars, len_dist(word_rng), word_rng);
      EXPECT_EQ(MtdfaAccepts(got, w), MtdfaAccepts(want, w))
          << "membership mismatch on a length-" << w.size()
          << " trace for phi=" << FormulaStr(phi);
    }
  }
#endif
}

// Required negative control on the MONA-gated primary oracle itself
// (mtdfa-product Phase-0/Q1 lesson): a G(a)-vs-F(a) style mismatch -- two
// formulas with different languages -- fed through the SAME comparison the
// primary oracle uses must give a NON-empty product_xor, proving the oracle
// is not vacuously true.
TEST(MtnfaGeneratedCorpus, NegativeControlDetectsMismatchedGvsFFormulas) {
#ifndef MONA_FOUND
  GTEST_SKIP() << "mona not found (CMake find_program(mona)); skipping the "
                  "negative control";
#else
  const spot::bdd_dict_ptr dict = spot::make_bdd_dict();
  const spot::formula g_a = spot::parse_formula("G(a)");
  const spot::formula f_a = spot::parse_formula("F(a)");

  const Mtnfa mtnfa = ltlf_to_mtnfa(g_a, dict);
  const spot::mtdfa_ptr got = mtnfa_to_mtdfa(mtnfa);
  const spot::mtdfa_ptr want = spot::ltlf_to_mtdfa(f_a, dict);

  const spot::mtdfa_ptr xor_result = spot::product_xor(got, want);
  EXPECT_FALSE(xor_result->is_empty())
      << "negative control: G(a) and F(a) denote different languages; a "
         "vacuous (empty) product_xor here means the oracle cannot "
         "discriminate";
#endif
}

// Required negative control, deliberately-broken-determinization variant: a
// hand-corrupted Mtnfa (its own real construction, then every accepting flag
// flipped before determinizing) must desync from the independent
// spot::ltlf_to_mtdfa oracle for the SAME phi.
TEST(MtnfaGeneratedCorpus, NegativeControlDetectsCorruptedAcceptingFlags) {
#ifndef MONA_FOUND
  GTEST_SKIP() << "mona not found (CMake find_program(mona)); skipping the "
                  "negative control";
#else
  const spot::bdd_dict_ptr dict = spot::make_bdd_dict();
  const spot::formula phi = spot::parse_formula("X[!] a");

  Mtnfa mtnfa = ltlf_to_mtnfa(phi, dict);
  ASSERT_FALSE(mtnfa.accepting.empty());
  // Flip every accepting flag EXCEPT the initial state's: mtnfa_to_mtdfa's F2
  // precondition (asserted) forbids an accepting initial state, and corrupting
  // it would trip that assert rather than exercise the language oracle.  Every
  // *other* flag flipping still desyncs the determinized language for X[!] a
  // (which has non-initial accepting states), so the control stays valid.
  for (std::size_t s = 0; s < mtnfa.accepting.size(); ++s)
    if (s != mtnfa.initial)
      mtnfa.accepting[s] = !mtnfa.accepting[s];

  const spot::mtdfa_ptr broken = mtnfa_to_mtdfa(mtnfa);
  const spot::mtdfa_ptr want = spot::ltlf_to_mtdfa(phi, dict);

  const spot::mtdfa_ptr xor_result = spot::product_xor(broken, want);
  EXPECT_FALSE(xor_result->is_empty())
      << "negative control: flipping every Mtnfa::accepting flag must "
         "desync the determinized language from spot::ltlf_to_mtdfa; a "
         "vacuous (empty) product_xor here means the oracle cannot "
         "discriminate a broken determinization";
#endif
}

// ---------------------------------------------------------------------------
// Determinize structural edge cases (PRD "Edge cases" / "Test oracles"):
// phi=0 and phi=1 shapes, both MONA-gated (ltlf_to_mtnfa inherits
// ltlf_to_nfa's mona dependency even for these trivial formulas).
// ---------------------------------------------------------------------------

TEST(MtnfaToMtdfaEdgeCases, TriviallyFalseYieldsEmptyLanguageAndNeverNullptr) {
#ifndef MONA_FOUND
  GTEST_SKIP() << "mona not found (CMake find_program(mona)); skipping the "
                  "phi=0 edge case";
#else
  const spot::bdd_dict_ptr dict = spot::make_bdd_dict();
  const spot::formula phi = spot::parse_formula("0");
  const Mtnfa mtnfa = ltlf_to_mtnfa(phi, dict);
  const spot::mtdfa_ptr d = mtnfa_to_mtdfa(mtnfa);

  ASSERT_NE(d, nullptr);
  EXPECT_FALSE(d->states.empty());
  EXPECT_TRUE(d->is_empty()) << "L(N)=empty for phi=0 must determinize to an "
                                "empty-language mtdfa";
  EXPECT_TRUE(spot::product_xor(d, spot::ltlf_to_mtdfa(phi, dict))->is_empty());
#endif
}

TEST(MtnfaToMtdfaEdgeCases, TriviallyTrueAcceptsEveryNonEmptyTraceRejectsEmpty) {
#ifndef MONA_FOUND
  GTEST_SKIP() << "mona not found (CMake find_program(mona)); skipping the "
                  "phi=1 edge case";
#else
  const spot::bdd_dict_ptr dict = spot::make_bdd_dict();
  const spot::formula phi = spot::parse_formula("1");
  const Mtnfa mtnfa = ltlf_to_mtnfa(phi, dict);
  const spot::mtdfa_ptr d = mtnfa_to_mtdfa(mtnfa);

  ASSERT_NE(d, nullptr);
  EXPECT_FALSE(d->is_empty());
  EXPECT_FALSE(MtdfaAccepts(d, {}));

  const std::vector<int> ap_vars = ApVarsForFormula(dict, phi);
  std::mt19937 rng(20260717);
  for (int t = 0; t < 5; ++t)
    EXPECT_TRUE(MtdfaAccepts(d, RandomWord(ap_vars, 1 + t, rng)))
        << "phi=1 must accept every non-empty trace, length " << (1 + t);

  EXPECT_TRUE(spot::product_xor(d, spot::ltlf_to_mtdfa(phi, dict))->is_empty());
#endif
}

// ---------------------------------------------------------------------------
// bdd_dict lifetime regression test (docs/prd/mtnfa.md "Developer comments /
// PRD disagreements", 2026-07-17 Phase 2, flagged explicitly for
// /test-writer): ltlf_to_mtnfa's temporary ltlf_to_nfa(phi, dict) result used
// to die on return, freeing its AP variable numbers back to `dict` while
// Mtnfa::states still referenced them by index; a LATER dict->register_ap
// (here: spot::ltlf_to_mtdfa building the independent oracle) could alias
// those freed numbers, silently corrupting product_xor. Must call
// ltlf_to_mtnfa directly (NOT decomposed into ltlf_to_nfa + nfa_to_mtnfa
// with the intermediate kept alive in a named variable, which would hide the
// bug by keeping the graph alive incidentally).
//
// HISTORY: this test FAILED on arrival, and that failure was a real second
// bug -- a narrower lifetime hazard than the one Mtnfa::source_nfa fixes.
// source_nfa keeps phi's AP variables registered only for as long as the
// Mtnfa ITSELF is alive, but mtnfa_to_mtdfa's returned spot::mtdfa claimed no
// ownership stake of its own (it reused the source NFA's variable numbers via
// bdd_ite without ever calling dict->register_proposition(ap, out.get())).
// So this exact shape -- mtnfa_to_mtdfa(ltlf_to_mtnfa(phi, dict)), keeping
// ONLY the mtdfa_ptr and discarding the Mtnfa temporary, which is the natural
// calling pattern the public interface invites with no documented obligation
// to keep the Mtnfa alive -- left the mtdfa holding BDDs over variable
// numbers `dict` was free to recycle, yielding a spurious non-empty
// product_xor. Confirmed via an isolated probe: Mtnfa discarded => non-empty;
// Mtnfa kept alive in a named local => empty. That contrast is exactly why
// the generated-corpus loop above did NOT catch it -- it keeps the Mtnfa in a
// named local across the whole comparison.
//
// FIXED in src/mtnfa.cpp: mtnfa_to_mtdfa now registers its own APs, paired by
// spot::mtdfa's destructor (dict_->unregister_all_my_variables(this),
// ltlf2dfa.hh:130). This test now PASSES and guards that fix -- it is the
// only test exercising the discard-the-Mtnfa path, so keep it that way: do
// NOT "tidy" the Mtnfa into a named local here or it stops testing anything.
// ---------------------------------------------------------------------------

TEST(MtnfaBddDictLifetime, LtlfToMtnfaThenSpotLtlfToMtdfaOnSameDictAgree) {
#ifndef MONA_FOUND
  GTEST_SKIP() << "mona not found (CMake find_program(mona)); skipping the "
                  "bdd_dict lifetime regression test";
#else
  const spot::bdd_dict_ptr dict = spot::make_bdd_dict();
  const spot::formula phi = spot::parse_formula("!(a & b) | c");

  // Direct call -- the one-line composition, no intermediate Mtnfa kept
  // alive by the caller (only the resulting mtdfa_ptr survives). See the
  // FINDING above: Mtnfa::source_nfa does not cover this shape.
  const spot::mtdfa_ptr got = mtnfa_to_mtdfa(ltlf_to_mtnfa(phi, dict));
  // Built AFTER the above returns, on the SAME dict -- exactly the ordering
  // that exposed the bug (a later register_ap call reusing freed variable
  // numbers).
  const spot::mtdfa_ptr want = spot::ltlf_to_mtdfa(phi, dict);

  const spot::mtdfa_ptr xor_result = spot::product_xor(got, want);
  EXPECT_TRUE(xor_result->is_empty())
      << "bdd_dict lifetime regression: ltlf_to_mtnfa(phi, dict) called "
         "directly, mtdfa_ptr kept but the Mtnfa temporary discarded, then "
         "spot::ltlf_to_mtdfa(phi, dict) on the same dict must still agree "
         "-- a non-empty XOR here means the returned mtdfa_ptr does not "
         "keep phi's AP variables alive on its own (see the FINDING comment "
         "above this test)";
#endif
}

}  // namespace
