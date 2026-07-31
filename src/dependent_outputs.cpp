#include "ltlf_ek/dependent_outputs.hpp"

#include <cassert>
#include <deque>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <spot/twa/twagraph.hh>

#include "ltlf_ek/detail/util.hpp"
#include "ltlf_ek/ltlf_to_dfa.hpp"
#include "ltlf_ek/role.hpp"
#include "ltlf_ek/transducer_io.hpp"

namespace ltlf_ek {
namespace {

// I2: liveness is our own backward BFS, not spot::purge_dead_states (that
// primitive is Buchi dead-state semantics --- "reaches an accepting cycle" ---
// and ltlf_to_dfa's final states F_D carry no absorbing self-loop, so it would
// purge finality itself).  live(s) = some accepting state is reachable
// (forward) from s, including s itself; computed by propagating backward from
// every accepting state over the reversed edge relation.
std::vector<bool> compute_live(const spot::twa_graph_ptr& dfa) {
  const unsigned n = dfa->num_states();
  std::vector<std::vector<unsigned>> preds(n);
  for (unsigned s = 0; s < n; ++s)
    for (const auto& e : dfa->out(s)) {
      // Skip unsatisfiable guards: a bddfalse edge can never be taken, so it
      // must not propagate liveness.  These edges really do occur here (the
      // terminal accepting state of `(a<->x) & !X[!]1` carries s1 --F--> s1
      // beside s1 --true--> sink).  Guard-blind liveness would also break the
      // agreement between `live` and the live-letter region below, whose
      // `region |= e.cond` ignores such an edge either way: a state reachable
      // as live ONLY across a bddfalse edge would be live with an empty
      // region, which is the very shape compute_live_regions asserts against.
      if (e.cond != bddfalse) preds[e.dst].push_back(s);
    }

  std::vector<bool> live(n, false);
  std::deque<unsigned> queue;
  for (unsigned s = 0; s < n; ++s) {
    if (dfa->state_is_accepting(s)) {
      live[s] = true;
      queue.push_back(s);
    }
  }
  while (!queue.empty()) {
    const unsigned s = queue.front();
    queue.pop_front();
    for (unsigned p : preds[s]) {
      if (!live[p]) {
        live[p] = true;
        queue.push_back(p);
      }
    }
  }
  return live;
}

// The live-letter region \liveset{s} (docs/GLOSSARY.md "Live-letter region",
// \cref{lem:outdep-diagonal}): the union of out-edge guards out of s whose
// successor is live.  Independent of Xdep, so computed once outside the
// greedy loop (I6).
//
// An EMPTY region at a live state is legitimate, not a contradiction: `live`
// means "an accepting state is reachable from s, INCLUDING s itself", so an
// accepting state whose every successor is dead is live yet has no live
// letter.  That is precisely the terminal accepting state of a formula with a
// finite language --- phi = a & !X[!]1 gives s0 --a--> s1(acc) --true--> sink,
// and \liveset{s1} = bddfalse.  Such a state carries no constraint (no word
// continues past it), which is exactly how both consumers read bddfalse:
// is_dependent finds it vacuously functional, and the I5 totalisation defaults
// every letter there.  So the invariant is only the weaker disjunction below;
// a NON-accepting live state must still have a live successor, since that is
// the only way it could have become live.
std::vector<bdd> compute_live_regions(const spot::twa_graph_ptr& dfa,
                                      const std::vector<bool>& live) {
  const unsigned n = dfa->num_states();
  std::vector<bdd> regions(n, bddfalse);
  for (unsigned s = 0; s < n; ++s) {
    if (!live[s]) continue;
    bdd region = bddfalse;
    for (const auto& e : dfa->out(s))
      if (live[e.dst]) region |= e.cond;
    assert((dfa->state_is_accepting(s) || region != bddfalse) &&
           "compute_live_regions: a live NON-accepting state must have a live "
           "successor (that is the only way it could have become live)");
    regions[s] = region;
  }
  return regions;
}

// I1's dependency criterion for one candidate Xdep: every reachable live
// state's live-letter region, read as a relation Ydep -> candidate, must be
// functional (docs/GLOSSARY.md "Determinacy witness").  All states of `dfa`
// are reachable by construction (ltlf_to_mtdfa/as_twa only materialise
// discovered states, and spot::complete_here's sink is reached from them), so
// "reachable" needs no separate check here.
//
// Returns the first determinacy witness found, i.e. nullopt IFF the candidate
// is dependent.  Returning the witness rather than a bool costs nothing (the
// per-state check produces it either way) and is what lets CandidateObserver
// narrate a rejection without any caller re-deriving this search.
std::optional<std::string> undetermined_in_candidate(
    const std::vector<bdd>& live_regions, const std::vector<bool>& live,
    const std::set<std::string>& candidate, bdd candidate_cube,
    const spot::twa_graph_ptr& dfa) {
  const unsigned n = dfa->num_states();
  for (unsigned s = 0; s < n; ++s) {
    if (!live[s]) continue;
    if (std::optional<std::string> bad = undetermined_variable(
            live_regions[s], candidate, candidate_cube, dfa))
      return bad;
  }
  return std::nullopt;
}

}  // namespace

DependentOutputs dependent_outputs(const spot::formula& phi,
                                   const VariablePartition& partition,
                                   const spot::bdd_dict_ptr& dict,
                                   const CandidateObserver& on_candidate) {
  // I9: the tool owns output_known --- main.tex:125 has exactly one Sout/Tout,
  // so there is no "compose two Touts" notion, and an already-known output
  // would let lambda_out observe a variable produced in the same turn-order
  // phase.
  if (!partition.output_known.empty())
    throw std::invalid_argument(
        "dependent_outputs: partition.output_known must be empty (the tool "
        "owns output_known; refusing to compose two Touts)");

  // Closed-universe rule: every AP of phi must lie in I ∪ O.
  const std::set<std::string> universe = partition.universe();
  for (const auto& ap : collect_aps(phi))
    if (!universe.count(ap))
      throw std::invalid_argument(
          "dependent_outputs: formula names AP '" + ap +
          "' outside I∪O (the partition is the closed universe of APs)");

  // I2/I3: the Goal DFA, built once and reused both for the analysis (on its
  // live view) and, unmodified, as delta_out of the emitted Tout (I3: the
  // COMPLETE DFA, free since ltlf_to_dfa already calls spot::complete_here).
  const spot::twa_graph_ptr dfa = ltlf_to_dfa(phi, dict);

  // Pre-register every universe AP on dfa now, deterministically and once ---
  // rather than letting later per-candidate cube_of calls append whichever
  // candidate output happens not to occur in phi (harmless per-candidate, per
  // the Phase 1 "Developer comments" note on undetermined_variable's
  // register_ap precondition, but doing it here up front makes the mutation
  // independent of greedy order instead of contingent on it).
  for (const auto& ap : universe) dfa->register_ap(ap);

  const std::vector<bool> live = compute_live(dfa);

  // Unsatisfiable formula (Edge cases): the initial state is not live, i.e.
  // L(phi) = empty.  Every Xdep would otherwise be vacuously dependent (no
  // pair w, w' exists to violate functionality), so the greedy loop would
  // confidently return Xdep = O; detect and reject instead.
  if (!live[dfa->get_init_state_number()])
    throw UnsatisfiableFormula(
        "dependent_outputs: phi is unsatisfiable (no accepting state is "
        "reachable from the initial state)");

  const std::vector<bdd> live_regions = compute_live_regions(dfa, live);

  // I6: greedy accumulation over O in std::set<std::string> (lexicographic)
  // order.  The accumulated Xdep must be used --- singleton-union is unsound
  // (G(x<->y): {x} and {y} are each dependent, {x,y} is not).  The automaton,
  // live set and every live-letter region are Xdep-independent and already
  // computed once above; only the functionality test below consumes the
  // growing candidate.
  DependentOutputs result;
  result.partition = partition;
  for (const std::string& z : partition.outputs()) {
    std::set<std::string> candidate = result.dependent;
    candidate.insert(z);
    const bdd candidate_cube = detail::cube_of(candidate, dfa);
    const std::optional<std::string> bad = undetermined_in_candidate(
        live_regions, live, candidate, candidate_cube, dfa);
    if (!bad) result.dependent = std::move(candidate);
    if (on_candidate) on_candidate(z, !bad.has_value(), bad);
  }

  // I9: repartition O = Ofree ⊎ Xdep; I∪ (input_free/input_known) pass through
  // verbatim (I10: an input Tin, if present, does not refine this analysis).
  result.partition.output_free.clear();
  for (const auto& o : partition.outputs())
    if (!result.dependent.count(o)) result.partition.output_free.insert(o);
  result.partition.output_known = result.dependent;

  if (result.dependent.empty()) {
    // Xdep = empty (Edge cases): no Tout to build --- ltlf-ek-synth rejects a
    // --known-output-transducer when output_known is empty ("ambiguous"), so
    // a trivial_transducer here would emit a file that breaks the pipeline.
    result.t_out = std::nullopt;
    return result;
  }

  // \cref{lem:outdep-transducer}: Tout's delta is the complete Goal DFA (I3);
  // its lambda is the totalised live-letter region (I3, I5).
  const SigmaSlices slices = sigma_slices(result.partition, Role::t_out);
  const bdd sigma0_cube = detail::cube_of(slices.sigma0, dfa);
  const bdd sigma1_cube = detail::cube_of(slices.sigma1, dfa);
  const bdd xdep_cube = detail::cube_of(result.dependent, dfa);

  // I5: default_X, the all-negative cube over Xdep.  Any fixed choice is
  // sound (the defaulted letters are outside L(phi)), but it must be fixed,
  // or the emitted file is not reproducible.
  bdd default_x = bddtrue;
  for (const auto& x : result.dependent)
    default_x &= bdd_nithvar(dfa->register_ap(x));

  const unsigned n = dfa->num_states();
  std::vector<bdd> lambda_by_state(n, bddfalse);
  for (unsigned s = 0; s < n; ++s) {
    // I3: lambda_out is emitted at every state of the complete DFA, including
    // dead ones --- their live-letter region is (vacuously) bddfalse, so every
    // letter there is defaulted (Edge cases, exercised by U4).
    const bdd r_s = live[s] ? live_regions[s] : bddfalse;
    // I5: lambda_s = R_s | (!bdd_exist(R_s, Xdep_cube) & default_X).
    lambda_by_state[s] = r_s | (!bdd_exist(r_s, xdep_cube) & default_x);
  }

  result.t_out = OutputLabeledTransducer(dfa, std::move(lambda_by_state),
                                         sigma0_cube, sigma1_cube);
  return result;
}

}  // namespace ltlf_ek
