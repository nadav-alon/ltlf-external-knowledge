#pragma once

#include <cstddef>
#include <map>
#include <optional>
#include <utility>
#include <vector>

#include <bddx.h>
#include <spot/tl/formula.hh>
#include <spot/twa/twagraph.hh>

#include "ltlf_ek/transducer.hpp"
#include "ltlf_ek/variables.hpp"

namespace ltlf_ek {

// Product state over a Goal automaton x an ordered list of transducers
// (docs/GLOSSARY.md "Product", generalized from <s, q_in, q_out> to
// <s, q_1, ..., q_n>).  taus[i] is the state of the i-th transducer in the
// SAME order the caller passes the transducer list to build_product /
// agreeing_successor.
struct ProductState {
  unsigned goal;
  std::vector<unsigned> taus;
};

// Lexicographic on (goal, taus), for use as a std::map key.
bool operator<(const ProductState& a, const ProductState& b);
bool operator==(const ProductState& a, const ProductState& b);

// Goal automaton delta as a bare (deterministic) transition structure ---
// acceptance ignored, the same idiom as OutputLabeledTransducer::delta.
// nullopt = no matching edge.  Shared by both call sites (DfaProduct /
// verify_controller) as the goal-automaton transition step.
std::optional<unsigned> goal_delta(const spot::twa_graph_ptr& goal, unsigned s,
                                   bdd v);

// The full-letter alphabet Sigma = 2^{I union O} (docs/GLOSSARY.md "Letter
// alphabet"), materialized as an explicitly enumerated vector of letters over
// a fixed Ifree-first variable order --- so a letter index's low bits are
// exactly its Ifree combination.  Registers every AP of `vars` on `registrar`
// itself (register_ap is idempotent for APs already on the dict): the
// Ifree-first ordering is THIS class's invariant, not a caller obligation ---
// replacing the former io_vars-ordering comment-contract between the
// Product core and the Controller verifier.
class LetterAlphabet {
 public:
  // Registers every AP of `vars` on `registrar` in fixed block order:
  // input_free, input_known, output_free, output_known --- each block in
  // std::set (lexicographic) order.
  LetterAlphabet(const VariablePartition& vars,
                 const spot::twa_graph_ptr& registrar);

  // All 2^|I union O| full letters, LSB-first in registration order.  Empty
  // universe => {bddtrue} (size() == 1).
  const std::vector<bdd>& letters() const { return letters_; }
  std::size_t size() const { return letters_.size(); }

  // 2^|Ifree|.  Empty Ifree => 1.
  std::size_t n_ifree_combos() const { return std::size_t{1} << n_ifree_; }

  // The Ifree combination idx's low bits encode --- a plain mask, no bounds
  // check beyond the assert.  Precondition: idx < size().  Empty Ifree =>
  // always 0.
  std::size_t ifree_index(std::size_t idx) const;

