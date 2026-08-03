#include "ltlf_ek/detail/dependency_core.hpp"

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

namespace ltlf_ek::detail {
namespace {

// I4: liveness is our own backward BFS, not spot::purge_dead_states (that
// primitive is Buchi dead-state semantics --- "reaches an accepting cycle" ---
// and ltlf_to_dfa's final states F_D carry no absorbing self-loop, so it would
// purge finality itself).  live(s) = some accepting state is reachable
// (forward) from s, including s itself; computed by propagating backward from
// every accepting state over the reversed edge relation.  Shared by both
// directions (docs/GLOSSARY.md "Live-letter region", \cref{lem:outdep-diagonal}
// / \cref{lem:indep-diagonal}): on t_out's A_phi, live means the SYSTEM can
// still avoid losing; on t_in's A_lnot_phi (the Violation automaton), live
// means the ENVIRONMENT can still force a violation.  The BFS itself does not
// care which.
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

// The live-letter region \liveset{s} (docs/GLOSSARY.md "Live-letter region"):
// the union of out-edge guards out of s whose successor is live.  Independent
// of Xdep, so computed once outside the greedy loop (I6 / I8).
//
// An EMPTY region at a live state is legitimate, not a contradiction: `live`
// means "an accepting state is reachable from s, INCLUDING s itself", so an
// accepting state whose every successor is dead is live yet has no live
// letter.  That is precisely the terminal accepting state of a formula with a
// finite language --- phi = a & !X[!]1 gives s0 --a--> s1(acc) --true--> sink,
// and \liveset{s1} = bddfalse.  Such a state carries no constraint (no word
// continues past it), which is exactly how both consumers read bddfalse:
// is_dependent finds it vacuously functional, and the I5/I7 totalisation
// defaults every letter there.  So the invariant is only the weaker
// disjunction below; a NON-accepting live state must still have a live
// successor, since that is the only way it could have become live.
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

// I1's / \cref{lem:indep-diagonal}'s dependency criterion for one candidate
// Xdep: every reachable live state's (possibly projected, I3) region, read as
// a relation Ydep -> candidate, must be functional (docs/GLOSSARY.md
// "Determinacy witness").  All states of `dfa` are reachable by construction
// (ltlf_to_mtdfa/as_twa only materialise discovered states, and
// spot::complete_here's sink is reached from them), so "reachable" needs no
// separate check here.
//
// Returns the first determinacy witness found, i.e. nullopt IFF the candidate
// is dependent.  Returning the witness rather than a bool costs nothing (the
// per-state check produces it either way) and is what lets CandidateObserver
// narrate a rejection without any caller re-deriving this search.
std::optional<std::string> undetermined_in_candidate(
    const std::vector<bdd>& analysis_regions, const std::vector<bool>& live,
    const std::set<std::string>& candidate, bdd candidate_cube,
    const spot::twa_graph_ptr& dfa) {
  const unsigned n = dfa->num_states();
  for (unsigned s = 0; s < n; ++s) {
    if (!live[s]) continue;
    if (std::optional<std::string> bad = undetermined_variable(
            analysis_regions[s], candidate, candidate_cube, dfa))
      return bad;
  }
  return std::nullopt;
}

}  // namespace

DependencyResult run_dependency_analysis(
    const spot::formula& phi, const VariablePartition& partition, Role role,
    const spot::bdd_dict_ptr& dict, const CandidateObserver& on_candidate) {
  if (role == Role::t_c)
    throw std::invalid_argument(
        "run_dependency_analysis: Role::t_c has no dependency notion");
  const bool is_input = (role == Role::t_in);

  // I9: the role's own known-set must be empty --- main.tex has exactly one
  // S_in/T_in (resp. S_out/T_out) producing it, so there is no "compose two
  // Tins/Touts" notion, and an already-known variable would let lambda observe
  // something produced in the same turn-order phase.
  const std::set<std::string>& own_known =
      is_input ? partition.input_known : partition.output_known;
  if (!own_known.empty())
    throw std::invalid_argument(
        is_input ? "run_dependency_analysis: partition.input_known must be "
                   "empty (the tool owns input_known; refusing to compose "
                   "two Tins)"
                 : "run_dependency_analysis: partition.output_known must be "
                   "empty (the tool owns output_known; refusing to compose "
                   "two Touts)");

  // Closed-universe rule: every AP of phi must lie in I ∪ O --- this holds
  // regardless of direction, since I2's negation changes the analysed
  // automaton but not phi's own AP set.
  const std::set<std::string> universe = partition.universe();
  for (const auto& ap : collect_aps(phi))
    if (!universe.count(ap))
      throw std::invalid_argument(
          "run_dependency_analysis: formula names AP '" + ap +
          "' outside I∪O (the partition is the closed universe of APs)");

  // I2: the analysed automaton is A_phi for t_out, or the Violation automaton
  // A_lnot_phi = ltlf_to_dfa(Not(phi), dict) for t_in --- built by translating
  // the negation, never by flipping acceptance on A_phi (ltlf_to_dfa's
  // acceptance also encodes the empty/length-0 convention, so a flip is an
  // untested equivalence, not a free complement).  Both are complete
  // (ltlf_to_dfa calls spot::complete_here) and reused unmodified as delta of
  // the emitted transducer (I5).
  const spot::formula analysed = is_input ? spot::formula::Not(phi) : phi;
  const spot::twa_graph_ptr dfa = ltlf_to_dfa(analysed, dict);

  // Pre-register every universe AP on dfa now, deterministically and once ---
  // rather than letting later per-candidate cube_of calls append whichever
  // candidate variable happens not to occur in phi.
  for (const auto& ap : universe) dfa->register_ap(ap);

  const std::vector<bool> live = compute_live(dfa);

  // I11: the initial state is not live, i.e. the analysed language is empty
  // (L(phi) for t_out, L(!phi) --- phi valid --- for t_in).  Every candidate
  // would otherwise be vacuously dependent (no pair w, w' exists to violate
  // functionality), so the greedy loop would confidently return the whole
  // scanned set; detect and reject instead.
  if (!live[dfa->get_init_state_number()])
    throw UnsatisfiableFormula(
        is_input
            ? "run_dependency_analysis: phi is valid (no accepting state of "
              "the Violation automaton is reachable; the environment can "
              "never force a violation)"
            : "run_dependency_analysis: phi is unsatisfiable (no accepting "
              "state is reachable from the initial state)");

  const std::vector<bdd> live_regions = compute_live_regions(dfa, live);

  // I3: t_in's functionality test runs on the Moore-restricted, existentially
  // projected region \liveproj{s} = \exists O.\liveset{s} --- Sigma_in moves
  // before the controller, so lambda_in may not observe the current step's O.
  // t_out needs no projection: Sigma_out moves last and observes everything.
  // Quantifying by the FULL output cube (not a role-specific subset) makes an
  // empty O a no-op automatically (O5-in), since cube_of({}) = bddtrue and
  // bdd_exist over zero variables is the identity.
  std::vector<bdd> analysis_regions = live_regions;
  if (is_input) {
    const bdd output_cube = cube_of(partition.outputs(), dfa);
    for (unsigned s = 0; s < dfa->num_states(); ++s)
      if (live[s])
        analysis_regions[s] = bdd_exist(live_regions[s], output_cube);
  }

  // I6/I8: greedy accumulation over the scanned set (Ofree for t_out, Ifree
  // for t_in --- both equal the full outputs()/inputs() here, since the I9
  // check above already forced the role's own known-set to empty) in
  // std::set<std::string> (lexicographic) order.  The accumulated candidate
  // must be used --- singleton-union is unsound (G(x<->y): {x} and {y} are
  // each dependent, {x,y} is not).  The automaton, live set and every
  // (possibly projected) region are candidate-independent and already
  // computed once above; only the functionality test below consumes the
  // growing candidate.
  const std::set<std::string>& scanned =
      is_input ? partition.input_free : partition.output_free;

  DependencyResult result;
  result.partition = partition;
  for (const std::string& z : scanned) {
    std::set<std::string> candidate = result.dependent;
    candidate.insert(z);
    const bdd candidate_cube = cube_of(candidate, dfa);
    const std::optional<std::string> bad = undetermined_in_candidate(
        analysis_regions, live, candidate, candidate_cube, dfa);
    if (!bad) result.dependent = std::move(candidate);
    if (on_candidate) on_candidate(z, !bad.has_value(), bad);
  }

  // I9: repartition the role's own set into free/known; the other role's pair
  // passes through verbatim (I10).
  if (is_input) {
    result.partition.input_free.clear();
    for (const auto& i : partition.inputs())
      if (!result.dependent.count(i)) result.partition.input_free.insert(i);
    result.partition.input_known = result.dependent;
  } else {
    result.partition.output_free.clear();
    for (const auto& o : partition.outputs())
      if (!result.dependent.count(o)) result.partition.output_free.insert(o);
    result.partition.output_known = result.dependent;
  }

  if (result.dependent.empty()) {
    // Xdep = empty (Edge cases): no transducer to build --- ltlf-ek-synth
    // rejects a --known-{input,output}-transducer when the known set is empty
    // ("ambiguous"), so a trivial_transducer here would emit a file that
    // breaks the pipeline.
    result.transducer = std::nullopt;
    return result;
  }

  // \cref{lem:outdep-transducer} / \cref{lem:indep-transducer}: the emitted
  // transducer's delta is the complete analysed automaton (I5); its lambda is
  // the totalised (possibly projected) region (I3, I5, I7).
  const SigmaSlices slices = sigma_slices(result.partition, role);
  const bdd sigma0_cube = cube_of(slices.sigma0, dfa);
  const bdd sigma1_cube = cube_of(slices.sigma1, dfa);
  const bdd xdep_cube = cube_of(result.dependent, dfa);

  // I7: default_X, the all-negative cube over the dependent set.  Any fixed
  // choice is sound (the defaulted letters are outside the analysed
  // language), but it must be fixed, or the emitted file is not reproducible.
  bdd default_x = bddtrue;
  for (const auto& x : result.dependent)
    default_x &= bdd_nithvar(dfa->register_ap(x));

  const unsigned n = dfa->num_states();
  std::vector<bdd> lambda_by_state(n, bddfalse);
  for (unsigned s = 0; s < n; ++s) {
    // I5: lambda is emitted at every state of the complete automaton,
    // including dead ones --- their region is (vacuously) bddfalse, so every
    // letter there is defaulted (I6, exercised by U4 / U4-in).
    const bdd r_s = live[s] ? analysis_regions[s] : bdd(bddfalse);
    // lambda_s = R_s | (!bdd_exist(R_s, Xdep_cube) & default_X).
    lambda_by_state[s] = r_s | (!bdd_exist(r_s, xdep_cube) & default_x);
  }

  result.transducer = OutputLabeledTransducer(dfa, std::move(lambda_by_state),
                                              sigma0_cube, sigma1_cube);
  return result;
}

}  // namespace ltlf_ek::detail
