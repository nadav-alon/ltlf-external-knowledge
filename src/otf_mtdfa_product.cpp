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
#include <unordered_set>
#include <utility>
#include <vector>

#include <spot/misc/bddlt.hh>
#include <spot/twaalgos/backprop.hh>

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

// The incremental game behind otf_solve_fused: a spot::backprop_graph fed one
// expanded row at a time, in the shape mtdfa_to_backprop gives a finished
// mtdfa (Spot's own fused loop, ltlf_to_mtdfa, does the same).
//   - one vertex per root, with a single edge to the vertex of its MTBDD root;
//   - one vertex per internal MTBDD node, keyed on the `bdd` handle (never a
//     raw id, which BuDDy recycles) and owned by the controllability of the
//     node's variable;
//   - bddtrue and bddfalse are pre-determined vertices; an accepting terminal
//     maps to the true vertex and a non-accepting terminal 2j to root j's.
// A root's vertex is left unfrozen until its row is fed, so an unexpanded
// state stays undetermined.
class OnlineGame {
 public:
  OnlineGame(const VariablePartition& vars, const spot::bdd_dict_ptr& dict) {
    // Same controllable set solve_mtdfa installs; computed from the
    // variables themselves, since bdd_mt_quantify_prepare's buffer is wiped
    // whenever a BDD variable is created.
    std::set<std::string> controllable = vars.output_free;
    const std::set<std::string> known = vars.known();
    controllable.insert(known.begin(), known.end());
    for (const std::string& name : controllable)
      controllable_.insert(dict->varnum(spot::formula::ap(name)));

    true_ = graph_.new_state(true);
    graph_.set_winner(true_, true);
    false_ = graph_.new_state(false);
    graph_.set_winner(false_, false);
    root_vertex(0);
  }

  // Feed the expanded row of root `index`.
  void add_row(unsigned index, const bdd& row) {
    const unsigned src = root_vertex(index);
    const unsigned dst = node_vertex(row);
    graph_.new_edge(src, dst);
    graph_.freeze_state(src);
  }

  // new_edge reports a determined vertex 0 only when it is reached as a
  // predecessor, so a row that decides it directly (bddtrue / bddfalse) is
  // only visible through is_determined.
  bool decided() const { return graph_.is_determined(root_vertex_.at(0)); }
  bool system_wins() const { return graph_.winner(root_vertex_.at(0)); }

 private:
  unsigned root_vertex(unsigned index) {
    if (index >= root_vertex_.size()) root_vertex_.resize(index + 1, kNone);
    if (root_vertex_[index] == kNone)
      root_vertex_[index] = graph_.new_state(false);
    return root_vertex_[index];
  }

  unsigned node_vertex(const bdd& node) {
    if (node == bddtrue) return true_;
    if (node == bddfalse) return false_;
    if (bdd_is_terminal(node)) {
      const int t = bdd_get_terminal(node);
      return (t & 1) ? true_ : root_vertex(static_cast<unsigned>(t / 2));
    }
    if (auto it = node_vertex_.find(node); it != node_vertex_.end())
      return it->second;
    const unsigned v = graph_.new_state(controllable_.count(bdd_var(node)) > 0);
    // Children first: a vertex is only given edges once both are known.
    const unsigned lo = node_vertex(bdd_low(node));
    const unsigned hi = node_vertex(bdd_high(node));
    node_vertex_.emplace(node, v);
    graph_.new_edge(v, lo);
    if (!graph_.is_determined(v)) graph_.new_edge(v, hi);
    graph_.freeze_state(v);
    return v;
  }

  static constexpr unsigned kNone = static_cast<unsigned>(-1);
  spot::backprop_graph graph_;
  std::unordered_set<int> controllable_;
  std::unordered_map<bdd, unsigned, spot::bdd_hash> node_vertex_;
  std::vector<unsigned> root_vertex_;
  unsigned true_, false_;
};

struct ProductBuild {
  spot::mtdfa_ptr product;
  std::size_t expanded = 0;
  // Set only when `solve_online` decided the initial state before the
  // product was fully built: true = realizable.
  std::optional<bool> decided;
};

}  // namespace

