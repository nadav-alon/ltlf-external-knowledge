#include "ltlf_ek/output_labeled_transducer.hpp"

#include <stdexcept>
#include <utility>

namespace ltlf_ek {

OutputLabeledTransducer::OutputLabeledTransducer(spot::twa_graph_ptr delta_dfa,
                                                 std::vector<bdd> lambda_by_state,
                                                 bdd sigma0_cube, bdd sigma1_cube)
    : delta_dfa_(std::move(delta_dfa)),
      lambda_by_state_(std::move(lambda_by_state)),
      sigma0_cube_(sigma0_cube),
      sigma1_cube_(sigma1_cube) {
  // Invariant: exactly one output relation per state (glossary: "one BDD over
  // Sigma0 ∪ Sigma1 per state").  Otherwise lambda() would index out of range.
  if (lambda_by_state_.size() != delta_dfa_->num_states())
    throw std::invalid_argument(
        "OutputLabeledTransducer: lambda_by_state.size() must equal "
        "delta_dfa->num_states() (one output relation per state)");
}

unsigned OutputLabeledTransducer::initial_state() const {
  return delta_dfa_->get_init_state_number();
}

spot::bdd_dict_ptr OutputLabeledTransducer::dict() const {
  return delta_dfa_->get_dict();
}

std::optional<unsigned> OutputLabeledTransducer::delta(unsigned q, bdd v) const {
  // Navigate the twa purely as a transition structure (acceptance ignored).
  // v is a full letter (minterm), so it satisfies guard `e.cond` exactly when
  // (v & e.cond) != bddfalse.  A deterministic delta has at most one such edge;
  // zero means delta is undefined here (partial transducer, nullopt).
  std::optional<unsigned> succ;
  for (const auto& e : delta_dfa_->out(q)) {
    if ((v & e.cond) != bddfalse) {
      if (succ)
        throw std::runtime_error(
            "OutputLabeledTransducer::delta: non-deterministic transition "
            "structure (letter satisfies more than one guard)");
      succ = e.dst;
    }
  }
  return succ;
}

std::optional<bdd> OutputLabeledTransducer::lambda(unsigned q, bdd v) const {
  const bdd& out = lambda_by_state_[q];
  if (out == bddfalse) return std::nullopt;  // lambda undefined at this state.

  // Read only the Sigma0 slice of the full letter v (abuse-of-notation,
  // main.tex §87) and return the committed cube over Sigma1.
  //
  // Fix out_[q]'s Sigma0 variables to v's values while leaving Sigma1 free.
  // out_[q] mentions only Sigma0 ∪ Sigma1 variables, so a restrictor carrying
  // v's polarities on every variable *except* Sigma1 fixes exactly Sigma0
  // (bdd_restrict ignores variables absent from out_[q]).
  bdd observation = bdd_exist(v, sigma1_cube_);
  bdd r = bdd_restrict(out, observation);
  if (r == bddfalse)
    return std::nullopt;  // this observation has no committed completion.
  return bdd_exist(r, sigma0_cube_);  // keep only Sigma1.
}

}  // namespace ltlf_ek
