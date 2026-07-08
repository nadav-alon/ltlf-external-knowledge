#pragma once

#include <optional>
#include <vector>

#include <bddx.h>
#include <spot/twa/bdddict.hh>
#include <spot/twa/twagraph.hh>

#include "ltlf_ek/role.hpp"
#include "ltlf_ek/transducer.hpp"
#include "ltlf_ek/variables.hpp"

namespace ltlf_ek {

// A concrete Transducer whose transition structure reuses a spot::twa_graph and
// whose output function lambda is stored explicitly, one BDD per state over
// Sigma0 ∪ Sigma1.  See docs/GLOSSARY.md ("output-labeled transducer").
//
// IMPORTANT: the twa_graph is used *purely as a deterministic transition
// structure*.  A Transducer has NO acceptance condition (main.tex §101), so the
// twa's ω-acceptance is IGNORED entirely --- never read it as transducer
// finality.  Only the state graph, initial state, and edge guards are used.
//
// delta navigates the unique edge out of q whose guard is satisfied by the full
// letter v; if none is satisfied, delta is undefined (nullopt) --- this is how
// an incomplete twa expresses a partial delta (main.tex §107).  More than one
// satisfied guard violates the deterministic-delta contract and throws.
//
// Precondition: delta_dfa's edge guards, the lambda_by_state / sigma0_cube /
// sigma1_cube BDDs, and every letter v passed to delta/lambda must all share
// one spot::bdd_dict --- otherwise their variable numbers refer to unrelated
// variables and the (v & ...) tests are meaningless.
class OutputLabeledTransducer final : public Transducer {
 public:
  // delta_dfa      --- Q, delta, q0 (acceptance ignored).
  // lambda_by_state --- out_[q]: a BDD over Sigma0 ∪ Sigma1 per state encoding
  //                    the deterministic output relation; bddfalse (or a
  //                    Sigma0 slice with no completion) means lambda is
  //                    undefined at that observation.
  // sigma0_cube    --- variable-cube of the vars lambda may observe
  //                    (Ifree for T_in, I∪Ofree for T_out).
  // sigma1_cube    --- variable-cube of the vars lambda produces
  //                    (Iknown for T_in, Oknown for T_out).
  OutputLabeledTransducer(spot::twa_graph_ptr delta_dfa,
                          std::vector<bdd> lambda_by_state, bdd sigma0_cube,
                          bdd sigma1_cube);

  unsigned initial_state() const override;
  spot::bdd_dict_ptr dict() const override;
  std::optional<unsigned> delta(unsigned q, bdd v) const override;
  std::optional<bdd> lambda(unsigned q, bdd v) const override;

  // The observed / produced slices this transducer was built with
  // (docs/GLOSSARY.md: "observed / produced slice").  Exposed so a consumer can
  // build letters over the right variables and so parse_transducer's derivation
  // can be checked against the align block.
  bdd sigma0_cube() const { return sigma0_cube_; }
  bdd sigma1_cube() const { return sigma1_cube_; }

 private:
  spot::twa_graph_ptr delta_dfa_;
  std::vector<bdd> lambda_by_state_;
  bdd sigma0_cube_;
  bdd sigma1_cube_;
};

// A single-state transducer whose delta self-loops on every letter and whose
// lambda commits the empty cube, so `consistent` (docs/GLOSSARY.md) is
// trivially satisfied against it regardless of the letter --- a general
// transducer factory, not CLI plumbing (though the CLI is one caller, see
// docs/prd/cli-wrapper.md "Behaviour" #4: substituted when a known set is
// empty and no transducer file was supplied).
//
// Built on `dict` with role-correct Sigma0/Sigma1 (the same orientation
// `parse_transducer` would derive, see `sigma_slices`).  Only valid when the
// role's *produced* slice (Sigma1 --- Iknown for t_in, Oknown for t_out) is
// empty; throws std::invalid_argument otherwise, since a non-empty known set
// must be committed to something specific, not trivially "anything goes".
OutputLabeledTransducer trivial_transducer(const VariablePartition& partition,
                                           Role role,
                                           const spot::bdd_dict_ptr& dict);

}  // namespace ltlf_ek