// The on-the-fly product BFS; with `solve_online` it also feeds an OnlineGame row by row
// and stops as soon as the initial state is decided, leaving the discovered
// but unexpanded states padded with bddfalse.
static ProductBuild BuildProduct(const spot::formula& phi,
                                 const std::vector<const Transducer*>& taus,
                                 const VariablePartition& vars,
                                 const spot::bdd_dict_ptr& dict,
                                 bool solve_online) {
  ProductBuild build;
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
  std::optional<OnlineGame> game;
  if (solve_online) game.emplace(vars, dict);

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

    if (game) {
      game->add_row(static_cast<unsigned>(out->states.size() - 1), row_out);
      if (game->decided()) {
        build.decided = game->system_wins();
        break;
      }
    }
  }

  build.expanded = out->states.size();
  // Pad every interned index, up to the highest one, with a losing row.
  // Sound for a realizable verdict: the system already wins without them.
  out->states.resize(state_index.size(), bddfalse);
  build.product = out;
  return build;
}

// Charge table: OtfMtdfaProduct builds no Goal automaton --- no goal_*
// metric at all (B2, "Edge cases"). product_mtdfa_roots (the symbolic axis,
// NOT the explicit product_states) / product_bdd_nodes on the product handed
// to game solving; `roots` is the number of states whose row was expanded.
static void record_product_metrics(const spot::mtdfa_ptr& product,
                                   std::size_t roots) {
  record_size_metric(SizeMetric::product_mtdfa_roots, roots);
  // get_stats(nodes=true) is a full BDD-node traversal (linear in the
  // largest structure in the run) --- guard it so it never runs when no
  // BenchScope is active, matching bench.hpp's no-op contract.
  if (bench_scope_active())
    record_size_metric(SizeMetric::product_bdd_nodes,
                       product->get_stats(/*nodes=*/true, /*paths=*/false).nodes);
}

// Solves `product` under the game_solving stage; controller_states is
// absent (not zero) when unrealizable.
static std::optional<Controller> solve_and_record(const spot::mtdfa_ptr& product,
                                                  const VariablePartition& vars) {
  BenchTimer t(Stage::game_solving);
  std::optional<Controller> controller = solve_mtdfa(product, vars);
  if (controller)
    record_size_metric(SizeMetric::controller_states,
                       controller->strategy->num_states());
  return controller;
}

spot::mtdfa_ptr otf_product_to_mtdfa(const spot::formula& phi,
                                     const std::vector<const Transducer*>& taus,
                                     const VariablePartition& vars,
                                     const spot::bdd_dict_ptr& dict) {
  return BuildProduct(phi, taus, vars, dict, /*solve_online=*/false).product;
}

std::optional<Controller> otf_solve_fused(
    const spot::formula& phi, const std::vector<const Transducer*>& taus,
    const VariablePartition& vars, const spot::bdd_dict_ptr& dict,
    std::size_t* states_expanded) {
  require_turn_order_aps(vars, dict);
  ProductBuild build;
  {
    BenchTimer t(Stage::product_construction);
    build = BuildProduct(phi, taus, vars, dict, /*solve_online=*/true);
  }
  if (states_expanded) *states_expanded = build.expanded;
  // Every expanded row has been fed, so every root and node vertex is frozen:
  // a still-undetermined initial state cannot reach `true`, which is a loss.
  record_product_metrics(build.product, build.expanded);
  if (!build.decided || !*build.decided) return std::nullopt;
  return solve_and_record(build.product, vars);
}

std::optional<Controller> OtfMtdfaProduct::synthesize(const spot::formula& phi,
                                                       const VariablePartition& vars,
                                                       const Transducer& t_in,
                                                       const Transducer& t_out) {
  const std::vector<const Transducer*> taus{&t_in, &t_out};
  validate_product_inputs(phi, vars, taus);  // product.hpp preamble
  const spot::bdd_dict_ptr dict = t_in.dict();
  require_turn_order_aps(vars, dict);  // BEFORE any automaton is built

  if (otf_solve_) {
    std::size_t expanded = 0;
    auto controller = otf_solve_fused(phi, taus, vars, dict, &expanded);
    record_size_metric(kStatesExpandedMetric, expanded);
    return controller;
  }

  spot::mtdfa_ptr product;
  {
    BenchTimer t(Stage::product_construction);
    product = otf_product_to_mtdfa(phi, taus, vars, dict);
  }
  record_size_metric(kStatesExpandedMetric, product->num_roots());
  record_product_metrics(product, product->num_roots());
  return solve_and_record(product, vars);
}

}  // namespace ltlf_ek
