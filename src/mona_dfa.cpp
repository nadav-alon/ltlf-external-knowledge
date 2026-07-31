#include "ltlf_ek/detail/mona_dfa.hpp"

#include <sys/wait.h>
#include <unistd.h>

#include <cstdio>
#include <fstream>
#include <set>
#include <sstream>
#include <stdexcept>

#include <bddx.h>

#include "ltlf_ek/detail/util.hpp"

namespace ltlf_ek::detail {

namespace {

// A self-deleting temp file: `mona` needs a real filename argument for the
// M2L-Str source, and its stdout/stderr are each captured to their own temp
// file so a nonzero exit can be reported with the actual mona error text.
class ScopedTempFile {
 public:
  explicit ScopedTempFile(const std::string& contents = "") {
    path_ = temp_template("ltlf_ek_mona");
    const int fd = mkstemp(path_.data());
    if (fd < 0)
      throw std::runtime_error("mona_dfa: mkstemp failed for " + path_);
    if (!contents.empty()) {
      const ssize_t n = write(fd, contents.data(), contents.size());
      if (n < 0 || static_cast<std::size_t>(n) != contents.size()) {
        close(fd);
        std::remove(path_.c_str());
        throw std::runtime_error("mona_dfa: failed to write temp file " +
                                 path_);
      }
    }
    close(fd);
  }
  ~ScopedTempFile() { std::remove(path_.c_str()); }
  const std::string& path() const { return path_; }

  ScopedTempFile(const ScopedTempFile&) = delete;
  ScopedTempFile& operator=(const ScopedTempFile&) = delete;

