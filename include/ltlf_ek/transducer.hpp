#pragma once

#include <bddx.h>
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
class Transducer {
 public:
  virtual ~Transducer() = default;

  virtual unsigned initial_state() const = 0;

  // delta(q, v): successor of q under the full letter v (a cube over I∪O).
  virtual unsigned delta(unsigned q, bdd v) const = 0;

  // lambda(q, visible): the Sigma1-valued output committed at state q given the
  // Sigma0 slice `visible` this transducer is allowed to observe.  Returns a
  // cube over Sigma1.
  virtual bdd lambda(unsigned q, bdd visible) const = 0;
};

}  // namespace ltlf_ek
