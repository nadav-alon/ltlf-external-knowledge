#pragma once

#include <string>
#include <vector>

#include <spot/twa/bdddict.hh>
#include <spot/twa/twagraph.hh>

// P1 of docs/prd/ltlf-to-nfa.md ("Novel mechanisms (b)"): the MONA subprocess
// driver and DFA-output parser behind PastLtlfToDfa (glossary: "Reverse-
// language DFA (PastLtlfToDfa)").  Internal `detail` helpers, not the public
// ltlf_to_nfa contract --- no glossary entry (the PRD's "Internal phase
// boundaries" are explicitly tentative / no glossary commitment).  Exposed
// via this header, rather than kept file-local in the .cpp, only so P1's
// structural tests can drive the MONA round-trip directly against a
// checked-in .mona fixture ahead of the Phase 2 mirror encoder.
namespace ltlf_ek::detail {

// Runs `mona -q -w -n` on `m2l_str_source` (written to a temp file) and
// returns its stdout DFA table verbatim.
//
// Format choice (PRD "Novel mechanisms (b)"): `-w` (textual DFA table) over
// `-gw` (GraphViz).  `-w` emits exactly one line per (state, guard, dst)
// triple in a fixed grammar ("State <n>: <0/1/X bits> -> state <m>"), so it
// is unambiguous to parse; `-gw` instead merges several transitions into one
// multi-line GraphViz edge label (e.g. "0 1\nX,1") to keep the picture
// compact -- readable, but not a stable machine format.  `-q` suppresses the
// progress bar, `-n` skips the (irrelevant here) counter-/satisfying-example
// ANALYSIS section MONA prints after the DFA table.
//
// Throws std::runtime_error on a nonzero mona exit (a bad/unparseable
// M2L-Str source) -- never returns malformed/partial output.
std::string run_mona(const std::string& m2l_str_source);

// Parses `mona_stdout` (the `-q -w -n` table produced by run_mona) into a
// deterministic, complete spot::twa_graph_ptr D over `dict`.
//
// `var_order` gives the free-variable-bit-position -> AP-name mapping, in
// the same order the M2L-Str source declared its free `var2` variables (each
// name is registered on `dict` via twa_graph::register_ap).  It is
// cross-checked against the free-variable names MONA echoes on the "DFA for
// formula with free variables: ..." header line.
//
// MONA's "Accepting states" become D's final states: state-based acceptance
// (spot::twa_graph::prop_state_acc), every out-edge of an accepting state
// carrying the single mark {0} -- finiteness lives in *acceptance marks, not
// an extra AP*, the same convention as ltlf_to_dfa.  D is deterministic and
// complete because MONA's minimal DFA table already partitions the full
// letter space per state (0/1/X bits are transcribed faithfully: '0'/'1'
// become a polarity literal on that AP, 'X' contributes no literal).
//
// Throws std::runtime_error if `mona_stdout` doesn't match the expected
// MONA `-w` grammar (missing header lines, a free-variable-name / guard-
// length mismatch against var_order, an out-of-range state reference, etc).
spot::twa_graph_ptr mona_output_to_dfa(const std::string& mona_stdout,
                                       const std::vector<std::string>& var_order,
                                       const spot::bdd_dict_ptr& dict);

}  // namespace ltlf_ek::detail
