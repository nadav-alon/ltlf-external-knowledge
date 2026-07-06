#include "ltlf_ek/transducer_io.hpp"

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
#include <spot/twaalgos/isdet.hh>

namespace ltlf_ek {
namespace {

std::string slurp(std::istream& in) {
  std::ostringstream ss;
  ss << in.rdbuf();
  return ss.str();
}

std::string trim(const std::string& s) {
  const std::size_t a = s.find_first_not_of(" \t\r\n");
  if (a == std::string::npos) return "";
  const std::size_t b = s.find_last_not_of(" \t\r\n");
  return s.substr(a, b - a + 1);
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
    if (trim(text.substr(pos, line_end - pos)) == "--END--")
      return {text.substr(0, line_end),
              eol == std::string::npos ? "" : text.substr(eol + 1)};
    if (eol == std::string::npos)
      throw std::invalid_argument(
          "parse_transducer: missing HOA --END-- terminator");
    pos = eol + 1;
  }
}

// Positive-literal variable cube of `names`, registering each on the shared dict
// via the automaton (idempotent; a name already declared in the HOA AP header
// keeps its variable, an unused Sigma1 name is registered here so it still binds).
bdd cube_of(const std::set<std::string>& names,
            const spot::twa_graph_ptr& aut) {
  bdd cube = bddtrue;
  for (const auto& n : names) cube &= bdd_ithvar(aut->register_ap(n));
  return cube;
}

}  // namespace

// Sigma1 --- the *known* vars --- is mode-invariant; Sigma0 is the Mealy
// observed slice.  A future Moore mode would shrink Sigma0 here without
// touching the file format (see the PRD's mode note).
SigmaSlices sigma_slices(const VariablePartition& p, Role role) {
  SigmaSlices s;
  switch (role) {
    case Role::t_in:  // Sigma0 = Ifree, Sigma1 = Iknown.
      s.sigma0 = p.input_free;
      s.sigma1 = p.input_known;
      break;
    case Role::t_out:  // Sigma0 = I ∪ Ofree, Sigma1 = Oknown.
      s.sigma0 = p.inputs();
      s.sigma0.insert(p.output_free.begin(), p.output_free.end());
      s.sigma1 = p.output_known;
      break;
    case Role::t_c:  // Sigma0 = I, Sigma1 = Ofree (main.tex:125).
      s.sigma0 = p.inputs();
      s.sigma1 = p.output_free;
      break;
  }
  return s;
}

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
  std::set<std::string> universe = partition.inputs();
  const std::set<std::string> outs = partition.outputs();
  universe.insert(outs.begin(), outs.end());
  for (const spot::formula& ap : aut->ap())
    if (!universe.count(ap.ap_name()))
      throw std::invalid_argument(
          "parse_transducer: HOA declares AP '" + ap.ap_name() +
          "' outside I∪O (the partition is the closed universe of APs)");

  // --- Sigma0/Sigma1 derived from (partition, role); they orient lambda ---
  const SigmaSlices slices = sigma_slices(partition, role);
  const bdd sigma0_cube = cube_of(slices.sigma0, aut);
  const bdd sigma1_cube = cube_of(slices.sigma1, aut);
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
    const std::string t = trim(line);
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
    formula_text = trim(formula_text);
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
    // lambda is a boolean output relation (main.tex §103); a temporal operator
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
    // at most one Sigma1 completion (main.tex §103).  Per-variable form: no
    // observation may admit a Sigma1 variable both true and false.
    for (const auto& x : slices.sigma1) {
      const int xv = aut->register_ap(x);
      const bdd with1 = bdd_exist(out & bdd_ithvar(xv), sigma1_cube);
      const bdd with0 = bdd_exist(out & bdd_nithvar(xv), sigma1_cube);
      if ((with1 & with0) != bddfalse)
        throw std::invalid_argument(
            "parse_transducer: non-functional lambda at state " +
            std::to_string(q) + " (an observation leaves output '" + x +
            "' undetermined)");
    }

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

}  // namespace ltlf_ek
