#include "ltlf_ek/verify_controller.hpp"

#include <cstddef>
#include <map>
#include <optional>
#include <queue>
#include <set>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include <bddx.h>
#include <spot/twa/twagraph.hh>
#include <spot/twaalgos/synthesis.hh>

#include "ltlf_ek/consistency.hpp"
#include "ltlf_ek/ltlf_to_dfa.hpp"
#include "ltlf_ek/transducer_io.hpp"

// docs/prd/controller-verifier.md "Behaviour / semantics": a one-player (env)
// reachability/safety fixpoint on A_phi x T_in x T_out x T_C, since T_C is
// fixed and has no remaining moves.  NEVER reuses solve_dfa/solve_game --- the
// product traversal, Bad nu-fixpoint, virtual-start split and witness
// extraction are all hand-rolled here.
namespace ltlf_ek {
namespace {

// ProductState = <s_phi, q_in, q_out, q_c> --- A_phi x T_in x T_out x T_C.
using ProductState = std::tuple<unsigned, unsigned, unsigned, unsigned>;

// delta_phi(s, v): navigate the (complete, per ltlf_to_dfa) Goal DFA as a
// plain transition structure --- acceptance ignored, same idiom as
// OutputLabeledTransducer::delta / DfaProduct's dfa_delta.  Unlike
// DfaProduct's helper, an unmatched letter returns nullopt rather than
// throwing: agree()'s "delta_phi ... defined at v" conjunct is a legitimate
// non-agreement here, not an internal-invariant violation.
std::optional<unsigned> dfa_delta(const spot::twa_graph_ptr& dfa, unsigned s,
                                  bdd v) {
  for (const auto& e : dfa->out(s))
    if ((v & e.cond) != bddfalse) return e.dst;
  return std::nullopt;
}

// Every full letter v in 2^{I∪O}, ordered so the first n_ifree bits of the
// enumeration index k encode exactly the Ifree assignment (io_vars lists the
// Ifree variables first) --- the same accepted-baseline enumeration cost as
// DfaProduct::all_letters, chosen here so grouping-by-Ifree is a cheap bitmask
// rather than an extra bdd_exist per letter.
std::vector<bdd> all_letters(const std::vector<int>& io_vars) {
  const std::size_t n = io_vars.size();
  std::vector<bdd> letters;
  letters.reserve(std::size_t{1} << n);
  for (std::size_t k = 0; k < (std::size_t{1} << n); ++k) {
    bdd v = bddtrue;
    for (std::size_t i = 0; i < n; ++i)
      v &= (k >> i & 1) ? bdd_ithvar(io_vars[i]) : bdd_nithvar(io_vars[i]);
    letters.push_back(v);
  }
  return letters;
}

// agree(s, v) --- docs/prd/controller-verifier.md:
//   cons(t_in, q_in, t_out, q_out, v)
//   ∧ (v ∩ Ofree = lambda_c(q_c, v ∩ I))
//   ∧ delta_phi, delta_in, delta_out, delta_c all defined at v.
// Returns the resulting product successor iff v agrees at s, else nullopt.
std::optional<ProductState> agreeing_successor(
    const spot::twa_graph_ptr& dfa, const Transducer& t_in,
    const Transducer& t_out, const Transducer& t_c, const ProductState& s,
    bdd v) {
  const auto [s_phi, q_in, q_out, q_c] = s;
  if (!consistent(t_in, q_in, t_out, q_out, v)) return std::nullopt;

  const std::optional<bdd> lambda_c = t_c.lambda(q_c, v);
  if (!lambda_c || (v & *lambda_c) == bddfalse) return std::nullopt;

  const std::optional<unsigned> d_phi = dfa_delta(dfa, s_phi, v);
  const std::optional<unsigned> d_in = t_in.delta(q_in, v);
  const std::optional<unsigned> d_out = t_out.delta(q_out, v);
  const std::optional<unsigned> d_c = t_c.delta(q_c, v);
  if (!d_phi || !d_in || !d_out || !d_c) return std::nullopt;

  return ProductState{*d_phi, *d_in, *d_out, *d_c};
}

// One product state's outgoing structure: Acc(s), whether some Ifree has no
// agreeing letter (hasDeadEnd), and --- indexed by the Ifree combo's
// enumeration index --- the (letter, successor) an agreeing letter gives.
struct StateInfo {
  bool acc = false;
  bool has_dead_end = false;
  std::vector<std::optional<std::pair<bdd, ProductState>>> edges;
};

// Build the reachable product A_phi x T_in x T_out x T_C by brute-force full-
// letter enumeration at each state (docs/prd/controller-verifier.md
// "Product-letter enumeration": accepted DfaProduct-parity baseline cost).
std::map<ProductState, StateInfo> build_product(
    const spot::twa_graph_ptr& dfa, const Transducer& t_in,
    const Transducer& t_out, const Transducer& t_c,
    const std::vector<bdd>& letters, std::size_t n_ifree,
    const ProductState& init) {
  std::map<ProductState, StateInfo> graph;
  std::queue<ProductState> worklist;

  auto discover = [&](const ProductState& s) {
    if (graph.emplace(s, StateInfo{}).second) worklist.push(s);
  };
  discover(init);

  const std::size_t n_ifree_combos = std::size_t{1} << n_ifree;
  const std::size_t ifree_mask = n_ifree_combos - 1;

  while (!worklist.empty()) {
    const ProductState cur = worklist.front();
    worklist.pop();

    StateInfo info;
    info.acc = dfa->state_is_accepting(std::get<0>(cur));
    info.edges.assign(n_ifree_combos, std::nullopt);

    for (std::size_t k = 0; k < letters.size(); ++k) {
      const std::optional<ProductState> succ =
          agreeing_successor(dfa, t_in, t_out, t_c, cur, letters[k]);
      if (!succ) continue;
      discover(*succ);
      const std::size_t ifree_idx = k & ifree_mask;
      // Determinism of lambda_in/lambda_c/lambda_out (docs/prd/
      // controller-verifier.md) means at most one letter agrees per Ifree
      // combo; keep the first found if that invariant were ever violated.
      if (!info.edges[ifree_idx]) info.edges[ifree_idx] = {letters[k], *succ};
    }

    for (const auto& e : info.edges)
      if (!e) { info.has_dead_end = true; break; }

    graph[cur] = std::move(info);
  }
  return graph;
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
  const std::vector<bdd> letters = all_letters(io_vars);

  const ProductState init{dfa->get_init_state_number(), t_in.initial_state(),
                          t_out.initial_state(), t_c.initial_state()};
  const std::map<ProductState, StateInfo> graph =
      build_product(dfa, t_in, t_out, t_c, letters, n_ifree, init);
  const std::set<ProductState> bad = compute_bad(graph);

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
