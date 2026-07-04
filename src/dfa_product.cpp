#include "ltlf_ek/dfa_product.hpp"

#include <cstddef>
#include <map>
#include <queue>
#include <set>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

#include <bddx.h>
#include <spot/twa/twagraph.hh>

#include "ltlf_ek/consistency.hpp"
#include "ltlf_ek/ltlf_to_dfa.hpp"
#include "ltlf_ek/solve_dfa.hpp"
#include "ltlf_ek/variables.hpp"

namespace ltlf_ek {
namespace {

// ProductState = <s, q_in, q_out> --- glossary "Product".  The sink ⊥ is not a
// ProductState; it is a reserved twa_graph state index (kSink).
using ProductState = std::tuple<unsigned, unsigned, unsigned>;

// delta_D(s, v): navigate the (complete, deterministic) Goal DFA as a plain
// transition structure --- acceptance ignored, same idiom as
// OutputLabeledTransducer::delta.  Total after ltlf_to_dfa's complete_here.
unsigned dfa_delta(const spot::twa_graph_ptr& dfa, unsigned s, bdd v) {
  for (const auto& e : dfa->out(s))
    if ((v & e.cond) != bddfalse) return e.dst;
  throw std::runtime_error(
      "DfaProduct: Goal DFA delta undefined on a letter (expected complete)");
}

// Every full letter v ∈ 2^{I∪O}: the exponential \For of alg:dfa_product (the
// accepted, documented baseline cost --- symbolic construction is deferred).
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

std::optional<Controller> DfaProduct::synthesize(const spot::formula& phi,
                                                 const VariablePartition& vars,
                                                 const Transducer& t_in,
                                                 const Transducer& t_out) {
  // --- Validation (PRD "Validation policy"): phi's APs ⊆ I∪O, one shared dict.
  std::set<std::string> universe = vars.inputs();
  const std::set<std::string> outs = vars.outputs();
  universe.insert(outs.begin(), outs.end());
  for (const auto& ap : collect_aps(phi))
    if (!universe.count(ap))
      throw std::invalid_argument(
          "DfaProduct::synthesize: formula AP '" + ap +
          "' outside I∪O (the partition is the closed universe of APs)");

  const spot::bdd_dict_ptr dict = t_in.dict();
  if (t_out.dict() != dict)
    throw std::invalid_argument(
        "DfaProduct::synthesize: T_in and T_out must share one bdd_dict");

  // --- LtlfToDfa: A on the shared dict (accepting states = F_D). ---
  const spot::twa_graph_ptr dfa = ltlf_to_dfa(phi, dict);

  // --- Product P: explicit twa_graph over ProductState ∪ {kSink} (state-based
  //     Büchi, accepting = F_P), matching alg:dfa_product line-for-line. ---
  spot::twa_graph_ptr product = spot::make_twa_graph(dict);
  std::vector<int> io_vars;
  io_vars.reserve(universe.size());
  for (const auto& n : universe) io_vars.push_back(product->register_ap(n));
  product->set_buchi();
  product->prop_state_acc(true);

  // kSink: reserved, non-accepting, self-loops on every letter
  // (alg:dfa_product:self_loop).  Recorded so solve_dfa can drop it.
  const unsigned kSink = product->new_state();
  product->new_edge(kSink, kSink, bddtrue, {});
  product->set_named_prop(kSinkProperty, new unsigned(kSink));

  const std::vector<bdd> letters = all_letters(io_vars);
  const spot::acc_cond::mark_t kFinalMark = {0};
  const spot::acc_cond::mark_t kNoMark = {};

  std::map<ProductState, unsigned> index;
  std::queue<ProductState> worklist;

  const ProductState init{dfa->get_init_state_number(), t_in.initial_state(),
                          t_out.initial_state()};
  const unsigned init_idx = product->new_state();
  index.emplace(init, init_idx);
  product->set_init_state(init_idx);
  worklist.push(init);

  while (!worklist.empty()) {
    const ProductState cur = worklist.front();
    worklist.pop();
    const auto [s, q_in, q_out] = cur;
    const unsigned src = index.at(cur);

    // F_P = F_D × Q_in × Q_out (alg:dfa_product:final): the product state is
    // accepting iff its DFA component is.  State-based, so mark every out-edge.
    const bool final = dfa->state_is_accepting(s);
    const spot::acc_cond::mark_t mark = final ? kFinalMark : kNoMark;

    // Group letters sharing a destination into one guarded edge (OR of letters).
    std::map<unsigned, bdd> guards;  // dst state index -> accumulated guard.
    for (const bdd& v : letters) {
      // A letter is *enabled* iff delta_in, delta_out are defined and cons holds
      // (def:enabled; consistent() covers the lambda-definedness half).  Only
      // dereference delta after the enabled test --- never before.
      const std::optional<unsigned> d_in = t_in.delta(q_in, v);
      const std::optional<unsigned> d_out = t_out.delta(q_out, v);
      unsigned dst;
      if (d_in && d_out && consistent(t_in, q_in, t_out, q_out, v)) {
        const ProductState next{dfa_delta(dfa, s, v), *d_in, *d_out};
        auto it = index.find(next);
        if (it == index.end()) {
          const unsigned ni = product->new_state();
          index.emplace(next, ni);
          worklist.push(next);
          dst = ni;
        } else {
          dst = it->second;
        }
      } else {
        dst = kSink;  // non-enabled letter -> ⊥ (alg:dfa_product:non_cons).
      }
      guards[dst] |= v;  // bdd default-constructs to bddfalse.
    }

    for (const auto& [dst, guard] : guards)
      product->new_edge(src, dst, guard, mark);
  }

  // --- SolveDfa: solve the product game and lift the controller. ---
  return solve_dfa(product, vars);
}

}  // namespace ltlf_ek
