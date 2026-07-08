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

}  // namespace ltlf_ek
