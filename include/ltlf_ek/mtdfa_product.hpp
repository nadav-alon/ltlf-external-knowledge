#pragma once

#include "ltlf_ek/synthesis.hpp"

namespace ltlf_ek {

// Method 2 (alg:dfa_product) over the mtdfa Representation
// (docs/GLOSSARY.md "Representation", docs/prd/mtdfa-product.md) --- a
// SECOND implementation of Method 2, not a sixth method; DfaProduct is left
// untouched and is the differential this route is graded against.
//
// Decision 1 (docs/prd/mtdfa-product.md): the product is a language
// intersection of three mtdfa's --- the Goal's and each transducer's
// *Output-agreement automaton* (emits_dfa) lifted to mtdfa --- so the cons
// filter emerges from the intersection rather than being applied per-letter.
//
// Decision 2: solve_mtdfa makes Iknown, Oknown controllable alongside Ofree
// (the mtdfa game solver has only one knob), pinning them as forced system
// moves; require_turn_order_aps (on t_in.dict()) guards that this is safe
// under the BDD variable order (Phase 0/Q2) BEFORE spot::ltlf_to_mtdfa(phi,
// dict) is ever called directly --- there is no ltlf_ek::ltlf_to_mtdfa
// wrapper (Phase 0/Q2: register_ap's idempotence means a wrapper could not
// deliver the fix; see docs/GLOSSARY.md "Turn order").
//
// Shape forced by Synthesis; identical signature to DfaProduct.
class MtdfaProduct final : public Synthesis {
 public:
  std::optional<Controller> synthesize(const spot::formula& phi,
                                       const VariablePartition& vars,
                                       const Transducer& t_in,
                                       const Transducer& t_out) override;
};

}  // namespace ltlf_ek
