#include "ltlf_ek/variables.hpp"

#include <algorithm>
#include <iterator>

#include <spot/tl/apcollect.hh>

namespace ltlf_ek {
namespace {

std::set<std::string> set_diff(const std::set<std::string>& a,
                               const std::set<std::string>& b) {
  std::set<std::string> out;
  std::set_difference(a.begin(), a.end(), b.begin(), b.end(),
                      std::inserter(out, out.end()));
  return out;
}

std::set<std::string> set_inter(const std::set<std::string>& a,
                                const std::set<std::string>& b) {
  std::set<std::string> out;
  std::set_intersection(a.begin(), a.end(), b.begin(), b.end(),
                        std::inserter(out, out.end()));
  return out;
}

std::set<std::string> set_union(const std::set<std::string>& a,
                                const std::set<std::string>& b) {
  std::set<std::string> out;
  std::set_union(a.begin(), a.end(), b.begin(), b.end(),
                 std::inserter(out, out.end()));
  return out;
}

}  // namespace

VariablePartition VariablePartition::split(
    const std::set<std::string>& inputs, const std::set<std::string>& outputs,
    const std::set<std::string>& governed) {
  VariablePartition p;
  p.input_free = set_diff(inputs, governed);
  p.input_known = set_inter(inputs, governed);
  p.output_free = set_diff(outputs, governed);
  p.output_known = set_inter(outputs, governed);
  return p;
}

std::set<std::string> VariablePartition::known() const {
  return set_union(input_known, output_known);
}

std::set<std::string> VariablePartition::inputs() const {
  return set_union(input_free, input_known);
}

std::set<std::string> VariablePartition::outputs() const {
  return set_union(output_free, output_known);
}

std::set<std::string> collect_aps(const spot::formula& f) {
  std::set<std::string> names;
  spot::atomic_prop_set aps;
  spot::atomic_prop_collect(f, &aps);
  for (const auto& ap : aps) names.insert(ap.ap_name());
  return names;
}

}  // namespace ltlf_ek
