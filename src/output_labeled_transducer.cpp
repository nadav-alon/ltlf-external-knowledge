#include "ltlf_ek/output_labeled_transducer.hpp"

#include <stdexcept>
#include <utility>

#include "ltlf_ek/detail/util.hpp"

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

bdd OutputLabeledTransducer::emits_region(unsigned q) const {
  // lambda_by_state_[q] is already the whole output relation over
  // Sigma0 ∪ Sigma1 at q (or bddfalse if lambda is undefined there) --- exactly
  // the region form of emits (Transducer::emits_region doc-comment).
  return lambda_by_state_[q];
}

std::vector<std::pair<bdd, unsigned>> OutputLabeledTransducer::delta_edges(
    unsigned q) const {
  // The twa's out-edges out of q ARE the deterministic delta partition
  // (acceptance ignored, per the class comment); just copy (cond, dst).
  std::vector<std::pair<bdd, unsigned>> edges;
  for (const auto& e : delta_dfa_->out(q)) edges.emplace_back(e.cond, e.dst);
  return edges;
}

OutputLabeledTransducer trivial_transducer(const VariablePartition& partition,
                                           Role role,
                                           const spot::bdd_dict_ptr& dict) {
  const SigmaSlices slices = sigma_slices(partition, role);
  if (!slices.sigma1.empty())
    throw std::invalid_argument(
        "trivial_transducer: only valid when the role's known set is empty "
        "(t_in: Iknown, t_out: Oknown) --- supply a transducer file instead");

  auto g = spot::make_twa_graph(dict);
  const bdd sigma0_cube = detail::cube_of(slices.sigma0, g);
  const bdd sigma1_cube = detail::cube_of(slices.sigma1, g);

  g->new_states(1);
  g->set_init_state(0);
  g->new_edge(0, 0, bddtrue);  // delta self-loops on every letter.
  // lambda commits the empty cube (bddtrue): with Sigma1 = ∅ there is nothing
  // to commit to, so `consistent` (v & bddtrue != bddfalse) is trivially true.
  return OutputLabeledTransducer(g, {bddtrue}, sigma0_cube, sigma1_cube);
}

}  // namespace ltlf_ek