 private:
  std::vector<bdd> letters_;
  std::size_t n_ifree_ = 0;
};

// Lazy per-letter core (also serves on-the-fly Methods 3.x later).  For each
// tau_i: emits(tau_i, state.taus[i], v) AND delta_i defined; then goal_delta.
// Returns the successor ProductState iff v is enabled at `state`, else
// nullopt.
//
// goal_must_be_complete: if the transducer filter passes but goal_delta
// MISSES,
//   true  => throw std::runtime_error (DfaProduct's completeness invariant),
//   false => return nullopt (a legitimate non-agreement, the verifier's
//            case).
// The goal edge is consulted ONLY after the transducer filter passes --- so
// the throw fires only on letters the transducer filter has already found
// enabled, matching DfaProduct's completeness invariant.
std::optional<ProductState> agreeing_successor(
    const spot::twa_graph_ptr& goal,
    const std::vector<const Transducer*>& taus, const ProductState& state,
    bdd v, bool goal_must_be_complete);

// One product node: its Goal-acceptance flag and its agreeing edges.  Each
// edge stores the letter's INDEX into the LetterAlphabet (not the bdd):
// DfaProduct ORs alphabet.letters()[idx] into a per-dst guard; the verifier
// reads alphabet.ifree_index(idx) for the Ifree combo --- preserving the
// verifier's bitmask bucketing with zero extra bdd ops.  Edge order follows
// `alphabet.letters()`; at most one edge per Ifree combo holds by
// lambda-determinism (build_product does not enforce it).
struct ProductNode {
  bool acc;
  std::vector<std::pair<std::size_t, ProductState>> edges;
};

// Eager driver: worklist BFS from `init`, calling agreeing_successor over
// `alphabet.letters()`, returning the whole reachable product as a neutral
// map.  No visitor --- both current consumers materialize the entire product
// anyway (DfaProduct a twa_graph, the verifier a global nu-fixpoint), so pull
// beats push.
std::map<ProductState, ProductNode> build_product(
    const spot::twa_graph_ptr& goal,
    const std::vector<const Transducer*>& taus, const ProductState& init,
    const LetterAlphabet& alphabet, bool goal_must_be_complete);

// Shared validation preamble both call sites run: formula APs subseteq
// I union O; all transducers share one bdd_dict.  Throws
// std::invalid_argument on an out-of-universe AP or a dict mismatch.
void validate_product_inputs(const spot::formula& phi,
                             const VariablePartition& vars,
                             const std::vector<const Transducer*>& taus);

// Neutral per-dst guard map (docs/GLOSSARY.md "Product", symbolic pieces): for
// each reachable ProductState, its Goal-acceptance flag and, per destination
// ProductState, the accumulated (OR'd) edge guard.  Both the symbolic build
// and to_guard_map (the compressed per-letter build) emit THIS type, so they
// are directly comparable --- the build-equivalence metamorphic oracle
// (docs/prd/symbolic-dfa-product.md) diffs two ProductGuards.
struct ProductGuards {
  std::map<ProductState, std::pair<bool, std::map<ProductState, bdd>>> nodes;
};

// Symbolic build (docs/prd/symbolic-dfa-product.md, "The symbolic
// reformulation"): BFS from `init`, computing per-dst guards directly from
// delta_edges/emits_region + the Goal DFA's out-edges --- no LetterAlphabet,
// no minterm loop.  For a state <s, q_1, ..., q_n>, each transducer's
// contribution is (delta guard AND emits_region) at its OWN state q_i (the
// source, not the destination); the destination's guard is the Goal out-guard
// ANDed with every transducer's contribution, accumulated (OR) per
// destination.  Assumes `goal` is complete (invariant 3): asserts, per
// visited Goal state, that its out-guards union to bddtrue, throwing
// std::runtime_error otherwise (the symbolic analogue of
// agreeing_successor's goal_must_be_complete throw).  DfaProduct-only ---
// verify_controller keeps the per-letter build_product.
ProductGuards build_product_symbolic(
    const spot::twa_graph_ptr& goal,
    const std::vector<const Transducer*>& taus, const ProductState& init);

// Compress the per-letter build's ProductNode edges into the shared
// ProductGuards type --- the `guards[dst] |= letters[idx]` loop, extracted
// out of DfaProduct::synthesize.  Reference side of the build-equivalence
// metamorphic oracle (compared against build_product_symbolic on the same
// inputs).
ProductGuards to_guard_map(const std::map<ProductState, ProductNode>& graph,
                           const LetterAlphabet& alphabet);

// Materialise the game twa_graph from the shared ProductGuards type
// (state-based Buchi, F_P on the acc flag) --- the single place product
// states/guards become an automaton, called by DfaProduct.  `init` names the
// entry ProductState: ProductGuards::nodes is a std::map (keyed for
// build-equivalence comparison, not insertion order), so it cannot itself
// mark which node is the entry --- same reason build_product's caller
// (DfaProduct::synthesize) already keeps its own `init` local for
// set_init_state.  (Deviation from the PRD's two-argument signature; see
// docs/prd/symbolic-dfa-product.md "Developer comments / PRD disagreements".)
spot::twa_graph_ptr materialize_product(const ProductGuards& pg,
                                        const ProductState& init,
                                        const spot::bdd_dict_ptr& dict);

}  // namespace ltlf_ek
