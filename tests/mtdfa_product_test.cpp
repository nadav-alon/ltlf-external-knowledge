#include <optional>
#include <ostream>
#include <stdexcept>
#include <string>
#include <vector>

#include <gtest/gtest.h>
#include <bddx.h>
#include <spot/tl/parse.hh>
#include <spot/twa/bdddict.hh>
#include <spot/twa/twagraph.hh>

#include "ltlf_ek/dfa_product.hpp"
#include "ltlf_ek/mtdfa_product.hpp"
#include "ltlf_ek/output_labeled_transducer.hpp"
#include "ltlf_ek/role.hpp"
#include "ltlf_ek/turn_order.hpp"
#include "ltlf_ek/variables.hpp"

#include "support/fixtures.hpp"

// Hand-authored end-to-end fixtures for MtdfaProduct (docs/prd/mtdfa-
// product.md "Test oracles" #4): "mirror the existing Tables A-E rows,
// including the two Mealy-only payoff rows: they are exactly what would
// catch a turn-order error in decision 2."  Tables A-D (known-input Tin) and
// Table E (empty knowledge) are the corpora defined in
// tests/ltlfsynt_oracle_test.cpp's BuildKnownInputCorpus /
// BuildEmptyKnowledgeCorpus; duplicated here rather than shared (this
// project's one-file-per-suite duplication norm, matching CliResult /
// ShellQuote across ltlf_ek_synth_test.cpp / ltlfsynt_oracle_test.cpp), and
// re-targeted at MtdfaProduct::synthesize directly (library-level, no
// subprocess) instead of driving the ek-synth binary.
//
// FROZEN CONTRACT, NOT YET IMPLEMENTED (concurrent workflow): this file will
// not compile/link until include/ltlf_ek/mtdfa_product.hpp,
// include/ltlf_ek/turn_order.hpp and their src/*.cpp land.
//
// Every fixture calls register_turn_order_aps(vars, dict) immediately after
// spot::make_bdd_dict(), before building any transducer --- the "dict-setup
// site" rule (PRD "Definition of done": "register_turn_order_aps at every
// dict-setup site (CLI and tests)").
namespace {

using ltlf_ek::Controller;
using ltlf_ek::DfaProduct;
using ltlf_ek::MtdfaProduct;
using ltlf_ek::OutputLabeledTransducer;
using ltlf_ek::register_turn_order_aps;
using ltlf_ek::Role;
using ltlf_ek::trivial_transducer;
using ltlf_ek::VariablePartition;

using ltlf_ek::test_support::Phi;

// Both methods must agree with the row's known-correct verdict --- catches a
// turn-order regression in MtdfaProduct (the point of this whole oracle) as
// well as any accidental DfaProduct regression the shared fixture triggers.
void ExpectBothMethodsAgree(const spot::formula& phi,
                            const VariablePartition& vars,
                            const OutputLabeledTransducer& t_in,
                            const OutputLabeledTransducer& t_out,
                            bool expected_realizable) {
  DfaProduct dfa_method;
  MtdfaProduct mtdfa_method;
  const bool dfa_realizable =
      dfa_method.synthesize(phi, vars, t_in, t_out).has_value();
  const bool mtdfa_realizable =
      mtdfa_method.synthesize(phi, vars, t_in, t_out).has_value();
  EXPECT_EQ(dfa_realizable, expected_realizable)
      << "DfaProduct baseline disagrees with the corpus's known verdict";
  EXPECT_EQ(mtdfa_realizable, expected_realizable)
      << "MtdfaProduct disagrees with the corpus's known verdict --- a "
         "turn-order error in decision 2 is the first suspect";
}

// ---------------------------------------------------------------------------
// Tables A-D: known-input Tin corpus (mirrors tests/ltlfsynt_oracle_test.cpp
// BuildKnownInputCorpus; partition input_free=a, input_known=k,
// output_free=o).  Tin builders reproduce the same four HOA+lambda fixtures
// (ConstTrue/ConstFalse/Copy/Delay) directly via OutputLabeledTransducer,
// rather than via the HOA text + parse_transducer round trip that file uses
// --- both denote the identical delta/lambda, only the construction path
// differs.
// ---------------------------------------------------------------------------

VariablePartition KnownInputVars() {
  return VariablePartition::split({"a", "k"}, {"o"}, /*governed=*/{"k"});
}

OutputLabeledTransducer TinConstTrue(const spot::bdd_dict_ptr& dict) {
  auto g = spot::make_twa_graph(dict);
  const int a = g->register_ap("a");
  const int k = g->register_ap("k");
  (void)a;
  g->new_states(1);
  g->set_init_state(0);
  g->new_edge(0, 0, bddtrue);
  return OutputLabeledTransducer(g, {bdd_ithvar(k)}, /*sigma0=*/bdd_ithvar(a),
                                 /*sigma1=*/bdd_ithvar(k));
}

OutputLabeledTransducer TinConstFalse(const spot::bdd_dict_ptr& dict) {
  auto g = spot::make_twa_graph(dict);
  const int a = g->register_ap("a");
  const int k = g->register_ap("k");
  g->new_states(1);
  g->set_init_state(0);
  g->new_edge(0, 0, bddtrue);
  return OutputLabeledTransducer(g, {bdd_nithvar(k)}, /*sigma0=*/bdd_ithvar(a),
                                 /*sigma1=*/bdd_ithvar(k));
}

OutputLabeledTransducer TinCopy(const spot::bdd_dict_ptr& dict) {
  auto g = spot::make_twa_graph(dict);
  const int a = g->register_ap("a");
  const int k = g->register_ap("k");
  g->new_states(1);
  g->set_init_state(0);
  g->new_edge(0, 0, bddtrue);
  const bdd relation =
      (bdd_ithvar(a) & bdd_ithvar(k)) | (bdd_nithvar(a) & bdd_nithvar(k));
  return OutputLabeledTransducer(g, {relation}, /*sigma0=*/bdd_ithvar(a),
                                 /*sigma1=*/bdd_ithvar(k));
}

// One-step delay: k_t = a_{t-1}, k_0 = false; 2-state (mirrors kTinDelay).
OutputLabeledTransducer TinDelay(const spot::bdd_dict_ptr& dict) {
  auto g = spot::make_twa_graph(dict);
  const int a = g->register_ap("a");
  const int k = g->register_ap("k");
  g->new_states(2);
  g->set_init_state(0);
  g->new_edge(0, 1, bdd_ithvar(a));
  g->new_edge(0, 0, bdd_nithvar(a));
  g->new_edge(1, 1, bdd_ithvar(a));
  g->new_edge(1, 0, bdd_nithvar(a));
  return OutputLabeledTransducer(g, {bdd_nithvar(k), bdd_ithvar(k)},
                                 /*sigma0=*/bdd_ithvar(a),
                                 /*sigma1=*/bdd_ithvar(k));
}

enum class TinKind { kConstTrue, kConstFalse, kCopy, kDelay };

OutputLabeledTransducer BuildTin(TinKind kind, const spot::bdd_dict_ptr& dict) {
  switch (kind) {
    case TinKind::kConstTrue:
      return TinConstTrue(dict);
    case TinKind::kConstFalse:
      return TinConstFalse(dict);
    case TinKind::kCopy:
      return TinCopy(dict);
    case TinKind::kDelay:
      return TinDelay(dict);
  }
  throw std::logic_error("BuildTin: unhandled TinKind");
}

struct KnownInputRow {
  std::string name;
  TinKind tin;
  std::string phi;
  bool realizable;
};

void PrintTo(const KnownInputRow& row, std::ostream* os) { *os << row.name; }

std::vector<KnownInputRow> BuildKnownInputCorpus() {
  return {
      // --- Table A: Tin const-true ---
      {"A_k", TinKind::kConstTrue, "k", true},
      {"A_XBang_k", TinKind::kConstTrue, "X[!] k", true},
      {"A_XBang_XBang_k", TinKind::kConstTrue, "X[!](X[!] k)", true},
      {"A_G_k", TinKind::kConstTrue, "G(k)", true},
      {"A_F_k", TinKind::kConstTrue, "F(k)", true},
      {"A_XBang_k_and_o", TinKind::kConstTrue, "X[!](k & o)", true},
      {"A_XBang_k_implies_o", TinKind::kConstTrue, "X[!](k -> o)", true},
      {"A_G_k_implies_o", TinKind::kConstTrue, "G(k -> o)", true},

      // --- Table B: Tin const-false ---
      {"B_not_k", TinKind::kConstFalse, "!k", true},
      {"B_G_not_k", TinKind::kConstFalse, "G(!k)", true},
      {"B_XBang_not_k", TinKind::kConstFalse, "X[!] !k", true},
      {"B_F_not_k", TinKind::kConstFalse, "F(!k)", true},
      {"B_XBang_k", TinKind::kConstFalse, "X[!] k", false},
      {"B_G_k", TinKind::kConstFalse, "G(k)", false},

      // --- Table C: Tin copy (k = a) ---
      {"C_XBang_k_iff_a", TinKind::kCopy, "X[!](k <-> a)", true},
      {"C_G_a_implies_k", TinKind::kCopy, "G(a -> k)", true},
      {"C_G_k_implies_a", TinKind::kCopy, "G(k -> a)", true},
      {"C_G_o_iff_k", TinKind::kCopy, "G(o <-> k)", true},
      {"C_XBang_a_and_k", TinKind::kCopy, "X[!](a & k)", false},
      {"C_F_k_and_not_a", TinKind::kCopy, "F(k & !a)", false},

      // --- Table D: Tin one-step delay ---
      {"D_XBang_k", TinKind::kDelay, "X[!] k", false},
      {"D_G_a_implies_XBang_k", TinKind::kDelay, "G(a -> X[!] k)", false},
      {"D_F_k", TinKind::kDelay, "F(k)", false},
      {"D_XBang_XBang_k", TinKind::kDelay, "X[!](X[!] k)", false},
      {"D_k", TinKind::kDelay, "k", false},
      {"D_G_o_iff_k", TinKind::kDelay, "G(o <-> k)", true},
  };
}

class MtdfaKnownInputTest : public ::testing::TestWithParam<KnownInputRow> {};

TEST_P(MtdfaKnownInputTest, MatchesTheKnownInputCorpusVerdict) {
  const KnownInputRow& row = GetParam();
  auto dict = spot::make_bdd_dict();
  auto vars = KnownInputVars();
  register_turn_order_aps(vars, dict);
  const OutputLabeledTransducer t_in = BuildTin(row.tin, dict);
  const OutputLabeledTransducer t_out = trivial_transducer(vars, Role::t_out, dict);
  ExpectBothMethodsAgree(Phi(row.phi), vars, t_in, t_out, row.realizable);
}

INSTANTIATE_TEST_SUITE_P(
    KnownInputCorpus, MtdfaKnownInputTest,
    ::testing::ValuesIn(BuildKnownInputCorpus()),
    [](const ::testing::TestParamInfo<KnownInputRow>& info) {
      return info.param.name;
    });

// ---------------------------------------------------------------------------
// Table E: empty knowledge (V = empty) --- mirrors
// tests/ltlfsynt_oracle_test.cpp BuildEmptyKnowledgeCorpus, including the two
// Mealy-only payoff rows (o <-> i, G(o <-> i)) the PRD calls out by name:
// "exactly what would catch a turn-order error in decision 2".  For V =
// empty the turn-order-registered order is identical to the plain
// Ifree-then-Ofree order (PRD "src/ltlf_ek_synth.cpp:344-345" comment), so
// these rows also serve as a regression that register_turn_order_aps is
// inert (not merely harmless) on an empty-knowledge partition.
// ---------------------------------------------------------------------------

VariablePartition EmptyKnowledgeVars() {
  return VariablePartition::split({"i"}, {"o"}, /*governed=*/{});
}

OutputLabeledTransducer Trivial(const VariablePartition& vars, Role role,
                                const spot::bdd_dict_ptr& dict) {
  return trivial_transducer(vars, role, dict);
}

struct EmptyKnowledgeRow {
  std::string name;
  std::string phi;
  bool realizable;
};

void PrintTo(const EmptyKnowledgeRow& row, std::ostream* os) {
  *os << row.name;
}

std::vector<EmptyKnowledgeRow> BuildEmptyKnowledgeCorpus() {
  return {
      {"o", "o", true},
      {"zero", "0", false},
      {"one", "1", true},
      {"i", "i", false},
      {"G_i_implies_o", "G(i -> o)", true},
      {"XBang_i", "X[!] i", false},
      {"XBang_o", "X[!] o", true},
      {"F_o", "F o", true},
      {"G_i", "G i", false},
      {"o_U_i", "o U i", false},
      {"i_U_o", "i U o", true},
      {"G_o_or_i", "G(o) | i", true},
      {"o_iff_i", "o <-> i", true},       // Mealy-only.
      {"G_o_iff_i", "G(o <-> i)", true},  // Mealy-only.
  };
}

class MtdfaEmptyKnowledgeTest
    : public ::testing::TestWithParam<EmptyKnowledgeRow> {};

TEST_P(MtdfaEmptyKnowledgeTest, MatchesTheEmptyKnowledgeCorpusVerdict) {
  const EmptyKnowledgeRow& row = GetParam();
  auto dict = spot::make_bdd_dict();
  auto vars = EmptyKnowledgeVars();
  register_turn_order_aps(vars, dict);
  const OutputLabeledTransducer t_in = Trivial(vars, Role::t_in, dict);
  const OutputLabeledTransducer t_out = Trivial(vars, Role::t_out, dict);
  ExpectBothMethodsAgree(Phi(row.phi), vars, t_in, t_out, row.realizable);
}

INSTANTIATE_TEST_SUITE_P(
    EmptyKnowledgeCorpus, MtdfaEmptyKnowledgeTest,
    ::testing::ValuesIn(BuildEmptyKnowledgeCorpus()),
    [](const ::testing::TestParamInfo<EmptyKnowledgeRow>& info) {
      return info.param.name;
    });

// ---------------------------------------------------------------------------
// phi = 1 / phi = 0 collapse (PRD "Edge cases", Phase-0-confirmed regression
// fixtures): ltlf_to_mtdfa's detect_empty_univ may collapse these to a
// single bddtrue/bddfalse root before any product happens; spot::product
// must accept a collapsed operand in either argument position, and
// solve_mtdfa's Q3 test then reports realizable / unrealizable respectively.
// Already covered by the "one"/"zero" rows above (empty knowledge); these two
// are kept as their own named regressions per the task brief's instruction
// to land the phi=1/phi=0 findings as dedicated fixtures, independent of the
// corpus's row-name churn.
// ---------------------------------------------------------------------------

TEST(MtdfaProduct, FormulaOneCollapsesToASingleBddtrueRootAndIsRealizable) {
  auto dict = spot::make_bdd_dict();
  auto vars = EmptyKnowledgeVars();
  register_turn_order_aps(vars, dict);
  const OutputLabeledTransducer t_in = Trivial(vars, Role::t_in, dict);
  const OutputLabeledTransducer t_out = Trivial(vars, Role::t_out, dict);
  MtdfaProduct method;
  EXPECT_TRUE(method.synthesize(Phi("1"), vars, t_in, t_out).has_value());
}

TEST(MtdfaProduct, FormulaZeroCollapsesToASingleBddfalseRootAndIsUnrealizable) {
  auto dict = spot::make_bdd_dict();
  auto vars = EmptyKnowledgeVars();
  register_turn_order_aps(vars, dict);
  const OutputLabeledTransducer t_in = Trivial(vars, Role::t_in, dict);
  const OutputLabeledTransducer t_out = Trivial(vars, Role::t_out, dict);
  MtdfaProduct method;
  EXPECT_FALSE(method.synthesize(Phi("0"), vars, t_in, t_out).has_value());
}

// A tautologically-false conjunction (phi = a & !a) is a second route to the
// same bddfalse-collapse (PRD "Edge cases": "phi = 0 and phi = a /\ !a to a
// single bddfalse root").
TEST(MtdfaProduct, TautologicallyFalseConjunctionIsUnrealizable) {
  auto dict = spot::make_bdd_dict();
  auto vars = VariablePartition::split({"i"}, {"o"}, /*governed=*/{});
  register_turn_order_aps(vars, dict);
  const OutputLabeledTransducer t_in = Trivial(vars, Role::t_in, dict);
  const OutputLabeledTransducer t_out = Trivial(vars, Role::t_out, dict);
  MtdfaProduct method;
  EXPECT_FALSE(method.synthesize(Phi("i & !i"), vars, t_in, t_out).has_value());
}

// ---------------------------------------------------------------------------
// Controller shape (mirrors dfa_product_test.cpp
// RealizableControllerCarriesAStrategy): a realizable case's Controller
// carries a non-null strategy with at least one state.  Also confirms the
// Phase 0/Q4 follow-up contract indirectly: controller_as_transducer (used by
// the corpus's verify_controller round trip, tests/ltlfsynt_oracle_test.cpp)
// must accept an unsplit mealy without throwing --- exercised directly in
// tests/verify_controller_test.cpp, not re-derived here.
// ---------------------------------------------------------------------------

TEST(MtdfaProduct, RealizableControllerCarriesAStrategy) {
  auto dict = spot::make_bdd_dict();
  auto vars = EmptyKnowledgeVars();
  register_turn_order_aps(vars, dict);
  const OutputLabeledTransducer t_in = Trivial(vars, Role::t_in, dict);
  const OutputLabeledTransducer t_out = Trivial(vars, Role::t_out, dict);
  MtdfaProduct method;
  const std::optional<Controller> controller =
      method.synthesize(Phi("G(i -> o)"), vars, t_in, t_out);
  ASSERT_TRUE(controller.has_value());
  ASSERT_NE(controller->strategy, nullptr);
  EXPECT_GE(controller->strategy->num_states(), 1u);
}

}  // namespace
