#include "ltlf_ek/product.hpp"

#include <cassert>
#include <cstddef>
#include <queue>
#include <set>
#include <stdexcept>
#include <string>

#include "ltlf_ek/consistency.hpp"

namespace ltlf_ek {

namespace {

// Every full letter v in 2^{io_vars} over io_vars, LSB-first in io_vars
// order --- the accepted exponential \Sigma of alg:dfa_product (symbolic
// build deferred).  File-local: LetterAlphabet owns the ordering, so callers
// go through it, never this helper directly (absorbs the former public
// all_letters, docs/prd/architecture-cleanup.md).
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

// Recursively enumerate the cartesian product of every transducer's
// (already lambda-narrowed) delta-edges out of the current ProductState, then
// OR-accumulate the resulting guard into out_guards[<goal_dst, dst_taus...>].
// taus.size()-deep recursion, mirroring ProductState's N-transducer
// generality; `guard` is dropped as soon as it hits bddfalse (short-circuit).
void combine_taus(
    const std::vector<std::vector<std::pair<bdd, unsigned>>>& effective_edges,
    std::size_t i, bdd guard, std::vector<unsigned>& dst_taus,
    unsigned goal_dst, std::map<ProductState, bdd>& out_guards) {
  if (guard == bddfalse) return;
  if (i == effective_edges.size()) {
    out_guards[ProductState{goal_dst, dst_taus}] |= guard;
    return;
  }
  for (const auto& [g, d] : effective_edges[i]) {
    dst_taus[i] = d;
    combine_taus(effective_edges, i + 1, guard & g, dst_taus, goal_dst,
                 out_guards);
  }
}

// One source ProductState's symbolic successors (docs/prd/symbolic-dfa-
// product.md "The symbolic reformulation"): per transducer, narrow its
// delta_edges(q_i) guards by emits_region(q_i) --- both read at the SOURCE
// state q_i, not the destination --- then cross with the Goal's out-edges,
// OR-accumulating per destination.  Asserts Goal completeness (invariant 3).
void symbolic_successors(const spot::twa_graph_ptr& goal,
                         const std::vector<const Transducer*>& taus,
                         const ProductState& state,
                         std::map<ProductState, bdd>& out_guards) {
  std::vector<std::vector<std::pair<bdd, unsigned>>> effective(taus.size());
  for (std::size_t i = 0; i < taus.size(); ++i) {
    const unsigned q = state.taus[i];
    const bdd er = taus[i]->emits_region(q);
    bdd seen = bddfalse;  // union of raw delta_edges guards seen at q so far.
    for (const auto& [g, d] : taus[i]->delta_edges(q)) {
      // Mirror the per-letter delta()'s non-determinism throw
      // (OutputLabeledTransducer::delta): deterministic delta has pairwise-
      // disjoint out-guards, which is what makes combine_taus's per-dst OR
      // exact.  Checked on the RAW guard g (not the emits-narrowed eg), so an
      // overlap masked by the lambda intersection still trips.
      if ((g & seen) != bddfalse)
        throw std::runtime_error(
            "build_product_symbolic: non-deterministic transducer delta "
            "(overlapping delta_edges guards) at state " + std::to_string(q));
      seen |= g;
      const bdd eg = g & er;
      if (eg != bddfalse) effective[i].emplace_back(eg, d);
    }
  }

  // One pass over the Goal out-edges: cross each with the transducers and
  // accumulate the guard-union for the completeness check.  Invariant 3:
  // ltlf_to_dfa returns a complete DFA, so that union is bddtrue --- assert it
  // (the symbolic analogue of agreeing_successor's goal_must_be_complete throw)
  // rather than silently dropping enabled letters.  On the throw path the
  // partial out_guards is discarded with the aborted build, so combining
  // before the check is harmless.
  std::vector<unsigned> dst_taus(taus.size());
  bdd goal_union = bddfalse;
  for (const auto& e : goal->out(state.goal)) {
    goal_union |= e.cond;
    combine_taus(effective, 0, e.cond, dst_taus, e.dst, out_guards);
  }
  if (goal_union != bddtrue)
    throw std::runtime_error(
        "build_product_symbolic: Goal DFA is not complete at state " +
        std::to_string(state.goal) + " (expected complete)");
}

}  // namespace

bool operator<(const ProductState& a, const ProductState& b) {
  if (a.goal != b.goal) return a.goal < b.goal;
  return a.taus < b.taus;
}

bool operator==(const ProductState& a, const ProductState& b) {
  return a.goal == b.goal && a.taus == b.taus;
}

std::optional<unsigned> goal_delta(const spot::twa_graph_ptr& goal, unsigned s,
                                   bdd v) {
  for (const auto& e : goal->out(s))
    if ((v & e.cond) != bddfalse) return e.dst;
  return std::nullopt;
}

std::vector<unsigned> goal_delta_set(const spot::twa_graph_ptr& goal,
                                     unsigned s, bdd v) {
  std::vector<unsigned> dsts;
  for (const auto& e : goal->out(s))
    if ((v & e.cond) != bddfalse) dsts.push_back(e.dst);
  return dsts;
}

LetterAlphabet::LetterAlphabet(const VariablePartition& vars,
                               const spot::twa_graph_ptr& registrar) {
  std::vector<int> io_vars;
  io_vars.reserve(vars.universe().size());
  for (const auto& n : vars.input_free)
    io_vars.push_back(registrar->register_ap(n));
  n_ifree_ = io_vars.size();
  for (const auto& n : vars.input_known)
    io_vars.push_back(registrar->register_ap(n));
  for (const auto& n : vars.output_free)
    io_vars.push_back(registrar->register_ap(n));
  for (const auto& n : vars.output_known)
    io_vars.push_back(registrar->register_ap(n));
  letters_ = all_letters(io_vars);
}

std::size_t LetterAlphabet::ifree_index(std::size_t idx) const {
  assert(idx < size());
  return idx & (n_ifree_combos() - 1);
}

std::optional<ProductState> agreeing_successor(
    const spot::twa_graph_ptr& goal,
    const std::vector<const Transducer*>& taus, const ProductState& state,
    bdd v, bool goal_must_be_complete) {
  std::vector<unsigned> next_taus;
  next_taus.reserve(taus.size());
  for (std::size_t i = 0; i < taus.size(); ++i) {
    const Transducer& t = *taus[i];
    const unsigned q = state.taus[i];
    // enabled (def:consistency §203 + §211 partiality note): emits (the
    // lambda half) AND delta defined.
    if (!emits(t, q, v)) return std::nullopt;
    const std::optional<unsigned> d = t.delta(q, v);
    if (!d) return std::nullopt;
    next_taus.push_back(*d);
  }

  // Goal edge is consulted ONLY after the transducer filter passes, so the
  // throw fires only on letters the transducer filter has already found
  // enabled, matching DfaProduct's completeness invariant.
  const std::optional<unsigned> g = goal_delta(goal, state.goal, v);
  if (!g) {
    if (goal_must_be_complete)
      throw std::runtime_error(
          "agreeing_successor: Goal automaton delta undefined on an "
          "enabled letter (expected complete)");
    return std::nullopt;
  }
  return ProductState{*g, std::move(next_taus)};
}

std::map<ProductState, ProductNode> build_product(
    const spot::twa_graph_ptr& goal,
    const std::vector<const Transducer*>& taus, const ProductState& init,
    const LetterAlphabet& alphabet, bool goal_must_be_complete) {
  std::map<ProductState, ProductNode> graph;
  std::queue<ProductState> worklist;

  graph.emplace(init, ProductNode{goal->state_is_accepting(init.goal), {}});
  worklist.push(init);

  const std::vector<bdd>& letters = alphabet.letters();
  while (!worklist.empty()) {
    const ProductState cur = worklist.front();
    worklist.pop();
    // Reference stays valid: std::map insertion never invalidates existing
    // elements' references (only erasure does).
    ProductNode& node = graph.at(cur);

    for (std::size_t idx = 0; idx < letters.size(); ++idx) {
      const std::optional<ProductState> next = agreeing_successor(
          goal, taus, cur, letters[idx], goal_must_be_complete);
      if (!next) continue;
      if (!graph.count(*next)) {
        graph.emplace(*next,
                      ProductNode{goal->state_is_accepting(next->goal), {}});
        worklist.push(*next);
      }
      node.edges.emplace_back(idx, *next);
    }
  }
  return graph;
}

ProductGuards build_product_symbolic(
    const spot::twa_graph_ptr& goal,
    const std::vector<const Transducer*>& taus, const ProductState& init) {
  ProductGuards pg;
  std::queue<ProductState> worklist;

  pg.nodes.emplace(init, std::pair<bool, std::map<ProductState, bdd>>{
                             goal->state_is_accepting(init.goal), {}});
  worklist.push(init);

  while (!worklist.empty()) {
    const ProductState cur = worklist.front();
    worklist.pop();
    // Reference stays valid: std::map insertion never invalidates existing
    // elements' references (only erasure does) --- same reasoning as
    // build_product's `node` reference below.
    std::map<ProductState, bdd>& dst_guards = pg.nodes.at(cur).second;

    symbolic_successors(goal, taus, cur, dst_guards);

    for (const auto& [dst, guard] : dst_guards) {
      if (!pg.nodes.count(dst)) {
        pg.nodes.emplace(dst, std::pair<bool, std::map<ProductState, bdd>>{
                                  goal->state_is_accepting(dst.goal), {}});
        worklist.push(dst);
      }
    }
  }
  return pg;
}

ProductGuards build_product_nondet(
    const spot::twa_graph_ptr& goal,
    const std::vector<const Transducer*>& taus, const ProductState& init,
    const LetterAlphabet& alphabet) {
  ProductGuards pg;
  std::queue<ProductState> worklist;

  pg.nodes.emplace(init, std::pair<bool, std::map<ProductState, bdd>>{
                             goal->state_is_accepting(init.goal), {}});
  worklist.push(init);

  const std::vector<bdd>& letters = alphabet.letters();
  while (!worklist.empty()) {
    const ProductState cur = worklist.front();
    worklist.pop();
    // Reference stays valid across the inserts below: std::map insertion
    // never invalidates existing elements' references (only erasure does) ---
    // same reasoning as build_product_symbolic's dst_guards reference.
    std::map<ProductState, bdd>& dst_guards = pg.nodes.at(cur).second;

    for (const bdd& v : letters) {
      // cons filter (def:consistency): emits AND delta defined, per
      // transducer --- a failing filter skips v (no edge), the same per-tau
      // loop as agreeing_successor.
      std::vector<unsigned> next_taus;
      next_taus.reserve(taus.size());
      bool filtered = false;
      for (std::size_t i = 0; i < taus.size(); ++i) {
        const Transducer& t = *taus[i];
        const unsigned q = cur.taus[i];
        if (!emits(t, q, v)) {
          filtered = true;
          break;
        }
        const std::optional<unsigned> d = t.delta(q, v);
        if (!d) {
          filtered = true;
          break;
        }
        next_taus.push_back(*d);
      }
      if (filtered) continue;

      // For EVERY goal successor s' in delta_N(s, v) (goal is complete by
      // precondition, so this is non-empty): OR v into the guard of
      // <s', next_taus...>.
      for (unsigned s2 : goal_delta_set(goal, cur.goal, v)) {
        ProductState dst{s2, next_taus};
        dst_guards[dst] |= v;
        if (!pg.nodes.count(dst)) {
          pg.nodes.emplace(dst, std::pair<bool, std::map<ProductState, bdd>>{
                                    goal->state_is_accepting(dst.goal), {}});
          worklist.push(dst);
        }
      }
    }
  }
  return pg;
}

ProductGuards to_guard_map(const std::map<ProductState, ProductNode>& graph,
                           const LetterAlphabet& alphabet) {
  const std::vector<bdd>& letters = alphabet.letters();
  ProductGuards pg;
  for (const auto& [state, node] : graph) {
    std::map<ProductState, bdd> guards;
    for (const auto& [idx, succ] : node.edges)
      guards[succ] |= letters[idx];  // bdd default-constructs to bddfalse.
    pg.nodes.emplace(state, std::pair<bool, std::map<ProductState, bdd>>{
                                node.acc, std::move(guards)});
  }
  return pg;
}

spot::twa_graph_ptr materialize_product(const ProductGuards& pg,
                                        const ProductState& init,
                                        const spot::bdd_dict_ptr& dict,
                                        const VariablePartition& vars) {
  spot::twa_graph_ptr product = spot::make_twa_graph(dict);
  for (const auto& n : vars.universe()) product->register_ap(n);
  product->set_buchi();
  product->prop_state_acc(true);

  const spot::acc_cond::mark_t kFinalMark = {0};
  const spot::acc_cond::mark_t kNoMark = {};

  std::map<ProductState, unsigned> index;
  for (const auto& [state, node] : pg.nodes) index.emplace(state, product->new_state());
  product->set_init_state(index.at(init));

  for (const auto& [state, node] : pg.nodes) {
    const unsigned src = index.at(state);
    const auto& [acc, guards] = node;
    // F_P = F_D x Q_1 x ... (alg:dfa_product:final): state-based, so mark
    // every out-edge.
    const spot::acc_cond::mark_t mark = acc ? kFinalMark : kNoMark;
    for (const auto& [dst, guard] : guards)
      product->new_edge(src, index.at(dst), guard, mark);
  }
  return product;
}

void validate_product_inputs(const spot::formula& phi,
                             const VariablePartition& vars,
                             const std::vector<const Transducer*>& taus) {
  const std::set<std::string> universe = vars.universe();
  for (const auto& ap : collect_aps(phi))
    if (!universe.count(ap))
      throw std::invalid_argument(
          "validate_product_inputs: formula AP '" + ap +
          "' outside I∪O (the partition is the closed universe of APs)");

  if (taus.empty()) return;
  const spot::bdd_dict_ptr dict = taus.front()->dict();
  for (const Transducer* t : taus)
    if (t->dict() != dict)
      throw std::invalid_argument(
          "validate_product_inputs: all transducers must share one "
          "bdd_dict");
}

}  // namespace ltlf_ek
