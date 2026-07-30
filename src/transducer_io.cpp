#include "ltlf_ek/transducer_io.hpp"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <spot/parseaut/public.hh>
#include <spot/tl/formula.hh>
#include <spot/tl/parse.hh>
#include <spot/twa/formula2bdd.hh>
#include <spot/twaalgos/hoa.hh>
#include <spot/twaalgos/isdet.hh>

#include "ltlf_ek/detail/util.hpp"

namespace ltlf_ek {
namespace {

std::string slurp(std::istream& in) {
  std::ostringstream ss;
  ss << in.rdbuf();
  return ss.str();
}

// Replace each C-style /* ... */ comment with a single space (the file format
// example annotates both delta edges and %%LAMBDA entries with them) so token
// boundaries survive the strip.
std::string strip_block_comments(const std::string& s) {
  std::string out;
  out.reserve(s.size());
  for (std::size_t i = 0; i < s.size();) {
    if (s[i] == '/' && i + 1 < s.size() && s[i + 1] == '*') {
      const std::size_t close = s.find("*/", i + 2);
      if (close == std::string::npos)
        throw std::invalid_argument(
            "parse_transducer: unterminated /* comment in %%LAMBDA block");
      out.push_back(' ');
      i = close + 2;
    } else {
      out.push_back(s[i]);
      ++i;
    }
  }
  return out;
}

// Split the file at HOA's terminator: the first line whose content is exactly
// `--END--`.  Everything up to and including that line is the delta automaton
// (Spot parses it, then stops); the remainder is our %%LAMBDA block.  Matching a
// whole line (rather than a raw substring) keeps a `--END--` occurring inside an
// HOA comment or a quoted AP name from splitting the file early.
struct HoaSplit {
  std::string hoa;
  std::string lambda;
};

HoaSplit split_at_hoa_end(const std::string& text) {
  std::size_t pos = 0;
  for (;;) {
    const std::size_t eol = text.find('\n', pos);
    const std::size_t line_end = (eol == std::string::npos) ? text.size() : eol;
    if (detail::trim(text.substr(pos, line_end - pos)) == "--END--")
      return {text.substr(0, line_end),
              eol == std::string::npos ? "" : text.substr(eol + 1)};
    if (eol == std::string::npos)
      throw std::invalid_argument(
          "parse_transducer: missing HOA --END-- terminator");
    pos = eol + 1;
  }
}

}  // namespace

OutputLabeledTransducer parse_transducer(std::istream& in,
                                         const VariablePartition& partition,
                                         Role role, spot::bdd_dict_ptr dict) {
  const std::string text = slurp(in);

  const HoaSplit split = split_at_hoa_end(text);
  const std::string& hoa_text = split.hoa;
  const std::string& lambda_text = split.lambda;

  // --- delta: parse the HOA automaton on the shared dict (acceptance ignored) ---
  spot::automaton_stream_parser hoa_parser(hoa_text.c_str(), "<transducer>");
  spot::parsed_aut_ptr pa = hoa_parser.parse(dict);
  if (!pa || pa->aborted || !pa->errors.empty() || !pa->aut) {
    std::ostringstream msg;
    msg << "parse_transducer: malformed HOA delta";
    if (pa) {
      msg << ":\n";
      pa->format_errors(msg);
    }
    throw std::invalid_argument(msg.str());
  }
  const spot::twa_graph_ptr aut = pa->aut;

  // Validation 1: delta deterministic --- reject overlapping guards up front
  // rather than letting delta() throw mid-synthesis (incomplete delta is fine).
  if (!spot::is_deterministic(aut))
    throw std::invalid_argument(
        "parse_transducer: delta is non-deterministic (overlapping guards)");

  // The partition is the closed universe of APs: every AP the HOA declares must
  // lie in I∪O, so delta reads only variables in the alphabet Sigma=2^{I∪O}.  A
  // guard over a stray AP would otherwise escape every other check and make
  // delta ill-defined on letters that leave that AP free.
  const std::set<std::string> universe = partition.universe();
  for (const spot::formula& ap : aut->ap())
    if (!universe.count(ap.ap_name()))
      throw std::invalid_argument(
          "parse_transducer: HOA declares AP '" + ap.ap_name() +
          "' outside I∪O (the partition is the closed universe of APs)");

  // --- Sigma0/Sigma1 derived from (partition, role); they orient lambda ---
  const SigmaSlices slices = sigma_slices(partition, role);
  const bdd sigma0_cube = detail::cube_of(slices.sigma0, aut);
  const bdd sigma1_cube = detail::cube_of(slices.sigma1, aut);
  std::set<std::string> scope = slices.sigma0;
  scope.insert(slices.sigma1.begin(), slices.sigma1.end());

  // --- lambda: parse the %%LAMBDA block, one formula per HOA state ---
  const unsigned n_states = aut->num_states();
  std::vector<bdd> lambda_by_state(n_states, bddfalse);
  std::vector<bool> seen(n_states, false);

  std::istringstream ls(strip_block_comments(lambda_text));
  std::string line;
  bool saw_header = false;
  while (std::getline(ls, line)) {
    const std::string t = detail::trim(line);
    if (t.empty()) continue;
    if (!saw_header) {
      if (t != "%%LAMBDA")
        throw std::invalid_argument(
            "parse_transducer: expected %%LAMBDA sentinel, got: " + t);
      saw_header = true;
      continue;
    }

    // "state <n>: <formula>"
    std::istringstream ts(t);
    std::string kw;
    unsigned q = 0;
    char colon = 0;
    if (!(ts >> kw) || kw != "state" || !(ts >> q) || !(ts >> colon) ||
        colon != ':')
      throw std::invalid_argument(
          "parse_transducer: malformed %%LAMBDA entry: " + t);
    std::string formula_text;
    std::getline(ts, formula_text);
    formula_text = detail::trim(formula_text);
    if (formula_text.empty())
      throw std::invalid_argument(
          "parse_transducer: empty lambda formula for state " +
          std::to_string(q));
    if (q >= n_states)
      throw std::invalid_argument(
          "parse_transducer: %%LAMBDA state index " + std::to_string(q) +
          " out of range (num_states=" + std::to_string(n_states) + ")");
    if (seen[q])
      throw std::invalid_argument(
          "parse_transducer: duplicate %%LAMBDA entry for state " +
          std::to_string(q));

    spot::formula f;
    try {
      f = spot::parse_formula(formula_text);
    } catch (const std::runtime_error& e) {
      throw std::invalid_argument(
          "parse_transducer: unparsable lambda formula for state " +
          std::to_string(q) + ": " + e.what());
    }
    // lambda is a boolean output relation (main.tex §110); a temporal operator
    // is meaningless here and cannot become a BDD.
    if (!f.is_boolean())
      throw std::invalid_argument(
          "parse_transducer: non-boolean lambda formula for state " +
          std::to_string(q) + ": " + formula_text);
    // Validation 3: AP scope --- every AP must lie in Sigma0 ∪ Sigma1.
    for (const auto& ap : collect_aps(f))
      if (!scope.count(ap))
        throw std::invalid_argument(
            "parse_transducer: lambda formula for state " + std::to_string(q) +
            " names AP '" + ap + "' outside Sigma0 ∪ Sigma1");

    const bdd out = spot::formula_to_bdd(f, dict, aut.get());

    // Validation 2: lambda functional --- fixing any Sigma0 observation leaves
    // at most one Sigma1 completion --- "at most", not "exactly": lambda is not
    // assumed total (main.tex §110 for the signature, §114-115 for
    // non-totality).  Delegates to the shared determinacy witness
    // (docs/GLOSSARY.md); see \cref{lem:outdep-diagonal}'s reuse of the same
    // predicate on a letter region.
    if (const std::optional<std::string> bad =
            undetermined_variable(out, slices.sigma1, sigma1_cube, aut))
      throw std::invalid_argument(
          "parse_transducer: non-functional lambda at state " +
          std::to_string(q) + " (an observation leaves output '" + *bad +
          "' undetermined)");

    lambda_by_state[q] = out;
    seen[q] = true;
  }

  if (!saw_header)
    throw std::invalid_argument(
        "parse_transducer: missing %%LAMBDA block after --END--");

  // Validation 4: exactly one entry per state (a missing entry is a parse
  // error, not an implicit undefined lambda).
  for (unsigned q = 0; q < n_states; ++q)
    if (!seen[q])
      throw std::invalid_argument(
          "parse_transducer: missing %%LAMBDA entry for state " +
          std::to_string(q));

  return OutputLabeledTransducer(aut, std::move(lambda_by_state), sigma0_cube,
                                 sigma1_cube);
}

std::optional<std::string> undetermined_variable(
    bdd relation, const std::set<std::string>& produced, bdd produced_cube,
    const spot::twa_graph_ptr& aut) {
  // Two preconditions, both of which fail SILENTLY and one of which fails in
  // the unsound direction, so pin them in debug builds rather than trusting the
  // caller.  (Release builds pay nothing: NDEBUG drops the cube_of call too.)
  //
  // 1. Every `produced` name is already an AP of `aut`.  register_ap below
  //    APPENDS an unknown one, silently mutating the caller's automaton --- and
  //    for the dependency caller that automaton is the Goal DFA which is itself
  //    emitted as delta_out, so a candidate Xdep variable absent from phi would
  //    grow an AP the automaton never constrains.
  // 2. `produced_cube` is the cube of exactly `produced`.  A mismatch does not
  //    merely weaken the test, it flips the verdict, in either direction: a
  //    variable missing from the cube stays in both cofactors, so with1 & with0
  //    is unconditionally empty and an unconstrained variable reads as
  //    functional (the unsound direction --- a wrong "dependent" verdict with
  //    no diagnostic); an extra observed variable in the cube reports a
  //    determined variable as undetermined.
#ifndef NDEBUG
  for (const auto& x : produced)
    assert(std::find(aut->ap().begin(), aut->ap().end(), spot::formula::ap(x)) !=
               aut->ap().end() &&
           "undetermined_variable: every `produced` name must already be an AP "
           "of `aut` (the query must not mutate its input automaton)");
  assert(produced_cube == detail::cube_of(produced, aut) &&
         "undetermined_variable: produced_cube must be the cube of exactly "
         "`produced`");
#endif

  // Per-variable cofactor form (docs/GLOSSARY.md "Determinacy witness"): no
  // observation may admit a `produced` variable both true and false.  Correct
  // for sets, not just singletons --- two distinct produced tuples over one
  // observation differ in some coordinate, and that coordinate then admits
  // both polarities.
  for (const auto& x : produced) {
    const int xv = aut->register_ap(x);
    const bdd with1 = bdd_exist(relation & bdd_ithvar(xv), produced_cube);
    const bdd with0 = bdd_exist(relation & bdd_nithvar(xv), produced_cube);
    if ((with1 & with0) != bddfalse) return x;
  }
  return std::nullopt;
}

void print_transducer(std::ostream& out, const OutputLabeledTransducer& t) {
  // delta: the HOA half of the file format.  A transducer has no F (main.tex
  // §108; see OutputLabeledTransducer::delta_dfa()), but print_hoa copies
  // whatever acceptance the delta twa happens to carry --- for a Tout built on
  // the Goal DFA that is F_D, i.e. an omega-acceptance advertising finality for
  // what is really finite-word reachability.  Emit the canonical `Acceptance: 0
  // t` of docs/prd/transducer-file-format.md instead, so the artifact cannot be
  // read (by a human or by autfilt) as claiming a finality it does not have.
  // Nothing is lost: parse_transducer ignores acceptance on the way back in.
  const spot::twa_graph_ptr delta =
      spot::make_twa_graph(t.delta_dfa(), spot::twa::prop_set::all());
  delta->set_acceptance(0, spot::acc_cond::acc_code::t());
  for (auto& e : delta->edges()) e.acc = {};

  // print_hoa does not emit a trailing newline after `--END--`, so the newline
  // that starts the %%LAMBDA block below also terminates delta's --END-- line.
  // split_at_hoa_end (parse_transducer, above) matches that line exactly; it
  // also skips blank lines, so a Spot that did emit its own newline would still
  // parse.
  spot::print_hoa(out, delta);
  out << "\n%%LAMBDA\n";

  // lambda: one boolean formula per state, the inverse of the %%LAMBDA parse
  // loop above.  emits_region(q) is exactly lambda_by_state_[q] (docs/GLOSSARY.md
  // "Output agreement (emits)"), bddfalse when lambda is undefined at q; that
  // round-trips through bdd_to_formula/parse_formula/formula_to_bdd regardless
  // of which literal token Spot prints for false.
  const spot::bdd_dict_ptr dict = t.dict();
  const unsigned n_states = delta->num_states();
  for (unsigned q = 0; q < n_states; ++q)
    out << "state " << q << ": "
        << spot::bdd_to_formula(t.emits_region(q), dict) << "\n";
}

}  // namespace ltlf_ek
