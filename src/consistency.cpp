#include "ltlf_ek/consistency.hpp"

#include <optional>

namespace ltlf_ek {

// cons(q_in, q_out, v) := (v ∩ Iknown = lambda_in(q_in, v))
//                       ∧ (v ∩ Oknown = lambda_out(q_out, v))   (main.tex §149).
//
// lambda returns a cube over its Sigma1 (Iknown for T_in, Oknown for T_out).
// v is a full letter (a minterm over I∪O), so it lies in that cube --- i.e. its
// Sigma1 slice equals the committed output --- exactly when (v & out) != false.
// This makes the check Sigma1-agnostic: `consistent` never needs to know which
// variables Iknown/Oknown are, only that the transducers commit to cubes v must
// agree with.  A nullopt output (undefined lambda) is non-enabled, hence not
// consistent (\cref{def:enabled}).
bool consistent(const Transducer& t_in, unsigned q_in, const Transducer& t_out,
                unsigned q_out, bdd v) {
  std::optional<bdd> out_in = t_in.lambda(q_in, v);
  if (!out_in) return false;
  std::optional<bdd> out_out = t_out.lambda(q_out, v);
  if (!out_out) return false;
  return (v & *out_in) != bddfalse && (v & *out_out) != bddfalse;
}

}  // namespace ltlf_ek
