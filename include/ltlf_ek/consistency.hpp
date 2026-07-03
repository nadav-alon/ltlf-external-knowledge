#pragma once

#include <bddx.h>

#include "ltlf_ek/transducer.hpp"

namespace ltlf_ek {

// cons(q_in, q_out, v) --- main.tex, Method 1:
//
//   cons := (v ∩ \Iknown = lambda_in(q_in, v))
//         ∧ (v ∩ \Oknown = lambda_out(q_out, v)).
//
// A full letter v is *consistent* at transducer states (q_in, q_out) exactly
// when its V-variables are what the two knowledge transducers output.  This is
// the canonical spelling of the predicate: call it `consistent`, never
// "agrees"/"valid"/"matches" (see docs/GLOSSARY.md).
//
// Partiality (main.tex §107, \cref{def:enabled}): `enabled` subsumes `cons`, so
// an undefined lambda (nullopt) makes v non-enabled --- `consistent` returns
// false, exactly as a mismatch would.  delta-definedness is the caller's
// concern (it guards the delta applications inside the product); this predicate
// covers the lambda half of `enabled`.
//
// Precondition: t_in, t_out and the letter v must share one spot::bdd_dict, and
// v is a full letter (a minterm over I∪O) --- the per-letter equality check
// relies on it.
bool consistent(const Transducer& t_in, unsigned q_in, const Transducer& t_out,
                unsigned q_out, bdd v);

}  // namespace ltlf_ek
