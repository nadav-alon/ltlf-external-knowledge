#include <random>
#include <string>
#include <gtest/gtest.h>
#include "ltlf_ek/window_source.hpp"

namespace ltlf_ek {
namespace {

int Count(const std::string& text, const std::string& needle) {
  int n = 0;
  for (std::size_t at = text.find(needle); at != std::string::npos;
       at = text.find(needle, at + 1))
    ++n;
  return n;
}

TEST(WindowSource, WeakNextNestsXAroundTheInner) {
  EXPECT_EQ(weak_next(0, "k"), "k");
  EXPECT_EQ(weak_next(3, "!k"), "X(X(X(!k)))");
}

TEST(WindowSource, DelayTableReadsTheFreeInputDepthStepsBack) {
  for (int d = 1; d <= 3; ++d) {
    const WindowSource src = delay_window_source(d);
    EXPECT_EQ(src.kind, WindowKind::kDelay);
    EXPECT_EQ(src.free_inputs, 1);
    EXPECT_EQ(src.depth, d);
    ASSERT_EQ(src.table.size(), 1u << (d + 1));
    for (unsigned idx = 0; idx < src.table.size(); ++idx)
      EXPECT_EQ(src.table[idx], ((idx >> d) & 1u) != 0) << "d=" << d;
  }
}

TEST(WindowSource, TinFileHasTwoToTheMTimesDStates) {
  for (int d = 1; d <= 3; ++d) {
    const std::string text = window_tin_file(delay_window_source(d));
    EXPECT_NE(text.find("States: " + std::to_string(1 << d) + "\n"),
              std::string::npos);
    EXPECT_EQ(Count(text, "State: "), 1 << d);
  }
}

TEST(WindowSource, DelayDepthOnePsiInIsTheGuardedWeakXShape) {
  const std::string psi = window_psi_in(delay_window_source(1));
  // d = 1: one head clause of length 1, then G of the length-2 clause.
  EXPECT_EQ(Count(psi, "G("), 1);
  EXPECT_EQ(Count(psi, "->"), 2 + 4);
  EXPECT_EQ(psi.find("X[!]"), std::string::npos);
}

TEST(WindowSource, PsiInClauseCountFollowsTheWindowOfTheDrawnSource) {
  std::mt19937 rng(7);
  for (int i = 0; i < 20; ++i) {
    const WindowSource src = draw_window_source(rng);
    const int m = src.free_inputs, d = src.depth;
    int clauses = 1 << (m * (d + 1));
    for (int s = 0; s < d; ++s) clauses += 1 << (m * (s + 1));
    EXPECT_EQ(Count(window_psi_in(src), "->"), clauses)
        << WindowKindName(src.kind) << " m=" << m << " d=" << d;
    EXPECT_EQ(src.table.size(), 1u << (m * (d + 1)));
  }
}

TEST(WindowSource, KindNamesAreTheLowercaseMemberNames) {
  EXPECT_STREQ(WindowKindName(WindowKind::kConst), "const");
  EXPECT_STREQ(WindowKindName(WindowKind::kCopy), "copy");
  EXPECT_STREQ(WindowKindName(WindowKind::kDelay), "delay");
  EXPECT_STREQ(WindowKindName(WindowKind::kWindow), "window");
  EXPECT_EQ(free_input_name(3), "a3");
}

}  // namespace
}  // namespace ltlf_ek
