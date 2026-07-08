#include "ltlf_ek/verify_controller.hpp"

#include <cstddef>
#include <map>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include <bddx.h>
#include <spot/twa/twagraph.hh>
#include <spot/twaalgos/synthesis.hh>

#include "ltlf_ek/ltlf_to_dfa.hpp"
#include "ltlf_ek/product.hpp"
#include "ltlf_ek/transducer_io.hpp"

// docs/prd/controller-verifier.md "Behaviour / semantics": a one-player (env)
// reachability/safety fixpoint on A_phi x T_in x T_out x T_C, since T_C is
// fixed and has no remaining moves.  NEVER reuses solve_dfa/solve_game --- the
// product traversal, Bad nu-fixpoint, virtual-start split and witness
// extraction are all hand-rolled here.
namespace ltlf_ek {
namespace {

// ProductState = <s_phi, q_in, q_out, q_c> --- A_phi x T_in x T_out x T_C.
// Shadows ltlf_ek::ProductState (product.hpp) for every unqualified use below
// (StateInfo / compute_bad / extract_witness) --- only build_verifier_graph
// needs the shared struct type, so it fully-qualifies it there.
using ProductState = std::tuple<unsigned, unsigned, unsigned, unsigned>;

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
// this file's tuple-keyed StateInfo graph from the returned neutral map.
// io_vars must list Ifree first (n_ifree of them) so a letters-index's low
// bits are exactly the Ifree combo, matching StateInfo::edges' bucketing.
std::map<ProductState, StateInfo> build_verifier_graph(
    const spot::twa_graph_ptr& dfa, const Transducer& t_in,
    const Transducer& t_out, const Transducer& t_c,
    const std::vector<int>& io_vars, std::size_t n_ifree) {
  const std::vector<bdd> letters = ltlf_ek::all_letters(io_vars);
  const std::vector<const Transducer*> taus{&t_in, &t_out, &t_c};
  const ltlf_ek::ProductState init{
      dfa->get_init_state_number(),
      {t_in.initial_state(), t_out.initial_state(), t_c.initial_state()}};
  const std::map<ltlf_ek::ProductState, ltlf_ek::ProductNode> graph =
      ltlf_ek::build_product(dfa, taus, init, letters,
                             /*goal_must_be_complete=*/false);

  const std::size_t n_ifree_combos = std::size_t{1} << n_ifree;
  const std::size_t ifree_mask = n_ifree_combos - 1;

  std::map<ProductState, StateInfo> result;
  for (const auto& [state, node] : graph) {
    const ProductState key{state.goal, state.taus[0], state.taus[1],
                           state.taus[2]};
    StateInfo info;
    info.acc = node.acc;
    info.edges.assign(n_ifree_combos, std::nullopt);

    for (const auto& [idx, succ] : node.edges) {
      const std::size_t ifree_idx = idx & ifree_mask;
      // Determinism of lambda_in/lambda_c/lambda_out (docs/prd/
      // controller-verifier.md) means at most one letter agrees per Ifree
      // combo; build_product appends edges in ascending letters-index order,
      // so keeping the first found reproduces the pre-migration tie-break.
      if (!info.edges[ifree_idx])
        info.edges[ifree_idx] = {
            letters[idx],
            ProductState{succ.goal, succ.taus[0], succ.taus[1], succ.taus[2]}};
    }

    for (const auto& e : info.edges)
      if (!e) { info.has_dead_end = true; break; }

    result.emplace(key, std::move(info));
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
  std::set<std::string> universe = vars.inputs();
  const std::set<std::string> outs = vars.outputs();
  universe.insert(outs.begin(), outs.end());
  for (const auto& ap : collect_aps(phi))
    if (!universe.count(ap))
      throw std::invalid_argument(
          "verify_controller: formula AP '" + ap +
          "' outside I∪O (the partition is the closed universe of APs)");

  const spot::bdd_dict_ptr dict = t_in.dict();
  if (t_out.dict() != dict || t_c.dict() != dict)
    throw std::invalid_argument(
        "verify_controller: T_in, T_out and T_C must share one bdd_dict");

  // --- A_phi on the shared dict (accepting states = F_phi). ---
  const spot::twa_graph_ptr dfa = ltlf_to_dfa(phi, dict);

  // --- Full-letter alphabet, Ifree variables enumerated first (see
  //     all_letters) so an enumeration index's low bits are the Ifree combo.
  spot::twa_graph_ptr registrar = spot::make_twa_graph(dict);
  std::vector<int> io_vars;
  io_vars.reserve(universe.size());
  for (const auto& n : vars.input_free) io_vars.push_back(registrar->register_ap(n));
  const std::size_t n_ifree = io_vars.size();
  for (const auto& n : vars.input_known) io_vars.push_back(registrar->register_ap(n));
  for (const auto& n : vars.output_free) io_vars.push_back(registrar->register_ap(n));
  for (const auto& n : vars.output_known) io_vars.push_back(registrar->register_ap(n));

  // std::tuple<unsigned,unsigned,unsigned,unsigned> spelled out (not the bare
  // ProductState name) --- this call site sits outside the anonymous
  // namespace above, where that name is ambiguous with ltlf_ek::ProductState
  // (product.hpp), unlike inside the anonymous namespace where it shadows it.
  const std::tuple<unsigned, unsigned, unsigned, unsigned> init{
      dfa->get_init_state_number(), t_in.initial_state(),
      t_out.initial_state(), t_c.initial_state()};
  const auto graph = build_verifier_graph(dfa, t_in, t_out, t_c, io_vars, n_ifree);
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

OutputLabeledTransducer controller_as_transducer(const Controller& controller,
                                                 const VariablePartition& vars) {
  // controller.strategy (solved_game_to_mealy) is a SPLIT/alternating arena
  // --- an env-player node's out-edges are guarded by Ifree alone, leading to
  // a sys-player node whose out-edges are guarded by Ofree alone (Spot's
  // "state-player" named property; confirmed by unsplit_2step's own
  // precondition).  unsplit_2step collapses each Ifree-then-Ofree hop into
  // one edge guarded by their conjunction ("ins&outs", synthesis.hh), leaving
  // only the (real) Q_C states --- exactly the one-edge-per-transition shape
  // OutputLabeledTransducer expects.
  const spot::twa_graph_ptr g = spot::unsplit_2step(controller.strategy);

  const SigmaSlices slices = sigma_slices(vars, Role::t_c);
  bdd sigma0_cube = bddtrue;
  for (const auto& n : slices.sigma0) sigma0_cube &= bdd_ithvar(g->register_ap(n));
  bdd sigma1_cube = bddtrue;
  for (const auto& n : slices.sigma1) sigma1_cube &= bdd_ithvar(g->register_ap(n));

  // lambda_C(q, ...) --- the union of q's out-edge guards is already the
  // relation over Ifree x Ofree the Mealy strategy commits to at q (the
  // "delta via edges, output derived" idiom, docs/GLOSSARY.md
  // "Controller-as-transducer view"); a state with no out-edges commits to
  // bddfalse (undefined lambda_C there, mirrored by an equally undefined
  // delta_C --- see OutputLabeledTransducer::delta/::lambda).
  const unsigned n_states = g->num_states();
  std::vector<bdd> lambda_by_state(n_states, bddfalse);
  for (unsigned q = 0; q < n_states; ++q) {
    bdd out = bddfalse;
    for (const auto& e : g->out(q)) out |= e.cond;
    lambda_by_state[q] = out;
  }

  return OutputLabeledTransducer(g, std::move(lambda_by_state), sigma0_cube,
                                 sigma1_cube);
}

}  // namespace ltlf_ek
