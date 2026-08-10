#include "ltlf_ek/mtnfa_product.hpp"

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
#include "ltlf_ek/solve_mtdfa.hpp"
#include "ltlf_ek/turn_order.hpp"

namespace ltlf_ek {

namespace {

// The fused-BFS state (docs/prd/mtnfa-product.md "Novel mechanisms (a)"): R
// a sorted, de-duplicated subset of S_N (goal states), q one transducer
// state per element of `taus`, in `taus` order.  Interned in a std::map
// keyed lexicographically on (R, q) --- the same subset_index/pending idiom
// mtnfa_to_mtdfa (src/mtnfa.cpp) uses, widened by the q component.
struct Key {
  std::vector<unsigned> R;
  std::vector<unsigned> q;
};

bool operator<(const Key& a, const Key& b) {
  if (a.R != b.R) return a.R < b.R;
  return a.q < b.q;
}

// Relabel ("Novel mechanisms (c)"): a unary, memoized MTBDD map from a
// StateSetPool-terminated `node` (a successor-subset MTBDD over the GOAL
// alone, from goal.pool.set_union) to a spot::mtdfa row, parameterized by
// the successor transducer-state vector `d`.  Same skeleton as
// src/mtnfa.cpp's RelabelRec, widened to intern Key{R, q} instead of a bare
// R.  `memo` MUST be allocated fresh per top-level call (one per (b).3
// combination): it is keyed on bdd::id(), and BuDDy recycles a node id once
// its last handle is released, so a memo hoisted to member/loop scope to
// "amortize across combinations" would be UNSOUND, not a perf tweak
// (docs/prd/mtnfa.md theory-review F4).
bdd Relabel(const bdd& node, const Mtnfa& goal, const std::vector<unsigned>& d,
           std::map<Key, unsigned>& subset_index, std::deque<Key>& pending,
           std::unordered_map<int, bdd>& memo) {
  if (auto it = memo.find(node.id()); it != memo.end()) return it->second;

  bdd result;
  if (bdd_is_terminal(node)) {
    const std::vector<unsigned>& S = goal.pool.set_of(bdd_get_terminal(node));
    if (S.empty()) {
      result = bddfalse;  // empty successor subset: the rejecting sink
    } else {
      Key key{S, d};
      unsigned j;
      if (auto found = subset_index.find(key); found != subset_index.end()) {
        j = found->second;
      } else {
        j = static_cast<unsigned>(subset_index.size());
        subset_index.emplace(key, j);
        pending.push_back(std::move(key));
      }
      const bool b = std::any_of(S.begin(), S.end(),
                                 [&](unsigned s) { return goal.accepting[s]; });
      result = bdd_terminalpp(static_cast<int>(2 * j + (b ? 1 : 0)));
    }
  } else {
    const int v = bdd_var(node);
    result = bdd_ite(bdd_ithvarpp(v),
                     Relabel(bdd_high(node), goal, d, subset_index, pending, memo),
                     Relabel(bdd_low(node), goal, d, subset_index, pending, memo));
  }
  memo.emplace(node.id(), result);
  return result;
}

// (b).3's cartesian product of `taus[k]->delta_edges(q[k])`, recursive over
// k --- the same shape src/product.cpp's combine_taus uses for
// build_product_symbolic.  `guard` is dropped as soon as it hits bddfalse
// (short-circuit, and the natural handling of a delta-undefined tau at this
// combination).  At a leaf (k == edges.size()), `visit(guard, dst)` fires
// with the fully-combined guard and destination vector.
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

spot::mtdfa_ptr mtnfa_product_to_mtdfa(const Mtnfa& goal,
                                       const std::vector<const Transducer*>& taus,
                                       const VariablePartition& vars) {
  // Precondition (F2, shared with mtnfa_to_mtdfa): R0 = {goal.initial} is
  // seeded directly at output index 0 and never rediscovered as a
  // destination subset, so Relabel never reads its acceptance bit --- an
  // accepting initial state would be silently dropped.  ltlf_to_mtnfa
  // always satisfies it (fresh non-accepting s_{N,0}).
  assert(!goal.accepting[goal.initial] &&
        "mtnfa_product_to_mtdfa: accepting initial goal state unsupported "
        "(see F2)");

  auto out = std::make_shared<spot::mtdfa>(goal.dict);

  // out->aps = vars.universe(), NOT goal.aps (which is only phi's support):
  // a transducer's emits_region/delta_edges may mention APs phi never uses,
  // and the Closed universe of APs commitment makes I u O an exact
  // superset of everything any row can reference.  Sorted by formula id,
  // mirroring spot::mtdfa's documented convention / src/mtnfa.cpp's
  // SortedAps.  Hoisted out of the loop header: universe() returns a fresh
  // std::set by value, so calling it twice (once for reserve, once for the
  // range-for) would build it twice for no reason.
  const std::set<std::string> universe = vars.universe();
  out->aps.reserve(universe.size());
  for (const std::string& name : universe)
    out->aps.push_back(spot::formula::ap(name));
  std::sort(out->aps.begin(), out->aps.end());

  // AP ownership ("Novel mechanisms (e)"): register THIS mtdfa's own stake
  // in every AP it references, so it outlives `goal` (a temporary under the
  // natural calling pattern mtnfa_product_to_mtdfa(ltlf_to_mtnfa(phi,
  // dict), ...)) without a later register_ap recycling the variable numbers
  // already baked into the rows below.  register_proposition returns the
  // already-assigned number and just adds out.get() to that variable's
  // owner list; the pairing unregister is spot::mtdfa's own destructor.
  // See docs/prd/mtnfa.md "Developer comments", entry 3.
  for (const spot::formula& ap : out->aps)
    goal.dict->register_proposition(ap, out.get());

  // BFS over reachable (R, q) pairs ("Novel mechanisms (a)"): R0 =
  // {goal.initial}, q0[k] = taus[k]->initial_state(), at output index 0 ---
  // so states[0] is the initial state, as solve_mtdfa / Spot expect.
  std::map<Key, unsigned> subset_index;
  std::deque<Key> pending;
  Key k0;
  k0.R = {goal.initial};
  k0.q.reserve(taus.size());
  for (const Transducer* tau : taus) k0.q.push_back(tau->initial_state());
  subset_index.emplace(k0, 0u);
  pending.push_back(k0);

  while (!pending.empty()) {
    const Key key = std::move(pending.front());
    pending.pop_front();
    const std::vector<unsigned>& R = key.R;
    const std::vector<unsigned>& q = key.q;

    // (b).1: row_set = fold(goal.pool.set_union, {goal.states[s] : s in R})
    // --- one MTBDD over letters whose leaf at v is the set-terminal for
    // union_{s in R} delta_N(s, v).  R is never empty (only non-empty
    // subsets are ever enqueued, see Relabel).  goal's OWN pool: the
    // terminals in goal.states are meaningful only relative to it.
    bdd row_set = goal.states[R.front()];
    for (std::size_t i = 1; i < R.size(); ++i)
      row_set = goal.pool.set_union(row_set, goal.states[R[i]]);

    // (b).2: cons = AND_k taus[k]->emits_region(q[k]).  If bddfalse, no
    // letter is consistent here (a lambda-undefined transducer state); the
    // row stays bddfalse below --- an optimization AND the natural reading
    // of the partiality clause.
    bdd cons = bddtrue;
    for (std::size_t k = 0; k < taus.size(); ++k) cons &= taus[k]->emits_region(q[k]);

    bdd row = bddfalse;
    if (cons != bddfalse) {
      // (b).3: cartesian product of taus[k]->delta_edges(q[k]), combined
      // with `cons`; skip a combination whose accumulated guard is
      // bddfalse.
      std::vector<std::vector<std::pair<bdd, unsigned>>> edges(taus.size());
      for (std::size_t k = 0; k < taus.size(); ++k) {
        edges[k] = taus[k]->delta_edges(q[k]);
        // (d): the bdd_ite accumulation below is exact only because the
        // combination guards are pairwise disjoint, and that holds iff each
        // tau's OWN delta_edges guards are --- so check the precondition at
        // its source rather than inferring it from the combined guards.
        // A runtime THROW, not an assert: Transducer is a public virtual
        // interface, so a subclass with overlapping guards would otherwise
        // silently overwrite an earlier combination's successors and yield a
        // WRONG language under NDEBUG, with no diagnostic (generic
        // code-review, 2026-07-27).  Same check, same wording and the same
        // std::runtime_error as build_product_symbolic (src/product.cpp) and
        // OutputLabeledTransducer::delta's per-letter throw.  Checked on the
        // RAW guards, so an overlap that `cons` happens to mask apart still
        // trips.  Reached only when cons != bddfalse, which is exactly when
        // it can matter: a cons-dead state's row is bddfalse whatever the
        // transducer does, so (b).2's shortcut stays a pure shortcut.
        bdd seen = bddfalse;
        for (const auto& [g, d] : edges[k]) {
          if ((g & seen) != bddfalse)
            throw std::runtime_error(
                "mtnfa_product_to_mtdfa: non-deterministic transducer delta "
                "(overlapping delta_edges guards) at state " +
                std::to_string(q[k]));
          seen |= g;
        }
      }

      std::vector<unsigned> dst(taus.size());
      ForEachCombination(
          edges, 0, cons, dst,
          [&](bdd g, const std::vector<unsigned>& d) {
            // PRD-change event, 2026-07-27 (see "Developer comments / PRD
            // disagreements"): Relabel must walk `row_set` MASKED to `g`,
            // not the unrestricted `row_set` --- otherwise it interns
            // Key{S, d} for every set-terminal reachable ANYWHERE in
            // row_set, including branches that only occur OUTSIDE `g`
            // (where the true successor state vector is a different `d`).
            // Those spurious keys get enqueued and processed even though
            // the outer bdd_ite immediately discards their branch here; the
            // language is unaffected (they are unreachable dead states) but
            // reachability pruning --- Method 1's entire selling point ---
            // is silently lost.  Masking makes row_set_g the empty-set
            // terminal outside `g`, and Relabel already maps the empty set
            // to bddfalse without interning anything.
            const bdd row_set_g = bdd_ite(g, row_set, bdd_terminalpp(0));
            std::unordered_map<int, bdd> memo;  // fresh per Relabel call (F4)
            row = bdd_ite(g, Relabel(row_set_g, goal, d, subset_index, pending, memo),
                         row);
          });
    }

    // BFS discovery order assigns output indices 0,1,2,... in dequeue
    // order, so `key`'s index always equals out->states.size() here.
    // Inlined into the assert so the lookup vanishes entirely under NDEBUG.
    assert(subset_index.at(key) == out->states.size());
    out->states.push_back(row);
  }

  return out;
}

std::optional<Controller> MtnfaProduct::synthesize(const spot::formula& phi,
                                                    const VariablePartition& vars,
                                                    const Transducer& t_in,
                                                    const Transducer& t_out) {
  const std::vector<const Transducer*> taus{&t_in, &t_out};
  validate_product_inputs(phi, vars, taus);  // product.hpp preamble
  const spot::bdd_dict_ptr dict = t_in.dict();
  require_turn_order_aps(vars, dict);  // BEFORE any automaton is built

  Mtnfa goal;
  {
    BenchTimer t(Stage::automaton_construction);
    goal = ltlf_to_mtnfa(phi, dict);
  }
  // Charge table: MtnfaProduct charges goal_nfa_states to Mtnfa::states.size().
  record_size_metric(SizeMetric::goal_nfa_states, goal.states.size());

  spot::mtdfa_ptr product;
  {
    BenchTimer t(Stage::product_construction);
    product = mtnfa_product_to_mtdfa(goal, taus, vars);
  }
  // Charge table: product and determinization are fused here (no separate
  // nfa_product_states cell); product_mtdfa_roots (the symbolic axis, NOT the
  // explicit product_states) / product_bdd_nodes on the fused product mtdfa
  // handed to game solving.
  record_size_metric(SizeMetric::product_mtdfa_roots, product->num_roots());
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
