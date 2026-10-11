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

namespace ltlf_ek {
namespace {

// One block of letters on which an automaton's behaviour at a state is
// uniform: `region` is the set of letters that take the first matching edge
// to `dst` (the first-intersecting-edge rule of goal_delta), so the blocks of
// one state partition the letter space. Working with these blocks, rather
// than with an enumerated alphabet, is what keeps the walk independent of
// how many APs the universe holds: only the distinctions the two automata
// actually draw are ever visited.
struct Block {
  int dst;
  bdd region;
};

constexpr int kSinkDst = -1;

// The blocks out of `s`; when `complete` is false the uncovered remainder is
// a final block into the implicit rejecting sink (pinned behaviour #1).
// Returns false, leaving `blocks` partial, iff `complete` is true and edges
// leave a remainder uncovered.
bool blocks_of(const spot::twa_graph_ptr& dfa, unsigned s, bool complete,
               std::vector<Block>& blocks) {
  bdd remaining = bddtrue;
  for (const auto& e : dfa->out(s)) {
    const bdd r = e.cond & remaining;
    if (r != bddfalse) blocks.push_back({static_cast<int>(e.dst), r});
    remaining &= !e.cond;
  }
  if (remaining == bddfalse) return true;
  if (complete) return false;
  blocks.push_back({kSinkDst, remaining});
  return true;
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
                                            const VariablePartition& /*vars*/,
                                            Role /*role*/) {
  const spot::bdd_dict_ptr dict = tau.dict();

  // Pinned behaviour #1: one shared bdd_dict, this construction order.
  const spot::twa_graph_ptr tau_dfa = emits_dfa(tau, dict);
  const spot::twa_graph_ptr psi_dfa = ltlf_to_dfa(psi, dict);

  auto tau_final = [&](TauSide s) {
    return s != kSink &&
           tau_dfa->state_is_accepting(static_cast<unsigned>(s));
  };
  auto psi_final = [&](unsigned s) { return psi_dfa->state_is_accepting(s); };

  EquivalenceResult result{};
  // The witness's AP variables belong to these automata; hand them over so
  // the variables stay registered as long as the result does.
  result.witness_owners = {tau_dfa, psi_dfa};
  result.tau_dfa_states = tau_dfa->num_states();
  result.psi_dfa_states = psi_dfa->num_states();

  const Key init{static_cast<TauSide>(tau_dfa->get_init_state_number()),
                psi_dfa->get_init_state_number()};
  // Pinned behaviour #4: the empty word never enters the BFS/verdict below.
  result.empty_word_agrees =
      (tau_final(init.first) == psi_final(init.second));

  // Breadth-first walk of the synchronous product, letter blocks tried in
  // the fixed edge order of the two automata at every dequeued state --- this both discovers states
  // in non-decreasing distance from `init` (a plain BFS invariant) AND
  // breaks ties deterministically, so the first divergent pair found is the
  // shortest witness, reproducibly (pinned behaviour #2/#3).
  //
  // Only the EMPTY word (zero letters) is excluded from this walk --- that
  // case is `result.empty_word_agrees` above, never folded in here. A
  // NON-EMPTY word that returns to `init` is a perfectly ordinary product
  // state and must be checked like any other: `visited` therefore does
  // *not* pre-seed `init`, so its first non-empty-word rediscovery still
  // gets the finality test below. (A pre-seeded `visited{init}` was the
  // 2026-08-21 code-review bug: it silently pruned exactly that case,
  // dropping counterexamples that only return through the initial pair ---
  // see docs/prd/engineered-domain-families.md's dated correction.)
  //
  // Parent pointers use `std::optional<Key>` rather than `Key`, because
  // `init` is not a distinguishable sentinel from an ordinary reachable
  // pair --- a length-1 witness that lands back on `init` would otherwise
  // make `parent[init]` point at itself. `std::nullopt` instead marks "no
  // letter consumed yet" (the true, virtual pre-initial root), assigned only
  // to states discovered while expanding `init` itself; every other state's
  // predecessor is a real `Key`. Reconstruction below stops on `nullopt`,
  // not on `cur == init`.
  std::map<Key, std::pair<std::optional<Key>, bdd>> parent;
  std::set<Key> visited;
  std::queue<Key> worklist;
  worklist.push(init);

  std::optional<Key> divergent;
  bool expanding_root = true;
  while (!worklist.empty() && !divergent) {
    const Key cur = worklist.front();
    worklist.pop();
    const std::optional<Key> cur_id =
        expanding_root ? std::nullopt : std::optional<Key>(cur);
    expanding_root = false;
    std::vector<Block> tau_blocks;
    if (cur.first == kSink)
      tau_blocks.push_back({kSinkDst, bddtrue});
    else
      blocks_of(tau_dfa, static_cast<unsigned>(cur.first), false, tau_blocks);
    std::vector<Block> psi_blocks;
    if (!blocks_of(psi_dfa, cur.second, true, psi_blocks))
      throw std::logic_error(
          "produced_trace_equivalent: ltlf_to_dfa(psi) was not complete "
          "(expected complete_here to make it total)");

    for (const Block& tb : tau_blocks) {
      for (const Block& pb : psi_blocks) {
        const bdd letters = tb.region & pb.region;
        if (letters == bddfalse) continue;

        const Key next{static_cast<TauSide>(tb.dst),
                       static_cast<unsigned>(pb.dst)};
        if (!visited.insert(next).second) continue;  // reached, no shorter.
        parent.emplace(next, std::make_pair(cur_id, letters));
        worklist.push(next);

        // Pinned behaviour #2 (corrected 2026-08-21): some reachable pair,
        // possibly `init` itself when reached by a non-empty word, differs
        // in finality.
        if (tau_final(next.first) != psi_final(next.second)) {
          divergent = next;
          break;
        }
      }
      if (divergent) break;
    }
  }

  // The literal root (`init`, zero letters) is always explored even when no
  // word ever returns to it, so it is not double-counted nor silently
  // dropped from "reachable pairs explored".
  result.product_states =
      static_cast<unsigned>(visited.size()) + (visited.count(init) ? 0u : 1u);

  if (!divergent) {
    result.equivalent_on_nonempty = true;
    result.counterexample = std::nullopt;
    return result;
  }

  result.equivalent_on_nonempty = false;
  std::vector<bdd> witness;
  std::optional<Key> cur = *divergent;
  while (cur) {
    const auto& [pred, letter] = parent.at(*cur);
    // Every letter of the block reaches the same pair, so any one of them
    // witnesses; bdd_satone picks it deterministically.
    witness.push_back(bdd_satone(letter));
    cur = pred;
  }
  std::reverse(witness.begin(), witness.end());
  result.counterexample = std::move(witness);
  return result;
}

}  // namespace ltlf_ek
