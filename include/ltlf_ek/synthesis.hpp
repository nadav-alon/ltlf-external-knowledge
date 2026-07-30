#pragma once

#include <optional>

#include <spot/tl/formula.hh>
#include <spot/twa/twagraph.hh>

#include "ltlf_ek/output_labeled_transducer.hpp"
#include "ltlf_ek/transducer.hpp"
#include "ltlf_ek/variables.hpp"

namespace ltlf_ek {

// The synthesized controller T_C: a transducer with
//   lambda_C : Q × 2^{I} -> 2^{\Ofree}
// (main.tex, def:probDefTransducer).  Holds only the strategy DFA (delta_C,
// state history); lambda_C is materialized on demand by
// controller_as_transducer, below --- a plain struct: one public member, no
// invariants to enforce.
struct Controller {
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

// Materialize a synthesized Controller's strategy graph as a Role::t_c
// OutputLabeledTransducer: Sigma0 = I, Sigma1 = Ofree (main.tex:130,
// docs/GLOSSARY.md "Controller-as-transducer view").  lambda_C is read off
// the Mealy strategy edges (the union of a state's out-edge guards, already
// a relation over Ifree x Ofree); delta_C off the edge destinations --- the
// same "delta via edges, output derived" idiom OutputLabeledTransducer uses.
OutputLabeledTransducer controller_as_transducer(const Controller& controller,
                                                 const VariablePartition& vars);

}  // namespace ltlf_ek
