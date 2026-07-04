#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>

#include <gtest/gtest.h>
#include <bddx.h>
#include <spot/tl/parse.hh>
#include <spot/twa/bdddict.hh>
#include <spot/twa/twagraph.hh>

#include "ltlf_ek/cli.hpp"
#include "ltlf_ek/consistency.hpp"
#include "ltlf_ek/dfa_product.hpp"
#include "ltlf_ek/output_labeled_transducer.hpp"
#include "ltlf_ek/synthesis.hpp"
#include "ltlf_ek/transducer_io.hpp"
#include "ltlf_ek/variables.hpp"

// Unit fixtures for the three library-level pieces of the CLI wrapper
// (docs/prd/cli-wrapper.md "Interfaces & types"): parse_partition_file,
// trivial_transducer, make_synthesis_method.  End-to-end / subprocess
// coverage of the ltlf-ek-synth binary lives in tests/ltlf_ek_synth_test.cpp.
namespace {

using ltlf_ek::consistent;
using ltlf_ek::DfaProduct;
using ltlf_ek::make_synthesis_method;
using ltlf_ek::OutputLabeledTransducer;
using ltlf_ek::parse_partition_file;
using ltlf_ek::Role;
using ltlf_ek::Synthesis;
using ltlf_ek::trivial_transducer;
using ltlf_ek::VariablePartition;

VariablePartition Parse(const std::string& text) {
  std::istringstream in(text);
  return parse_partition_file(in);
}

// ---------------------------------------------------------------------------
// parse_partition_file --- round-trip on a well-formed file.
// ---------------------------------------------------------------------------

TEST(ParsePartitionFile, ReadsAllFourKeys) {
  const VariablePartition p = Parse(
      "input_free:   a b\n"
      "input_known:  c\n"
      "output_free:  x\n"
      "output_known: y\n");
  EXPECT_EQ(p.input_free, (std::set<std::string>{"a", "b"}));
  EXPECT_EQ(p.input_known, (std::set<std::string>{"c"}));
  EXPECT_EQ(p.output_free, (std::set<std::string>{"x"}));
  EXPECT_EQ(p.output_known, (std::set<std::string>{"y"}));
}

TEST(ParsePartitionFile, BlankLinesAndCommentsAreIgnored) {
  const VariablePartition p = Parse(
      "# a leading comment\n"
      "\n"
      "input_free: a   # trailing comment\n"
      "\n"
      "# another comment\n"
      "output_free: x\n");
  EXPECT_EQ(p.input_free, (std::set<std::string>{"a"}));
  EXPECT_EQ(p.output_free, (std::set<std::string>{"x"}));
  EXPECT_TRUE(p.input_known.empty());
  EXPECT_TRUE(p.output_known.empty());
}

TEST(ParsePartitionFile, AWholeLineCommentIsSkippedEntirely) {
  const VariablePartition p = Parse("# input_free: a\noutput_free: x\n");
  EXPECT_TRUE(p.input_free.empty());
  EXPECT_EQ(p.output_free, (std::set<std::string>{"x"}));
}

TEST(ParsePartitionFile, MissingKeyIsEmptySet) {
  const VariablePartition p = Parse("input_free: a\noutput_free: x\n");
  EXPECT_TRUE(p.input_known.empty());
  EXPECT_TRUE(p.output_known.empty());
}

TEST(ParsePartitionFile, EmptyValueIsEmptySet) {
  const VariablePartition p = Parse("input_free:\noutput_free: x\n");
  EXPECT_TRUE(p.input_free.empty());
  EXPECT_EQ(p.output_free, (std::set<std::string>{"x"}));
}

TEST(ParsePartitionFile, EmptyFileGivesAFullyEmptyPartition) {
  const VariablePartition p = Parse("");
  EXPECT_TRUE(p.input_free.empty());
  EXPECT_TRUE(p.input_known.empty());
  EXPECT_TRUE(p.output_free.empty());
  EXPECT_TRUE(p.output_known.empty());
}

TEST(ParsePartitionFile, WhitespaceAroundKeyAndValueIsTrimmed) {
  const VariablePartition p = Parse("  input_free  :   a    b  \n");
  EXPECT_EQ(p.input_free, (std::set<std::string>{"a", "b"}));
}

TEST(ParsePartitionFile, MultipleApsAreSpaceSeparated) {
  const VariablePartition p = Parse("output_known: y1 y2 y3\n");
  EXPECT_EQ(p.output_known, (std::set<std::string>{"y1", "y2", "y3"}));
}

// --- malformed input: every case throws std::invalid_argument -------------

TEST(ParsePartitionFile, ThrowsOnLineWithoutColon) {
  EXPECT_THROW(Parse("input_free a b\n"), std::invalid_argument);
}

TEST(ParsePartitionFile, ThrowsOnUnknownKey) {
  EXPECT_THROW(Parse("input_maybe: a\n"), std::invalid_argument);
}

TEST(ParsePartitionFile, ThrowsOnDuplicateKeyAcrossLines) {
  EXPECT_THROW(Parse("input_free: a\ninput_free: b\n"),
               std::invalid_argument);
}

TEST(ParsePartitionFile, ThrowsOnApListedInTwoSets) {
  // 'a' claimed by both input_free and output_free --- violates disjointness.
  EXPECT_THROW(Parse("input_free: a\noutput_free: a\n"),
               std::invalid_argument);
}

TEST(ParsePartitionFile, ThrowsOnApSharedBetweenKnownAndFreeOfSameKind) {
  EXPECT_THROW(Parse("input_free: a\ninput_known: a\n"),
               std::invalid_argument);
}

// ---------------------------------------------------------------------------
// trivial_transducer --- single state, delta self-loops, lambda = bddtrue.
// ---------------------------------------------------------------------------

TEST(TrivialTransducer, IsASingleInitialState) {
  auto dict = spot::make_bdd_dict();
  auto part = VariablePartition::split({"i"}, {"o"}, /*governed=*/{});
  const OutputLabeledTransducer t = trivial_transducer(part, Role::t_in, dict);
  EXPECT_EQ(t.initial_state(), 0u);
}

TEST(TrivialTransducer, DeltaSelfLoopsOnEveryLetter) {
  auto dict = spot::make_bdd_dict();
  auto probe = spot::make_twa_graph(dict);
  auto part = VariablePartition::split({"i"}, {"o"}, /*governed=*/{});
  const OutputLabeledTransducer t = trivial_transducer(part, Role::t_in, dict);
  const bdd iv = bdd_ithvar(probe->register_ap("i"));
  const bdd ov = bdd_ithvar(probe->register_ap("o"));
  EXPECT_EQ(t.delta(0, iv & ov), std::optional<unsigned>(0));
  EXPECT_EQ(t.delta(0, !iv & !ov), std::optional<unsigned>(0));
}

TEST(TrivialTransducer, LambdaCommitsTheEmptyCube) {
  auto dict = spot::make_bdd_dict();
  auto probe = spot::make_twa_graph(dict);
  auto part = VariablePartition::split({"i"}, {"o"}, /*governed=*/{});
  const OutputLabeledTransducer t = trivial_transducer(part, Role::t_in, dict);
  const bdd iv = bdd_ithvar(probe->register_ap("i"));
  EXPECT_EQ(t.lambda(0, iv), std::optional<bdd>(bddtrue));
  EXPECT_EQ(t.lambda(0, !iv), std::optional<bdd>(bddtrue));
}

TEST(TrivialTransducer, SigmaSlicesMatchRoleForTIn) {
  auto dict = spot::make_bdd_dict();
  auto probe = spot::make_twa_graph(dict);
  // Ifree = {a}, Iknown = ∅ (only role-empty known set is a legal trivial).
  auto part = VariablePartition::split({"a"}, {}, /*governed=*/{});
  const OutputLabeledTransducer t = trivial_transducer(part, Role::t_in, dict);
  EXPECT_EQ(t.sigma0_cube(), bdd_ithvar(probe->register_ap("a")));
  EXPECT_EQ(t.sigma1_cube(), bddtrue);
}

TEST(TrivialTransducer, SigmaSlicesMatchRoleForTOut) {
  auto dict = spot::make_bdd_dict();
  auto probe = spot::make_twa_graph(dict);
  // I = {a}, Ofree = {x}, Oknown = ∅.
  auto part = VariablePartition::split({"a"}, {"x"}, /*governed=*/{});
  const OutputLabeledTransducer t = trivial_transducer(part, Role::t_out, dict);
  EXPECT_EQ(t.sigma0_cube(), bdd_ithvar(probe->register_ap("a")) &
                                 bdd_ithvar(probe->register_ap("x")));
  EXPECT_EQ(t.sigma1_cube(), bddtrue);
}

TEST(TrivialTransducer, ThrowsWhenInputKnownIsNonEmptyForTIn) {
  auto dict = spot::make_bdd_dict();
  auto part = VariablePartition::split({"a"}, {}, /*governed=*/{"a"});
  EXPECT_THROW(trivial_transducer(part, Role::t_in, dict),
               std::invalid_argument);
}

TEST(TrivialTransducer, ThrowsWhenOutputKnownIsNonEmptyForTOut) {
  auto dict = spot::make_bdd_dict();
  auto part = VariablePartition::split({"a"}, {"x"}, /*governed=*/{"x"});
  EXPECT_THROW(trivial_transducer(part, Role::t_out, dict),
               std::invalid_argument);
}

// consistent(...) is trivially satisfied by two trivial transducers, for any
// letter --- the PRD's stated rationale for substituting Trivial when V = ∅
// (docs/prd/cli-wrapper.md "Behaviour" #4).
TEST(TrivialTransducer, MakesConsistentTriviallyTrueForAnyLetter) {
  auto dict = spot::make_bdd_dict();
  auto probe = spot::make_twa_graph(dict);
  auto part = VariablePartition::split({"i"}, {"o"}, /*governed=*/{});
  const OutputLabeledTransducer t_in = trivial_transducer(part, Role::t_in, dict);
  const OutputLabeledTransducer t_out =
      trivial_transducer(part, Role::t_out, dict);
  const bdd iv = bdd_ithvar(probe->register_ap("i"));
  const bdd ov = bdd_ithvar(probe->register_ap("o"));
  EXPECT_TRUE(consistent(t_in, 0, t_out, 0, iv & ov));
  EXPECT_TRUE(consistent(t_in, 0, t_out, 0, !iv & !ov));
  EXPECT_TRUE(consistent(t_in, 0, t_out, 0, iv & !ov));
}

// ---------------------------------------------------------------------------
// make_synthesis_method --- dispatch table.
// ---------------------------------------------------------------------------

TEST(MakeSynthesisMethod, DfaProductFlagBuildsADfaProduct) {
  std::unique_ptr<Synthesis> method = make_synthesis_method("dfa-product");
  ASSERT_NE(method, nullptr);
  EXPECT_NE(dynamic_cast<DfaProduct*>(method.get()), nullptr);
}

TEST(MakeSynthesisMethod, UnwiredMethodsThrowLogicError) {
  for (const std::string& flag : {"nfa-product", "otf-dfa-product",
                                  "otf-agg-product", "otf-dyn-agg-product"}) {
    SCOPED_TRACE(flag);
    EXPECT_THROW(make_synthesis_method(flag), std::logic_error);
  }
}

TEST(MakeSynthesisMethod, UnrecognisedNameThrowsInvalidArgument) {
  EXPECT_THROW(make_synthesis_method("not-a-method"), std::invalid_argument);
}

// The factory-built DfaProduct behaves identically to a directly-constructed
// one on the same inputs (the CLI adds no semantics of its own).
TEST(MakeSynthesisMethod, FactoryDfaProductAgreesWithDirectDfaProduct) {
  auto dict = spot::make_bdd_dict();
  auto part = VariablePartition::split({"i"}, {"o"}, /*governed=*/{});
  const OutputLabeledTransducer t_in = trivial_transducer(part, Role::t_in, dict);
  const OutputLabeledTransducer t_out =
      trivial_transducer(part, Role::t_out, dict);
  const spot::formula phi = spot::parse_formula("G(i -> o)");

  std::unique_ptr<Synthesis> via_factory = make_synthesis_method("dfa-product");
  DfaProduct direct;
  EXPECT_EQ(via_factory->synthesize(phi, part, t_in, t_out).has_value(),
            direct.synthesize(phi, part, t_in, t_out).has_value());
}

}  // namespace
