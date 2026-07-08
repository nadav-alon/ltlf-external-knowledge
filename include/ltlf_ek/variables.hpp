#pragma once

#include <set>
#include <string>

#include <spot/tl/formula.hh>

namespace ltlf_ek {

// Ubiquitous language --- see docs/GLOSSARY.md.
//   input_free   \Ifree   free inputs    --- environment decides
//   input_known  \Iknown  known inputs   --- produced by S_in / T_in
//   output_free  \Ofree   free outputs   --- produced by the controller S_C / T_C
//   output_known \Oknown  known outputs  --- produced by S_out / T_out
// with V = input_known ∪ output_known the externally-governed variables.
struct VariablePartition {
  std::set<std::string> input_free;    // \Ifree
  std::set<std::string> input_known;   // \Iknown
  std::set<std::string> output_free;   // \Ofree
  std::set<std::string> output_known;  // \Oknown

  std::set<std::string> known() const;    // V   = \Iknown ∪ \Oknown
  std::set<std::string> inputs() const;   // \mathcal{I}
  std::set<std::string> outputs() const;  // \mathcal{O}
  // Closed universe of APs (docs/GLOSSARY.md): \mathcal{I} ∪ \mathcal{O},
  // the set every AP (of phi, of a transducer file, of a lambda formula)
  // must lie in.
  std::set<std::string> universe() const;

  // Split the full input/output sets by the externally-governed subset
  // `governed` (= V). Anything in `governed` becomes *known*, the rest *free*.
  static VariablePartition split(const std::set<std::string>& inputs,
                                 const std::set<std::string>& outputs,
                                 const std::set<std::string>& governed);
};

// Names of the atomic propositions occurring in `f` (thin Spot helper).
std::set<std::string> collect_aps(const spot::formula& f);

}  // namespace ltlf_ek
