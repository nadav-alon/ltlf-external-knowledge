// include/ltlf_ek/mtnfa_product.hpp                                       [new]
#pragma once

#include <optional>
#include <vector>

#include <spot/twaalgos/ltlf2dfa.hh>

#include "ltlf_ek/mtnfa.hpp"
#include "ltlf_ek/synthesis.hpp"
#include "ltlf_ek/transducer.hpp"
#include "ltlf_ek/variables.hpp"

namespace ltlf_ek {

// The cons-filtered product of the Goal MTNFA with the knowledge transducers,
// subset-determinized into a spot::mtdfa --- alg:nfa_product lines
// :cons and :determinize FUSED into one symbolic pass (see the PRD's "Novel
// mechanisms").  Method 1's product in the mtdfa Representation; the sibling of
// mtnfa_to_mtdfa, which is the same determinization applied to the Goal NFA
// alone.
//
// `goal`  : the Goal MTNFA (ltlf_to_mtnfa(phi, dict)).
// `taus`  : the knowledge transducers, T_in then T_out --- generalized to n
//           the way build_product / build_product_symbolic / build_product_nondet
//           are; the determinized state carries one state per element.
// `vars`  : supplies the output mtdfa's `aps` = universe() (see below).
//
// Preconditions:
//   - `goal`, every tau, and `vars` share ONE spot::bdd_dict (goal.dict).
//   - !goal.accepting[goal.initial] (mtnfa_to_mtdfa's F2 precondition, same
//     reason: R0 is seeded at output index 0 and never rediscovered as a
//     destination, so an accepting initial state would be silently dropped).
//     ltlf_to_mtnfa always satisfies it (fresh non-accepting s_{N,0}).
//   - Every tau is deterministic (delta_edges' guards pairwise disjoint) ---
//     asserted, see "Novel mechanisms (d)".
// Does NOT check the Turn order AP-ordering contract: that is solve_mtdfa's
// precondition, discharged by MtnfaProduct::synthesize.  Language equality is
// independent of the BDD variable order, so a direct caller comparing languages
// (the oracle) needs no ordering.
//
// nullptr is NEVER returned.
spot::mtdfa_ptr mtnfa_product_to_mtdfa(const Mtnfa& goal,
                                       const std::vector<const Transducer*>& taus,
                                       const VariablePartition& vars);

// Method 1 (main.tex §nfa, \cref{alg:nfa_product}) over the mtdfa Representation
// (docs/GLOSSARY.md "Representation") --- a SECOND implementation of Method 1,
// not a sixth method; NfaProduct is left untouched and is the differential this
// route is graded against.
//
// Shape forced by Synthesis; identical signature to NfaProduct / MtdfaProduct.
class MtnfaProduct final : public Synthesis {
 public:
  std::optional<Controller> synthesize(const spot::formula& phi,
                                       const VariablePartition& vars,
                                       const Transducer& t_in,
                                       const Transducer& t_out) override;
};

}  // namespace ltlf_ek
