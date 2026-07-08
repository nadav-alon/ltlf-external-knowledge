#include "ltlf_ek/verify_controller.hpp"

#include <cstddef>
#include <map>
#include <optional>
#include <set>
#include <stdexcept>
#include <utility>
#include <vector>

#include <bddx.h>
#include <spot/twa/twagraph.hh>

#include "ltlf_ek/ltlf_to_dfa.hpp"
#include "ltlf_ek/product.hpp"
#include "ltlf_ek/synthesis.hpp"

// docs/prd/controller-verifier.md "Behaviour / semantics": a one-player (env)
// reachability/safety fixpoint on A_phi x T_in x T_out x T_C, since T_C is
// fixed and has no remaining moves.  NEVER reuses solve_dfa/solve_game --- the
// product traversal, Bad nu-fixpoint, virtual-start split and witness
// extraction are all hand-rolled here.
namespace ltlf_ek {
namespace {

// One product state's outgoing structure: Acc(s), whether some Ifree has no
// agreeing letter (hasDeadEnd), and --- indexed by the Ifree combo's
// enumeration index --- the (letter, successor) an agreeing letter gives.
struct StateInfo {
  bool acc = false;
  bool has_dead_end = false;
  std::vector<std::optional<std::pair<bdd, ProductState>>> edges;
};

// Reshaping bridge onto the shared core (include/ltlf_ek/product.hpp,
// docs/prd/transducer-product.md "verify_controller" consumer note): drives
// ltlf_ek::build_product over A_phi x T_in x T_out x T_c
// (goal_must_be_complete = false --- a goal miss is a legitimate
// non-agreement here, not DfaProduct's completeness invariant) and rebuilds
// this file's ifree-bucketed StateInfo graph from the returned neutral map,
// keyed on the shared ltlf_ek::ProductState throughout (no local reshaping).
std::map<ProductState, StateInfo> build_verifier_graph(
    const spot::twa_graph_ptr& dfa, const Transducer& t_in,
    const Transducer& t_out, const Transducer& t_c,
    const LetterAlphabet& alphabet) {
  const std::vector<const Transducer*> taus{&t_in, &t_out, &t_c};
  const ProductState init{
      dfa->get_init_state_number(),
      {t_in.initial_state(), t_out.initial_state(), t_c.initial_state()}};
  const std::map<ProductState, ProductNode> graph =
      build_product(dfa, taus, init, alphabet,
                    /*goal_must_be_complete=*/false);

  const std::size_t n_ifree_combos = alphabet.n_ifree_combos();

  std::map<ProductState, StateInfo> result;
  for (const auto& [state, node] : graph) {
    StateInfo info;
    info.acc = node.acc;
    info.edges.assign(n_ifree_combos, std::nullopt);

    for (const auto& [idx, succ] : node.edges) {
      const std::size_t ifree_idx = alphabet.ifree_index(idx);
      // Determinism of lambda_in/lambda_c/lambda_out (docs/prd/
      // controller-verifier.md) means at most one letter agrees per Ifree
      // combo; build_product appends edges in ascending letters-index order,
      // so keeping the first found fixes the tie-break deterministically.
      if (!info.edges[ifree_idx])
        info.edges[ifree_idx] = {alphabet.letters()[idx], succ};
    }

    for (const auto& e : info.edges)
      if (!e) { info.has_dead_end = true; break; }

    result.emplace(state, std::move(info));
  }
  return result;
}

// Bad = nuY. { s : ¬Acc(s) ∧ (hasDeadEnd(s) ∨ ∃ Ifree whose agreeing
// successor s' ∈ Y) } --- docs/prd/controller-verifier.md.  Y_0 = {¬Acc(s)}
// is decreasing under the monotone operator (every candidate already implies
// ¬Acc(s)), so plain iteration to a fixpoint computes the greatest fixpoint.
std::set<ProductState> compute_bad(const std::map<ProductState, StateInfo>& graph) {
  std::set<ProductState> bad;
  for (const auto& [s, info] : graph)
    if (!info.acc) bad.insert(s);

  for (;;) {
    std::set<ProductState> next;
    for (const ProductState& s : bad) {
      const StateInfo& info = graph.at(s);
      bool stays = info.has_dead_end;
      if (!stays)
        for (const auto& e : info.edges)
          if (e && bad.count(e->second)) { stays = true; break; }
      if (stays) next.insert(s);
    }
    if (next.size() == bad.size()) return next;  // fixpoint (monotone shrink).
    bad = std::move(next);
  }
}

// Extract the counterexample lasso starting from the virtual start's single
// mandatory transition (docs/prd/controller-verifier.md "Witness").  Callable
// only when !ok, i.e. `init` either has a dead end or some Ifree choice's
// agreeing successor is in `bad` --- so the first iteration below always finds
// one of the two branches, and every subsequent state visited is itself in
// `bad` (by construction), so it always finds one too; termination is
// guaranteed by the finite state space (pigeonhole -> a repeat or dead-end).
Witness extract_witness(const ProductState& init,
                        const std::map<ProductState, StateInfo>& graph,
                        const std::set<ProductState>& bad) {
  std::vector<bdd> letters;
  std::map<ProductState, std::size_t> first_seen;  // state -> index into `letters`
                                                    // of the letter that reached it.
  ProductState cur = init;
  for (;;) {
    const StateInfo& info = graph.at(cur);
    if (info.has_dead_end) return Witness{letters, {}};

    // Deterministic tie-break: lexicographically least Ifree combo whose
    // agreeing successor is in Bad (must exist --- see function comment).
    const std::pair<bdd, ProductState>* chosen = nullptr;
    for (const auto& e : info.edges)
      if (e && bad.count(e->second)) { chosen = &*e; break; }
    if (!chosen)
      throw std::logic_error(
          "verify_controller: extract_witness invariant violated (a Bad "
          "state with neither a dead end nor a Bad-bound Ifree choice)");

    letters.push_back(chosen->first);
    const ProductState succ = chosen->second;
    const auto it = first_seen.find(succ);
    if (it != first_seen.end()) {
      const std::size_t j = it->second;
      return Witness{std::vector<bdd>(letters.begin(), letters.begin() + j + 1),
                     std::vector<bdd>(letters.begin() + j + 1, letters.end())};
    }
    first_seen.emplace(succ, letters.size() - 1);
    cur = succ;
  }
}

}  // namespace

VerifyResult verify_controller(const spot::formula& phi,
                               const VariablePartition& vars,
                               const Transducer& t_in, const Transducer& t_out,
                               const Transducer& t_c) {
  // --- Validation (same policy as DfaProduct::synthesize). ---
  const std::vector<const Transducer*> taus{&t_in, &t_out, &t_c};
  validate_product_inputs(phi, vars, taus);

  const spot::bdd_dict_ptr dict = t_in.dict();

  // --- A_phi on the shared dict (accepting states = F_phi). ---
  const spot::twa_graph_ptr dfa = ltlf_to_dfa(phi, dict);

  // --- Full-letter alphabet, Ifree-first (see LetterAlphabet) so a letter's
  //     enumeration index's low bits are the Ifree combo.
  spot::twa_graph_ptr registrar = spot::make_twa_graph(dict);
  const LetterAlphabet alphabet(vars, registrar);

  const ProductState init{
      dfa->get_init_state_number(),
      {t_in.initial_state(), t_out.initial_state(), t_c.initial_state()}};
  const auto graph = build_verifier_graph(dfa, t_in, t_out, t_c, alphabet);
  const auto bad = compute_bad(graph);

  // --- Non-empty-trace / virtual-start split (docs/prd/
  //     controller-verifier.md): correct(T_C) iff the virtual start has no
  //     dead end and every first-successor escapes Bad --- Acc(init) itself
  //     is irrelevant (the system may not stop before consuming >=1 letter).
  const StateInfo& start = graph.at(init);
  bool ok = !start.has_dead_end;
  if (ok)
    for (const auto& e : start.edges)
      if (e && bad.count(e->second)) { ok = false; break; }

  if (ok) return VerifyResult{true, std::nullopt};
  Witness w = extract_witness(init, graph, bad);
  return VerifyResult{false, w};
}

VerifyResult verify_controller(const spot::formula& phi,
                               const VariablePartition& vars,
                               const Transducer& t_in, const Transducer& t_out,
                               const Controller& controller) {
  const OutputLabeledTransducer t_c = controller_as_transducer(controller, vars);
  return verify_controller(phi, vars, t_in, t_out, t_c);
}

}  // namespace ltlf_ek
