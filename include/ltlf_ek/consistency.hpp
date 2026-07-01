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
// TODO(developer): implement per main.tex once lambda projection is settled.
bool consistent(const Transducer& t_in, unsigned q_in, const Transducer& t_out,
                unsigned q_out, bdd v);

}  // namespace ltlf_ek
