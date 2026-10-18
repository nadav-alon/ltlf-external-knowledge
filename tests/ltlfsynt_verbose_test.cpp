#include <fstream>
#include <optional>
#include <sstream>
#include <string>

#include <gtest/gtest.h>

#include "ltlf_ek/ltlfsynt_verbose.hpp"

#ifndef LTLF_EK_TEST_FIXTURES_DIR
#define LTLF_EK_TEST_FIXTURES_DIR "tests/fixtures"
#endif

namespace {

using ltlf_ek::parse_ltlfsynt_verbose;
using ltlf_ek::parse_ltlfsynt_verdict;

std::string read_fixture(const std::string& relative_path) {
  std::ifstream in(std::string(LTLF_EK_TEST_FIXTURES_DIR) + "/" + relative_path);
  EXPECT_TRUE(in) << relative_path;
  std::ostringstream ss;
  ss << in.rdbuf();
  return ss.str();
}

// The two `--realizability` transcripts are `ltlfsynt --verbose` of Spot
// 2.15.1, one per verdict (the realizable one also prints the signal-removal
// lines); the only stage-time line that version prints in either is the MTDFA
// translation. The `synthesis-*` transcripts below are full synthesis.
TEST(LtlfsyntVerbose, SpotTranscriptYieldsTheTranslationTimeAndRoots) {
  const auto v = parse_ltlfsynt_verbose(read_fixture("ltlfsynt/verbose-2.15.1.txt"));
  ASSERT_TRUE(v.internal_ns);
  EXPECT_EQ(*v.internal_ns, 652407u);
  ASSERT_TRUE(v.explored_roots);
  EXPECT_EQ(*v.explored_roots, 1u);
}

TEST(LtlfsyntVerbose, RealizableSpotTranscriptYieldsTheTranslationTimeAndRoots) {
  const auto v =
      parse_ltlfsynt_verbose(read_fixture("ltlfsynt/verbose-realizable-2.15.1.txt"));
  ASSERT_TRUE(v.internal_ns);
  EXPECT_EQ(*v.internal_ns, 3547820u);
  ASSERT_TRUE(v.explored_roots);
  EXPECT_EQ(*v.explored_roots, 1u);
}

// Full synthesis (no --realizability) adds a strategy-extraction stage when
// the spec is realizable: "Mealy machine (N states) created in S seconds".
TEST(LtlfsyntVerbose, SynthesisTranscriptSumsTheStrategyExtractionStage) {
  const auto v = parse_ltlfsynt_verbose(
      read_fixture("ltlfsynt/synthesis-realizable-2.15.1.stderr.txt"));
  ASSERT_TRUE(v.internal_ns);
  EXPECT_EQ(*v.internal_ns, 627116u + 3259u);
  EXPECT_EQ(*v.explored_roots, 1u);
}

TEST(LtlfsyntVerbose, UnrealizableSynthesisTranscriptHasOnlyTheTranslationTime) {
  const auto v = parse_ltlfsynt_verbose(
      read_fixture("ltlfsynt/synthesis-unrealizable-2.15.1.stderr.txt"));
  ASSERT_TRUE(v.internal_ns);
  EXPECT_EQ(*v.internal_ns, 666867u);
  EXPECT_EQ(*v.explored_roots, 1u);
}

TEST(LtlfsyntVerbose, StageTimesAreSummed) {
  const auto v = parse_ltlfsynt_verbose(
      "translation to MTDFA (7 roots, 0 APs) took 0.5 seconds\n"
      "simplification took 0.25 seconds\n");
  ASSERT_TRUE(v.internal_ns);
  EXPECT_EQ(*v.internal_ns, 750000000u);
  EXPECT_EQ(*v.explored_roots, 7u);
}

TEST(LtlfsyntVerbose, EmptyTranscriptHasNothingNotZero) {
  const auto v = parse_ltlfsynt_verbose("");
  EXPECT_FALSE(v.internal_ns);
  EXPECT_FALSE(v.explored_roots);
}

TEST(LtlfsyntVerbose, TranscriptWithoutStageLinesHasNothingNotZero) {
  const auto v = parse_ltlfsynt_verbose("using Mealy semantics\nREALIZABLE\n");
  EXPECT_FALSE(v.internal_ns);
  EXPECT_FALSE(v.explored_roots);
}

TEST(LtlfsyntVerbose, UnparseableStageTimeHasNoInternalTime) {
  const auto v = parse_ltlfsynt_verbose(
      "translation to MTDFA (3 roots, 0 APs) took soon seconds\n");
  EXPECT_FALSE(v.internal_ns);
  EXPECT_EQ(*v.explored_roots, 3u);
}

TEST(LtlfsyntVerbose, OneUnparseableStageTimeSpoilsTheSum) {
  const auto v = parse_ltlfsynt_verbose(
      "translation to MTDFA (3 roots, 0 APs) took 0.1 seconds\n"
      "simplification took -- seconds\n");
  EXPECT_FALSE(v.internal_ns);
}

TEST(LtlfsyntVerbose, StageLineCutAfterTookSpoilsTheSum) {
  const auto v = parse_ltlfsynt_verbose(
      "translation to MTDFA (3 roots, 0 APs) took 0.1 seconds\n"
      "simplification took 0.00");
  EXPECT_FALSE(v.internal_ns);
}

TEST(LtlfsyntVerbose, FoundInStageLineIsSummed) {
  const auto v = parse_ltlfsynt_verbose(
      "translation to MTDFA (3 roots, 0 APs) took 0.1 seconds\n"
      "strategy (4 roots) found in 0.25 seconds\n");
  ASSERT_TRUE(v.internal_ns);
  EXPECT_EQ(*v.internal_ns, 350000000u);
}

TEST(LtlfsyntVerbose, StageLineMissingItsUnitSpoilsTheSum) {
  const auto v = parse_ltlfsynt_verbose(
      "translation to MTDFA (3 roots, 0 APs) took 0.1 seconds\n"
      "simplification took 0.25\n");
  EXPECT_FALSE(v.internal_ns);
}

TEST(LtlfsyntVerbose, TranscriptCutInsideTheStageLineHasNothingNotZero) {
  const auto v = parse_ltlfsynt_verbose("translation to MTDFA (3 ro");
  EXPECT_FALSE(v.internal_ns);
  EXPECT_FALSE(v.explored_roots);
}

TEST(LtlfsyntVerdict, RealizableSynthesisStdoutCarriesAStrategyAfterTheVerdict) {
  EXPECT_EQ(parse_ltlfsynt_verdict(
                0, read_fixture("ltlfsynt/synthesis-realizable-2.15.1.stdout.txt")),
            std::optional<bool>(true));
}

TEST(LtlfsyntVerdict, UnrealizableSynthesisStdoutIsJustTheVerdict) {
  EXPECT_EQ(parse_ltlfsynt_verdict(
                1, read_fixture("ltlfsynt/synthesis-unrealizable-2.15.1.stdout.txt")),
            std::optional<bool>(false));
}

TEST(LtlfsyntVerdict, AVerdictWordWithTheWrongExitCodeIsNoVerdict) {
  EXPECT_FALSE(parse_ltlfsynt_verdict(1, "REALIZABLE\nHOA: v1\n"));
  EXPECT_FALSE(parse_ltlfsynt_verdict(0, "UNREALIZABLE\n"));
}

TEST(LtlfsyntVerdict, NoVerdictWordIsNoVerdict) {
  EXPECT_FALSE(parse_ltlfsynt_verdict(0, ""));
  EXPECT_FALSE(parse_ltlfsynt_verdict(0, "HOA: v1\n"));
}

}  // namespace
