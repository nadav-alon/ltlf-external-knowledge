#include <iostream>
#include <set>
#include <string>

#include <gtest/gtest.h>
#include <spot/tl/parse.hh>

#include "ltlf_ek/variables.hpp"

namespace {

using ltlf_ek::collect_aps;
using ltlf_ek::VariablePartition;
using StrSet = std::set<std::string>;

TEST(VariablePartition, SplitByGovernedSet) {
  const StrSet inputs{"i0", "i1"};
  const StrSet outputs{"o0", "o1"};
  const StrSet governed{"i1", "o1"};  // V = \Iknown ∪ \Oknown

  const auto p = VariablePartition::split(inputs, outputs, governed);

  EXPECT_EQ(p.input_free, (StrSet{"i0"}));
  EXPECT_EQ(p.input_known, (StrSet{"i1"}));
  EXPECT_EQ(p.output_free, (StrSet{"o0"}));
  EXPECT_EQ(p.output_known, (StrSet{"o1"}));
  EXPECT_EQ(p.known(), governed);
  EXPECT_EQ(p.inputs(), inputs);
  EXPECT_EQ(p.outputs(), outputs);
}

TEST(CollectAps, ReturnsFormulaAtomicPropositions) {
  auto pf = spot::parse_infix_psl("G(i0 -> F o0)");
  ASSERT_FALSE(pf.format_errors(std::cerr));

  EXPECT_EQ(collect_aps(pf.f), (StrSet{"i0", "o0"}));
}

}  // namespace
