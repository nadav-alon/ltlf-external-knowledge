#pragma once

#include <spot/twa/bdddict.hh>

#include "ltlf_ek/variables.hpp"

namespace ltlf_ek {

// Turn order (docs/GLOSSARY.md "Turn order", main.tex §83) --- on the mtdfa
// Representation this is encoded ONLY in the BDD variable order (Phase 0/Q2,
// docs/prd/mtdfa-product.md): every input_free variable must sit strictly
// ABOVE every controllable variable (output_free u input_known u
// output_known, decision 2) for the mtdfa game to read Mealy semantics; the
// reverse silently yields Moore semantics and a WRONG verdict --- no crash,
// which is exactly what makes this load-bearing.  register_ap is idempotent,
// so the order can only be FIXED before the first registration of each AP ---
// hence the split below: a registrar to run at dict setup, and a checker to
// guard MtdfaProduct's use of that dict.

// Register every AP of `vars` on `dict`, in the MTDFA-game-correct order:
// input_free (sorted), then input_known, output_free, output_known.  Call at
// dict creation, BEFORE parsing any transducer or building any automaton on
// `dict` --- register_ap's idempotence means this can only ESTABLISH a good
// order, never repair a bad one a prior registration already set.
void register_turn_order_aps(const VariablePartition& vars,
                             const spot::bdd_dict_ptr& dict);

// Precondition guard for MtdfaProduct (Phase 0/Q2): throws
// std::invalid_argument iff some input_free variable's BDD level does not
// sit strictly above every controllable variable's (output_free u
// input_known u output_known) --- the necessary AND sufficient rule; order
// AMONG the controllables is unconstrained.  A violated order does not crash
// on its own --- it silently returns a plausible, wrong "unrealizable" --- so
// this turns that into a loud failure instead.
void require_turn_order_aps(const VariablePartition& vars,
                            const spot::bdd_dict_ptr& dict);

}  // namespace ltlf_ek