 private:
  std::string path_;
};

std::string ShellQuote(const std::string& s) {
  std::string out = "'";
  for (char c : s) {
    if (c == '\'')
      out += "'\\''";
    else
      out += c;
  }
  out += "'";
  return out;
}

std::string ReadFile(const std::string& path) {
  std::ifstream in(path);
  std::ostringstream ss;
  ss << in.rdbuf();
  return ss.str();
}

}  // namespace

std::string run_mona(const std::string& m2l_str_source) {
  ScopedTempFile source_file(m2l_str_source);
  ScopedTempFile out_capture, err_capture;

  // -w: whole-automaton textual DFA table (see mona_dfa.hpp for the -w vs
  // -gw rationale). -q: no progress bar. -n: skip the counter-/satisfying-
  // example ANALYSIS section (irrelevant here, and it can be slow).
  std::ostringstream cmd;
  cmd << "mona -q -w -n " << ShellQuote(source_file.path()) << " >"
      << ShellQuote(out_capture.path()) << " 2>"
      << ShellQuote(err_capture.path());
  const int rc = std::system(cmd.str().c_str());
  const int exit_code = WIFEXITED(rc) ? WEXITSTATUS(rc) : -1;

  if (exit_code != 0)
    throw std::runtime_error("mona_dfa: `mona` exited " +
                             std::to_string(exit_code) +
                             "; stderr: " + ReadFile(err_capture.path()));

  const std::string out = ReadFile(out_capture.path());
  if (out.empty())
    throw std::runtime_error("mona_dfa: `mona` produced no output");
  return out;
}

namespace {

// One parsed MONA transition line: source state, the 0/1/X guard bits
// (ordered per var_order), destination state.
struct RawTransition {
  unsigned src;
  std::string bits;
  unsigned dst;
};

[[noreturn]] void Fail(const std::string& why) {
  throw std::runtime_error("mona_dfa: unparseable MONA -w output: " + why);
}

std::vector<std::string> Tokenize(const std::string& s) {
  std::vector<std::string> tokens;
  std::istringstream iss(s);
  std::string tok;
  while (iss >> tok) tokens.push_back(tok);
  return tokens;
}

unsigned ParseUnsigned(const std::string& tok, const std::string& context) {
  if (tok.empty() || tok.find_first_not_of("0123456789") != std::string::npos)
    Fail("expected a non-negative integer in " + context + ", got '" + tok +
         "'");
  return static_cast<unsigned>(std::stoul(tok));
}

// Skips blank/whitespace-only lines (mona -w pads its table with a leading
// blank line and one between the header block and "Automaton has ...") and
// returns the next non-blank line, or fails at EOF.
std::string NextNonBlankLine(std::istringstream& in,
                             const std::string& while_looking_for) {
  std::string line;
  while (std::getline(in, line))
    if (!trim(line).empty()) return line;
  Fail("reached end of input while looking for " + while_looking_for);
}

// Finds `prefix` in `line` and returns the trimmed remainder after it, or
// fails.
std::string ExpectPrefix(const std::string& line, const std::string& prefix) {
  const std::size_t pos = line.find(prefix);
  if (pos == std::string::npos)
    Fail("expected a line containing '" + prefix + "', got '" + line + "'");
  return trim(line.substr(pos + prefix.size()));
}

}  // namespace

spot::twa_graph_ptr mona_output_to_dfa(const std::string& mona_stdout,
                                       const std::vector<std::string>& var_order,
                                       const spot::bdd_dict_ptr& dict) {
  std::istringstream in(mona_stdout);

  // "DFA for formula with free variables: a b"
  const std::vector<std::string> free_vars = Tokenize(ExpectPrefix(
      NextNonBlankLine(in, "the 'DFA for formula with free variables:' header"),
      "DFA for formula with free variables:"));
  if (free_vars != var_order)
    Fail("free-variable order mismatch: MONA reported [" +
         [&] {
           std::string s;
           for (const auto& v : free_vars) s += v + " ";
           return s;
         }() +
         "], var_order expected the same names in the same order -- "
         "encoder/parser var_order desync");

  // "Initial state: 0"
  const unsigned init = ParseUnsigned(
      ExpectPrefix(NextNonBlankLine(in, "'Initial state:'"), "Initial state:"),
      "Initial state");

  // "Accepting states: 1 3 5" (possibly empty after the colon).
  std::set<unsigned> accepting;
  for (const std::string& tok : Tokenize(ExpectPrefix(
           NextNonBlankLine(in, "'Accepting states:'"), "Accepting states:")))
    accepting.insert(ParseUnsigned(tok, "Accepting states"));

  // "Rejecting states: ..." / "Don't-care states: ..." are informational
  // only: F_D is exactly the Accepting-states set (PRD "Novel mechanisms
  // (b)"), so their contents are not parsed, just their presence checked
  // (a stronger signal that this really is MONA's -w table).
  ExpectPrefix(NextNonBlankLine(in, "'Rejecting states:'"), "Rejecting states:");
  ExpectPrefix(NextNonBlankLine(in, "\"Don't-care states:\""),
              "Don't-care states:");

  // "Automaton has <S> states and <B> BDD-nodes"
  const std::vector<std::string> automaton_tokens =
      Tokenize(NextNonBlankLine(in, "'Automaton has ... states' line"));
  if (automaton_tokens.size() < 4 || automaton_tokens[0] != "Automaton" ||
      automaton_tokens[1] != "has" || automaton_tokens[3] != "states")
    Fail("malformed 'Automaton has ... states' line");
  const unsigned num_states =
      ParseUnsigned(automaton_tokens[2], "Automaton has ... states");

  // "Transitions:" literal, then one "State <s>: <bits> -> state <d>" per
  // out-edge; `bits` has var_order.size() characters from {0,1,X} (may be
  // empty when var_order is empty).
  if (trim(NextNonBlankLine(in, "'Transitions:'")) != "Transitions:")
    Fail("expected the 'Transitions:' line");

  std::vector<RawTransition> transitions;
  std::string line;
  while (std::getline(in, line)) {
    if (trim(line).empty()) continue;
    if (line.rfind("State ", 0) != 0)
      Fail("expected a 'State <n>: ...' line, got '" + line + "'");
    const std::size_t colon = line.find(':');
    if (colon == std::string::npos)
      Fail("missing ':' in transition line '" + line + "'");
    const unsigned src =
        ParseUnsigned(trim(line.substr(6, colon - 6)), "transition source state");
    const std::size_t arrow = line.find("->", colon);
    if (arrow == std::string::npos)
      Fail("missing '->' in transition line '" + line + "'");
    const std::string bits = trim(line.substr(colon + 1, arrow - colon - 1));
    if (bits.size() != var_order.size())
      Fail("guard '" + bits + "' has " + std::to_string(bits.size()) +
           " bit(s), expected " + std::to_string(var_order.size()));
    for (char c : bits)
      if (c != '0' && c != '1' && c != 'X')
        Fail("guard '" + bits + "' has an unexpected character '" +
             std::string(1, c) + "'");
    const std::string dst_prefix = "state";
    const std::size_t dst_pos = line.find(dst_prefix, arrow);
    if (dst_pos == std::string::npos)
      Fail("missing destination 'state <n>' in '" + line + "'");
    const unsigned dst = ParseUnsigned(
        trim(line.substr(dst_pos + dst_prefix.size())), "transition destination state");
    if (src >= num_states || dst >= num_states)
      Fail("transition references a state >= the declared state count (" +
           std::to_string(num_states) + ")");
    transitions.push_back({src, bits, dst});
  }
  if (init >= num_states) Fail("initial state out of range");
  for (unsigned a : accepting)
    if (a >= num_states) Fail("accepting state out of range");

  // --- Build D: deterministic, complete, state-based acceptance ---------
  spot::twa_graph_ptr dfa = spot::make_twa_graph(dict);
  dfa->set_buchi();
  dfa->prop_state_acc(true);
  dfa->new_states(num_states);
  dfa->set_init_state(init);

  std::vector<int> ap_index(var_order.size());
  for (std::size_t i = 0; i < var_order.size(); ++i)
    ap_index[i] = dfa->register_ap(var_order[i]);

  const spot::acc_cond::mark_t kFinalMark = {0};
  const spot::acc_cond::mark_t kNoMark = {};
  for (const RawTransition& t : transitions) {
    bdd guard = bddtrue;
    for (std::size_t i = 0; i < t.bits.size(); ++i) {
      if (t.bits[i] == '0')
        guard &= bdd_nithvar(ap_index[i]);
      else if (t.bits[i] == '1')
        guard &= bdd_ithvar(ap_index[i]);
      // 'X': don't-care, no literal contributed.
    }
    const spot::acc_cond::mark_t mark =
        accepting.count(t.src) ? kFinalMark : kNoMark;
    dfa->new_edge(t.src, t.dst, guard, mark);
  }
  return dfa;
}

}  // namespace ltlf_ek::detail
