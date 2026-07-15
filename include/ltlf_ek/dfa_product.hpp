#pragma once

#include "ltlf_ek/synthesis.hpp"

namespace ltlf_ek {

// Method 2 --- DFA product (main.tex §fulldfa, Algorithm "DFA Product").
// Constructs the full DFA A for phi, forms the product with T_in, T_out while
// skipping letters that disagree with the external knowledge (¬cons) --- as
// in Methods 1/3 (def:consistency) --- then solves the resulting game directly.
//
// This is the first method implemented: it is the simplest complete
// end-to-end path and the "good comparison point" of main.tex.
class DfaProduct final : public Synthesis {
 public:
  std::optional<Controller> synthesize(const spot::formula& phi,
                                       const VariablePartition& vars,
                                       const Transducer& t_in,
                                       const Transducer& t_out) override;
};

}  // namespace ltlf_ek
