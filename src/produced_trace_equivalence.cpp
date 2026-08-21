#include "ltlf_ek/produced_trace_equivalence.hpp"

#include <algorithm>
#include <map>
#include <optional>
#include <queue>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>

#include <bddx.h>
#include <spot/twa/twagraph.hh>

#include "ltlf_ek/emits_dfa.hpp"
#include "ltlf_ek/ltlf_to_dfa.hpp"
#include "ltlf_ek/product.hpp"  // goal_delta --- reused for both automata sides.

namespace ltlf_ek {
namespace {

// The full letter alphabet for the BFS, deterministically ORDERED so the
// witness is reproducible (header doc, pinned behaviour #3/#5): Sigma0(role)
// first, then Sigma1(role), then whatever else either automaton actually
// depends on (vars.universe() plus any AP emits_dfa(tau)/ltlf_to_dfa(psi)
// happened to register --- covering a psi that strays outside vars, rather
// than leaving such a variable unenumerated and producing an ambiguous
// letter-to-edge match).
std::vector<bdd> ordered_letter_alphabet(const VariablePartition& vars,
                                         Role role,
                                         const spot::twa_graph_ptr& tau_dfa,
                                         const spot::twa_graph_ptr& psi_dfa,
                                         const spot::bdd_dict_ptr& dict) {
  const SigmaSlices slices = sigma_slices(vars, role);

  std::set<std::string> used = vars.universe();
  for (const spot::formula& ap : tau_dfa->ap()) used.insert(ap.ap_name());
  for (const spot::formula& ap : psi_dfa->ap()) used.insert(ap.ap_name());

  std::vector<std::string> order;
  std::set<std::string> seen;
  auto append_sorted = [&](const std::set<std::string>& s) {
    for (const auto& name : s)
      if (seen.insert(name).second) order.push_back(name);
  };
  append_sorted(slices.sigma0);
  append_sorted(slices.sigma1);
  append_sorted(used);

  spot::twa_graph_ptr registrar = spot::make_twa_graph(dict);
  std::vector<bdd> vars_bdd;
  vars_bdd.reserve(order.size());
  for (const auto& name : order)
    vars_bdd.push_back(bdd_ithvar(registrar->register_ap(name)));

  const std::size_t k = vars_bdd.size();
  const std::size_t n_letters = std::size_t{1} << k;
  std::vector<bdd> letters(n_letters, bddtrue);
  for (std::size_t mask = 0; mask < n_letters; ++mask) {
    bdd cube = bddtrue;
    for (std::size_t i = 0; i < k; ++i)
      cube &= ((mask >> i) & 1u) ? vars_bdd[i] : !vars_bdd[i];
    letters[mask] = cube;
  }
  return letters;
}

// A product-walk state: tau's side is -1 (kSink) once a missing edge is hit
// --- the implicit rejecting sink, pinned behaviour #1 --- else a real
// emits_dfa(tau) state; psi's side is always a real ltlf_to_dfa(psi) state
// (that automaton is complete, spot::complete_here in ltlf_to_dfa.cpp, so it
// never needs the synthetic sink).
using TauSide = int;
constexpr TauSide kSink = -1;
using Key = std::pair<TauSide, unsigned>;

}  // namespace

EquivalenceResult produced_trace_equivalent(const Transducer& tau,
                                            spot::formula psi,
                                            const VariablePartition& vars,
                                            Role role) {
  const spot::bdd_dict_ptr dict = tau.dict();

  // Pinned behaviour #1: one shared bdd_dict, this construction order.
  const spot::twa_graph_ptr tau_dfa = emits_dfa(tau, dict);
  const spot::twa_graph_ptr psi_dfa = ltlf_to_dfa(psi, dict);

  const std::vector<bdd> letters =
      ordered_letter_alphabet(vars, role, tau_dfa, psi_dfa, dict);

  auto tau_final = [&](TauSide s) {
    return s != kSink &&
           tau_dfa->state_is_accepting(static_cast<unsigned>(s));
  };
  auto psi_final = [&](unsigned s) { return psi_dfa->state_is_accepting(s); };

  EquivalenceResult result{};
  result.tau_dfa_states = tau_dfa->num_states();
  result.psi_dfa_states = psi_dfa->num_states();

  const Key init{static_cast<TauSide>(tau_dfa->get_init_state_number()),
                psi_dfa->get_init_state_number()};
  // Pinned behaviour #4: the empty word never enters the BFS/verdict below.
  result.empty_word_agrees =
      (tau_final(init.first) == psi_final(init.second));

  // Breadth-first walk of the synchronous product, letters tried in the
  // fixed order above at every dequeued state --- this both discovers states
  // in non-decreasing distance from `init` (a plain BFS invariant) AND
  // breaks ties deterministically, so the first divergent pair found is the
  // shortest witness, reproducibly (pinned behaviour #2/#3).
  std::map<Key, std::pair<Key, bdd>> parent;
  std::set<Key> visited{init};
  std::queue<Key> worklist;
  worklist.push(init);

  std::optional<Key> divergent;
  while (!worklist.empty() && !divergent) {
    const Key cur = worklist.front();
    worklist.pop();
    for (const bdd& letter : letters) {
      TauSide tau_succ = kSink;
      if (cur.first != kSink) {
        const std::optional<unsigned> d =
            goal_delta(tau_dfa, static_cast<unsigned>(cur.first), letter);
        if (d) tau_succ = static_cast<TauSide>(*d);
      }
      const std::optional<unsigned> psi_succ =
          goal_delta(psi_dfa, cur.second, letter);
      if (!psi_succ)
        throw std::logic_error(
            "produced_trace_equivalent: ltlf_to_dfa(psi) was not complete "
            "(expected complete_here to make it total)");

      const Key next{tau_succ, *psi_succ};
      if (!visited.insert(next).second) continue;  // already reached, no shorter.
      parent.emplace(next, std::make_pair(cur, letter));
      worklist.push(next);

      // Pinned behaviour #2: some REACHABLE, non-initial pair --- `next` is
      // never `init` here (BFS never re-discovers the start state).
      if (tau_final(next.first) != psi_final(next.second)) {
        divergent = next;
        break;
      }
    }
  }

  result.product_states = static_cast<unsigned>(visited.size());

  if (!divergent) {
    result.equivalent_on_nonempty = true;
    result.counterexample = std::nullopt;
    return result;
  }

  result.equivalent_on_nonempty = false;
  std::vector<bdd> witness;
  Key cur = *divergent;
  while (cur != init) {
    const auto& [p, letter] = parent.at(cur);
    witness.push_back(letter);
    cur = p;
  }
  std::reverse(witness.begin(), witness.end());
  result.counterexample = std::move(witness);
  return result;
}

}  // namespace ltlf_ek
