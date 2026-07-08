#include "ltlf_ek/consistency.hpp"

#include <optional>

namespace ltlf_ek {

// emits(t, q, v): the per-transducer lambda-agreement atom (one conjunct of
// cons, def:consistency).  lambda returns a cube over its Sigma1; v is a full
// letter (a minterm over I∪O), so it lies in that cube --- i.e. its Sigma1
// slice equals the committed output --- exactly when (v & out) != false.  This
// makes the check Sigma1-agnostic: it never needs to know which variables
// Iknown/Oknown are, only that the transducer commits to a cube v must agree
// with.  A nullopt output (undefined lambda) is non-enabled, hence false
// (def:consistency partiality note, main.tex §211).
bool emits(const Transducer& t, unsigned q, bdd v) {
  std::optional<bdd> out = t.lambda(q, v);
  if (!out) return false;
  return (v & *out) != bddfalse;
}

// cons(q_in, q_out, v) := (v ∩ Iknown = lambda_in(q_in, v))
//                       ∧ (v ∩ Oknown = lambda_out(q_out, v))   (def:consistency, main.tex §203)
//                       =  emits(t_in, q_in, v) && emits(t_out, q_out, v).
bool consistent(const Transducer& t_in, unsigned q_in, const Transducer& t_out,
                unsigned q_out, bdd v) {
  return emits(t_in, q_in, v) && emits(t_out, q_out, v);
}

}  // namespace ltlf_ek
