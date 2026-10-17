#include <fstream>
#include <sstream>
#include <string>

#include <gtest/gtest.h>

#include "ltlf_ek/ltlfsynt_verbose.hpp"

#ifndef LTLF_EK_TEST_FIXTURES_DIR
#define LTLF_EK_TEST_FIXTURES_DIR "tests/fixtures"
#endif

namespace {

using ltlf_ek::parse_ltlfsynt_verbose;

std::string read_fixture(const std::string& relative_path) {
  std::ifstream in(std::string(LTLF_EK_TEST_FIXTURES_DIR) + "/" + relative_path);
  EXPECT_TRUE(in) << relative_path;
  std::ostringstream ss;
  ss << in.rdbuf();
  return ss.str();
}

// The transcript is `ltlfsynt --verbose` of Spot 2.15.1; the only stage-time
// line that version prints is the MTDFA translation.
TEST(LtlfsyntVerbose, SpotTranscriptYieldsTheTranslationTimeAndRoots) {
  const auto v = parse_ltlfsynt_verbose(read_fixture("ltlfsynt/verbose-2.15.1.txt"));
  ASSERT_TRUE(v.internal_ns);
  EXPECT_EQ(*v.internal_ns, 652407u);
  ASSERT_TRUE(v.explored_roots);
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

TEST(LtlfsyntVerbose, TranscriptCutInsideTheStageLineHasNothingNotZero) {
  const auto v = parse_ltlfsynt_verbose("translation to MTDFA (3 ro");
  EXPECT_FALSE(v.internal_ns);
  EXPECT_FALSE(v.explored_roots);
}

}  // namespace
