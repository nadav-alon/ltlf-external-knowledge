#pragma once

#include <bddx.h>

#include "ltlf_ek/transducer.hpp"

namespace ltlf_ek {

// cons(q_in, q_out, v) --- main.tex def:consistency (§203):
//
//   cons := (v ∩ \Iknown = lambda_in(q_in, v))
//         ∧ (v ∩ \Oknown = lambda_out(q_out, v)).
//
// A full letter v is *consistent* at transducer states (q_in, q_out) exactly
// when its V-variables are what the two knowledge transducers output.  This is
// the canonical spelling of the predicate: call it `consistent`, never
// "agrees"/"valid"/"matches" (see docs/GLOSSARY.md).
//
// Partiality (main.tex §211 note after def:consistency; transducer partiality
// prose §107, §111-112): `enabled` subsumes `cons`, so
// an undefined lambda (nullopt) makes v non-enabled --- `consistent` returns
// false, exactly as a mismatch would.  delta-definedness is the caller's
// concern (it guards the delta applications inside the product); this predicate
// covers the lambda half of `enabled`.
//
// Precondition: t_in, t_out and the letter v must share one spot::bdd_dict, and
// v is a full letter (a minterm over I∪O) --- the per-letter equality check
// relies on it.
//
// emits(t, q, v)  [glossary: "Output agreement (emits)"]: the per-transducer
// lambda-agreement atom --- one conjunct of cons (def:consistency), per
// transducer.  lambda-ONLY (delta-definedness is the caller's concern, read
// off the successor in the product loop, not here):
//   t.lambda(q, v) defined  &&  (v & *lambda) != bddfalse.
// A nullopt lambda (undefined) => false (non-enabled; def:consistency
// partiality note).  Sigma1-agnostic, same reasoning as `consistent` below.
bool emits(const Transducer& t, unsigned q, bdd v);

// consistent is now exactly emits(t_in, q_in, v) && emits(t_out, q_out, v) ---
// same def:consistency concept, same signature, no behaviour change;
// delta-definedness stays the caller's concern.
bool consistent(const Transducer& t_in, unsigned q_in, const Transducer& t_out,
                unsigned q_out, bdd v);

}  // namespace ltlf_ek
