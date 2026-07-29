#pragma once

#include <optional>
#include <vector>

#include <spot/tl/formula.hh>
#include <spot/twa/bdddict.hh>
#include <spot/twaalgos/ltlf2dfa.hh>

#include "ltlf_ek/synthesis.hpp"
#include "ltlf_ek/transducer.hpp"
#include "ltlf_ek/variables.hpp"

namespace ltlf_ek {

// Method 3.1's product: the cons-filtered product of the Goal automaton with
// the knowledge transducers, with the Goal side built ON THE FLY by forward
// progression --- alg:otfdfa_product, fused so that no Goal automaton object
// ever exists.  Sibling of mtnfa_product_to_mtdfa (which fuses cons with
// subset determinization); this one fuses cons with the Goal CONSTRUCTION
// itself.
//
// `phi`  : the Goal formula.
// `taus` : the knowledge transducers, T_in then T_out --- the same
//          n-transducer generalization build_product* / mtnfa_product_to_mtdfa
//          use; the product state carries one state per element.
// `vars` : the closed AP universe; supplies the output mtdfa's `aps`.
// `dict` : the shared bdd_dict (phi carries none, unlike mtnfa's Mtnfa::dict).
//
// NOT LANGUAGE-EXACT --- an OVER-APPROXIMATION, by design (I5, see the leaf
// triage in Relabel).  Once phi is irrevocably satisfied the row collapses to
// the accepting sink, which accepts EVERY continuation, so the cons filter
// stops being enforced past that point: L(result) is a strict superset of the
// true cons-filtered product language.  E.g. phi = a with a t_in whose
// emits_region excludes some letter v: the exact product rejects a.v, this one
// accepts it.  Sound for THIS consumer only because solve_mtdfa plus
// system-controlled termination make the two equirealizable with the same
// controller (docs/prd/otf-mtdfa-product.md I5).  A second consumer that reads
// L(P) itself --- Method 3.2's aggregation, a language-equality oracle, model
// checking over P --- would get wrong answers silently.
//
// Preconditions:
//   - every tau and `vars` share `dict`   (validate_product_inputs)
//   - phi's APs are a subset of vars.universe()
//   - every tau is deterministic (delta_edges guards pairwise disjoint) ---
//     CHECKED, throws std::runtime_error, same wording as
//     mtnfa_product_to_mtdfa / build_product_symbolic
// Does NOT check the Turn order contract: that is solve_mtdfa's
// precondition, discharged by OtfMtdfaProduct::synthesize.
//
// nullptr is NEVER returned.
spot::mtdfa_ptr otf_product_to_mtdfa(const spot::formula& phi,
                                     const std::vector<const Transducer*>& taus,
                                     const VariablePartition& vars,
                                     const spot::bdd_dict_ptr& dict);

// Method 3.1 (main.tex §otfdfa) in the mtdfa Representation
// (docs/GLOSSARY.md "The five methods", "Representation") --- a method, not
// a representation variant of an existing one: the five-methods table's THIRD
// row, mtdfa cell.  That row's explicit cell (OtfDfaProduct) is the first in
// the table deliberately left unbuilt --- Forward progression yields an MTBDD
// natively, so flattening it into a twa_graph only to re-solve is pure loss ---
// which makes this the only implementation of Method 3.1 today.
class OtfMtdfaProduct final : public Synthesis {
 public:
  // otf_solve (Phase 2, docs/prd/otf-mtdfa-product.md): fuse game solving
  // into the construction and abort as soon as the initial state is
  // determined.  Default OFF --- the proven build-then-solve path stays the
  // default until the benchmark says otherwise, the same discipline
  // MtdfaProduct's `minimize_mtdfa` knob follows.
  //
  // Phase 1: setting this true is NOT a silent fallback to build-then-solve
  // --- synthesize() throws std::logic_error, so a caller can never
  // mistake an unimplemented fast path for the slow path having run.
  explicit OtfMtdfaProduct(bool otf_solve = false) : otf_solve_(otf_solve) {}

  std::optional<Controller> synthesize(const spot::formula& phi,
                                       const VariablePartition& vars,
                                       const Transducer& t_in,
                                       const Transducer& t_out) override;

 private:
  bool otf_solve_;
};

}  // namespace ltlf_ek
