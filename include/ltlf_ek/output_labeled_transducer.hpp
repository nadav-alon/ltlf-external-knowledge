#pragma once

#include <optional>
#include <vector>

#include <bddx.h>
#include <spot/twa/twagraph.hh>

#include "ltlf_ek/transducer.hpp"

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
  std::optional<unsigned> delta(unsigned q, bdd v) const override;
  std::optional<bdd> lambda(unsigned q, bdd v) const override;

 private:
  spot::twa_graph_ptr delta_dfa_;
  std::vector<bdd> lambda_by_state_;
  bdd sigma0_cube_;
  bdd sigma1_cube_;
};

}  // namespace ltlf_ek
