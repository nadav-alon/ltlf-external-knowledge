#include "ltlf_ek/otf_mtdfa_product.hpp"

#include <algorithm>
#include <cassert>
#include <deque>
#include <map>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "ltlf_ek/bench.hpp"
#include "ltlf_ek/product.hpp"
#include "ltlf_ek/progression.hpp"
#include "ltlf_ek/solve_mtdfa.hpp"
#include "ltlf_ek/turn_order.hpp"

namespace ltlf_ek {

namespace {

// The fused-BFS state (docs/prd/otf-mtdfa-product.md "The BFS, pinned"):
// row = fp.progress_row([psi]), the WHOLE-alphabet row of the goal formula
// [psi] a product state carries; q one transducer state per element of
// `taus`, in `taus` order.  Interned in a std::map ordered on (row.id(), q)
// --- the same subset_index/pending idiom mtnfa_product_to_mtdfa
// (src/mtnfa_product.cpp) uses, with the goal half of the key widened from a
// state subset R to a row BDD.
//
// Keying on the bdd OBJECT (not a bare int id) is load-bearing twice over:
// it is the fuse of I7.3 (Spot's fuse_same_bdds, applied componentwise
// here), and holding the handle is what stops BuDDy recycling the id ---
// the docs/prd/mtnfa.md F4 hazard, and the same reason Spot's own
// bdd_to_state is unordered_map<bdd, int, bdd_hash>.  `bdd` has no usable
// operator< of its own (its operator< is a BDD-implication operator, not a
// total order), so the comparator below orders on .id() explicitly.
struct Key {
  bdd row;
  std::vector<unsigned> q;
};

bool operator<(const Key& a, const Key& b) {
  if (a.row.id() != b.row.id()) return a.row.id() < b.row.id();
  return a.q < b.q;
}

// Relabel ("The BFS, pinned" step 5): a unary, memoized MTBDD map from a
// row (masked to one delta_edges combination's guard, from the enclosing
// state's [psi] row) to a spot::mtdfa row, parameterized by the successor
// transducer-state vector `d`.  Same skeleton as mtnfa_product.cpp's
// Relabel, widened with the two extra leaf kinds I5 introduces: `node` may
// be bddfalse, bddtrue, or a genuine terminal --- alg:otfdfa_product has no
// counterpart to this leaf triage; it is a deliberate DEVIATION (I5).
// `memo` MUST be allocated fresh per top-level call (one per combination):
// it is keyed on bdd::id(), and BuDDy recycles a node id once its last
// handle is released, so a memo hoisted to member/loop scope to "amortize
// across combinations" would be UNSOUND, not a perf tweak (F4 again).
bdd Relabel(const bdd& node, ForwardProgression& fp,
           const std::vector<unsigned>& d, std::map<Key, unsigned>& state_index,
           std::deque<Key>& pending, std::unordered_map<int, bdd>& memo) {
  if (auto it = memo.find(node.id()); it != memo.end()) return it->second;

  bdd result;
  if (node == bddfalse) {
    // I5: phi irrevocably violated --- the rejecting sink.  Language-exact,
    // a dead product state is never accepting either way.
    result = bddfalse;
  } else if (node == bddtrue) {
    // I5: phi irrevocably satisfied --- collapse to the accepting sink and
    // stop exploring this branch.  The method's largest pruning win; see the
    // PRD's I5 for what this costs (no language-equality oracle on P).
    result = bddtrue;
  } else if (bdd_is_terminal(node)) {
    const auto [psi_prime, b] = fp.decode(bdd_get_terminal(node));
    Key key{fp.progress_row(psi_prime), d};
    unsigned j;
    if (auto found = state_index.find(key); found != state_index.end()) {
      j = found->second;
    } else {
      j = static_cast<unsigned>(state_index.size());
      state_index.emplace(key, j);
      pending.push_back(std::move(key));
    }
    result = bdd_terminalpp(static_cast<int>(2 * j + (b ? 1 : 0)));
  } else {
    const int v = bdd_var(node);
    result = bdd_ite(bdd_ithvarpp(v),
                     Relabel(bdd_high(node), fp, d, state_index, pending, memo),
                     Relabel(bdd_low(node), fp, d, state_index, pending, memo));
  }
  memo.emplace(node.id(), result);
  return result;
}

// Step 3's cartesian product of `taus[k]->delta_edges(q[k])`, recursive over
// k --- the same shape mtnfa_product.cpp's ForEachCombination uses (itself
// mirroring src/product.cpp's combine_taus for build_product_symbolic).
// `guard` is dropped as soon as it hits bddfalse (short-circuit, and the
// natural handling of a delta-undefined tau at this combination).  At a leaf
// (k == edges.size()), `visit(guard, dst)` fires with the fully-combined
// guard and destination vector.
template <typename Visit>
void ForEachCombination(
    const std::vector<std::vector<std::pair<bdd, unsigned>>>& edges,
    std::size_t k, bdd guard, std::vector<unsigned>& dst, const Visit& visit) {
  if (guard == bddfalse) return;
  if (k == edges.size()) {
    visit(guard, dst);
    return;
  }
  for (const auto& [g, d] : edges[k]) {
    dst[k] = d;
    ForEachCombination(edges, k + 1, guard & g, dst, visit);
  }
}

}  // namespace

spot::mtdfa_ptr otf_product_to_mtdfa(const spot::formula& phi,
                                     const std::vector<const Transducer*>& taus,
                                     const VariablePartition& vars,
                                     const spot::bdd_dict_ptr& dict) {
  auto out = std::make_shared<spot::mtdfa>(dict);

  // Step 0: out->aps = vars.universe() (NOT phi's bare support): a
  // transducer's emits_region/delta_edges may mention APs phi never uses,
  // and the Closed universe of APs commitment makes I u O an exact
  // superset of everything any row can reference.  Sorted by formula id,
  // mirroring spot::mtdfa's documented convention / mtnfa_product.cpp.
  const std::set<std::string> universe = vars.universe();
  out->aps.reserve(universe.size());
  for (const std::string& name : universe)
    out->aps.push_back(spot::formula::ap(name));
  std::sort(out->aps.begin(), out->aps.end());

  // AP ownership (step 0, "Dropped from the interview sketch" in the PRD):
  // register THIS mtdfa's own stake in every AP it references FIRST, before
  // any row is built, so it outlives the ForwardProgression below (whose own
  // translator unregisters ITS registrations when it is destroyed at
  // function return) without a later register_ap recycling the variable
  // numbers already baked into the rows below.
  for (const spot::formula& ap : out->aps)
    dict->register_proposition(ap, out.get());

  ForwardProgression fp(dict);

  // Step 1: seed index 0 with Key{fp.progress_row(phi), q0}, q0[k] =
  // taus[k]->initial_state().  Index 0 must be the initial state ---
  // solve_mtdfa and Spot both assume it.
  std::map<Key, unsigned> state_index;
  std::deque<Key> pending;
  Key k0;
  k0.row = fp.progress_row(phi);
  k0.q.reserve(taus.size());
  for (const Transducer* tau : taus) k0.q.push_back(tau->initial_state());
  state_index.emplace(k0, 0u);
  pending.push_back(k0);

  while (!pending.empty()) {
    const Key key = std::move(pending.front());
    pending.pop_front();
    const bdd& row = key.row;
    const std::vector<unsigned>& q = key.q;

    // Step 2: cons = AND_k taus[k]->emits_region(q[k]).  If bddfalse, no
    // letter is consistent here (the partiality clause, not an error); push
    // bddfalse as this state's row and continue.
    bdd cons = bddtrue;
    for (std::size_t k = 0; k < taus.size(); ++k) cons &= taus[k]->emits_region(q[k]);

    bdd row_out = bddfalse;
    if (cons != bddfalse) {
      // Step 3: cartesian product of taus[k]->delta_edges(q[k]), seeded with
      // `cons`, short-circuiting on bddfalse.  Check per-tau guard
      // disjointness on the RAW guards and throw on overlap --- same check,
      // same wording as mtnfa_product_to_mtdfa / build_product_symbolic.
      std::vector<std::vector<std::pair<bdd, unsigned>>> edges(taus.size());
      for (std::size_t k = 0; k < taus.size(); ++k) {
        edges[k] = taus[k]->delta_edges(q[k]);
        bdd seen = bddfalse;
        for (const auto& [g, d] : edges[k]) {
          if ((g & seen) != bddfalse)
            throw std::runtime_error(
                "otf_product_to_mtdfa: non-deterministic transducer delta "
                "(overlapping delta_edges guards) at state " +
                std::to_string(q[k]));
          seen |= g;
        }
      }

      std::vector<unsigned> dst(taus.size());
      ForEachCombination(
          edges, 0, cons, dst,
          [&](bdd g, const std::vector<unsigned>& d) {
            // Step 4: mask before walking --- bdd_ite(g, row, bddfalse), NOT
            // `&` (not meaningful on multi-terminal BDDs).  Walking the
            // unmasked row would intern successor states for branches
            // occurring only OUTSIDE g, silently destroying reachability
            // pruning (pre-paid PRD-change event from
            // docs/prd/mtnfa-product.md, 2026-07-27).
            const bdd row_g = bdd_ite(g, row, bddfalse);
            std::unordered_map<int, bdd> memo;  // fresh per Relabel call (F4)
            row_out = bdd_ite(g, Relabel(row_g, fp, d, state_index, pending, memo),
                              row_out);
          });
    }

    // Step 7: BFS discovery order assigns output indices 0,1,2,... in
    // dequeue order, so `key`'s index always equals out->states.size() here.
    // Inlined into the assert so the lookup vanishes entirely under NDEBUG.
    assert(state_index.at(key) == out->states.size());
    out->states.push_back(row_out);
  }

  return out;
}

std::optional<Controller> OtfMtdfaProduct::synthesize(const spot::formula& phi,
                                                       const VariablePartition& vars,
                                                       const Transducer& t_in,
                                                       const Transducer& t_out) {
  if (otf_solve_)
    throw std::logic_error(
        "OtfMtdfaProduct: otf_solve not implemented until Phase 2");

  const std::vector<const Transducer*> taus{&t_in, &t_out};
  validate_product_inputs(phi, vars, taus);  // product.hpp preamble
  const spot::bdd_dict_ptr dict = t_in.dict();
  require_turn_order_aps(vars, dict);  // BEFORE any automaton is built

  spot::mtdfa_ptr product;
  {
    BenchTimer t(Stage::product_construction);
    product = otf_product_to_mtdfa(phi, taus, vars, dict);
  }
  // Charge table: OtfMtdfaProduct builds no Goal automaton --- no goal_*
  // metric at all (B2, "Edge cases"). product_states / product_bdd_nodes on
  // the explored product mtdfa handed to game solving.
  record_size_metric(SizeMetric::product_states, product->num_roots());
  // get_stats(nodes=true) is a full BDD-node traversal (linear in the
  // largest structure in the run) --- guard it so it never runs when no
  // BenchScope is active, matching bench.hpp's no-op contract.
  if (bench_scope_active())
    record_size_metric(SizeMetric::product_bdd_nodes,
                       product->get_stats(/*nodes=*/true, /*paths=*/false).nodes);

  std::optional<Controller> controller;
  {
    BenchTimer t(Stage::game_solving);
    controller = solve_mtdfa(product, vars);
  }
  // Charge table: controller_states, absent (not zero) when unrealizable.
  if (controller)
    record_size_metric(SizeMetric::controller_states,
                       controller->strategy->num_states());
  return controller;
}

}  // namespace ltlf_ek
