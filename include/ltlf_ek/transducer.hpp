#pragma once

#include <optional>

#include <bddx.h>
#include <spot/twa/bdddict.hh>
#include <spot/twa/twagraph.hh>

namespace ltlf_ek {

// A transducer tau = (Q, Sigma, delta, lambda, q0) that "implements" a strategy
// (main.tex, Transducers subsection).  delta is a standard deterministic
// transition over the full alphabet Sigma = 2^{I∪O} and tracks the whole state
// history; lambda is the *non-standard* output function
//
//     lambda : Q × Sigma0 -> Sigma1        (Sigma0, Sigma1 ⊆ Sigma)
//
// which lets each party see only the variables allowed by the turn order.
// This lambda-split shape has no native Spot equivalent, so we model it
// explicitly.  See docs/GLOSSARY.md ("transducer", "lambda").
//
// Both delta and lambda are *partial* (main.tex §107, \cref{def:enabled}):
// either may be undefined at an argument, signalled by `std::nullopt`.  A letter
// is *enabled* at a pair of states iff delta and lambda are defined at it on
// both transducers AND `consistent(...)` holds; a nullopt from delta OR lambda
// makes the letter non-enabled, treated exactly like a consistency failure
// (skipped, as in Methods 1/3).  The `enabled` predicate therefore also guards
// delta: callers must only apply delta on a letter that already passed the
// enabled test.
class Transducer {
 public:
  virtual ~Transducer() = default;

  virtual unsigned initial_state() const = 0;

  // The shared spot::bdd_dict this transducer's delta guards and lambda cubes
  // live on.  Every product / consistency computation must be carried out on one
  // dict (see the precondition in consistency.hpp / output_labeled_transducer.hpp)
  // --- exposed here so a Synthesis method can build its Goal DFA on the same
  // dict as T_in, T_out.  This is BuDDy/Spot infrastructure, not a domain
  // concept, hence no glossary entry.
  virtual spot::bdd_dict_ptr dict() const = 0;

  // delta(q, v): successor of q under the full letter v (a cube over I∪O).
  // nullopt = undefined (partial transducer, main.tex §107).
  virtual std::optional<unsigned> delta(unsigned q, bdd v) const = 0;

  // lambda(q, v): the Sigma1-valued output committed at state q.  Passed the
  // *full* letter v (abuse-of-notation, main.tex §87); the implementation reads
  // only its Sigma0 slice and returns a cube over Sigma1.  nullopt = undefined.
  virtual std::optional<bdd> lambda(unsigned q, bdd v) const = 0;
};

}  // namespace ltlf_ek
