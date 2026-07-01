#pragma once

#include <optional>

#include <spot/tl/formula.hh>
#include <spot/twa/twagraph.hh>

#include "ltlf_ek/transducer.hpp"
#include "ltlf_ek/variables.hpp"

namespace ltlf_ek {

// The synthesized controller T_C: a transducer with
//   lambda_C : Q × 2^{I} -> 2^{\Ofree}
// (main.tex, def:probDefTransducer).  Skeleton holds the strategy DFA; the
// lambda_C output map is filled in as methods are implemented.
class Controller final {
 public:
  spot::twa_graph_ptr strategy;  // delta_C (state history)
};

// Common interface for the five synthesis methods (main.tex §Methods 1-3.3).
// A method takes a Goal formula phi, the variable partition, and the two
// knowledge transducers T_in, T_out, and returns a controller T_C such that
// every trace agreeing with T_in, T_out, T_C satisfies phi --- or nullopt if
// no such controller exists (unrealizable).
class Synthesis {
 public:
  virtual ~Synthesis() = default;

  virtual std::optional<Controller> synthesize(const spot::formula& phi,
                                               const VariablePartition& vars,
                                               const Transducer& t_in,
                                               const Transducer& t_out) = 0;
};

}  // namespace ltlf_ek
