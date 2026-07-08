#pragma once

#include <optional>
#include <vector>

#include <bddx.h>
#include <spot/tl/formula.hh>

#include "ltlf_ek/synthesis.hpp"
#include "ltlf_ek/transducer.hpp"
#include "ltlf_ek/variables.hpp"

namespace ltlf_ek {

// A concrete adversarial play the controller cannot win: the environment's
// letters from the virtual start into a not-F_phi cycle, or into a not-F_phi
// dead-end (cycle empty) --- docs/GLOSSARY.md "Controller verifier".
struct Witness {
  std::vector<bdd> prefix;  // agreeing letters, start -> cycle head / dead-end.
  std::vector<bdd> cycle;   // repeating not-F_phi loop; empty => dead-end.
};

struct VerifyResult {
  // True iff T_C solves def:probDefTransducer.  Deliberately redundant with
  // counterexample.has_value() (ok == !counterexample.has_value() always
  // holds) --- kept as an explicit verdict field so callers never have to
  // infer the verdict from optional-emptiness.
  bool ok;
  std::optional<Witness> counterexample;  // set iff !ok.
};

// Verify a controller (as a Role::t_c transducer) against the
// def:probDefTransducer postcondition (main.tex Sec.~129-131): every trace
// agreeing with T_in, T_out, T_C satisfies phi.  Reuses ltlf_to_dfa (A_phi)
// and consistent (cons); builds the product + attractor independently.
// NEVER calls solve_dfa / solve_game --- see docs/prd/controller-verifier.md
// "Behaviour / semantics" for the Bad nu-fixpoint this implements.
//
// Throws std::invalid_argument when an AP of phi is outside I∪O, or the
// automata/transducers do not share one bdd_dict (same policy as
// DfaProduct::synthesize).
VerifyResult verify_controller(const spot::formula& phi,
                               const VariablePartition& vars,
                               const Transducer& t_in, const Transducer& t_out,
                               const Transducer& t_c);

// Convenience overload: materialize a synthesized Controller as its Role::t_c
// transducer, then delegate.
VerifyResult verify_controller(const spot::formula& phi,
                               const VariablePartition& vars,
                               const Transducer& t_in, const Transducer& t_out,
                               const Controller& controller);

}  // namespace ltlf_ek
