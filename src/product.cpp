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
