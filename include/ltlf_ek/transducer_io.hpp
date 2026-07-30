#pragma once

#include <istream>
#include <optional>
#include <ostream>
#include <set>
#include <string>

#include <bddx.h>
#include <spot/twa/bdddict.hh>
#include <spot/twa/twagraph.hh>

#include "ltlf_ek/output_labeled_transducer.hpp"
#include "ltlf_ek/role.hpp"
#include "ltlf_ek/variables.hpp"

namespace ltlf_ek {

// Materialise one transducer from its external file representation: a Spot HOA
// automaton for delta, then --- after HOA's `--END--` --- a `%%LAMBDA` block
// carrying lambda as one boolean formula per HOA state (docs/GLOSSARY.md:
// "transducer file format").  See docs/prd/transducer-file-format.md.
//
//   partition --- classifies every AP (Ifree/Iknown/Ofree/Oknown); with `role`
//                 it orients lambda by deriving sigma0_cube / sigma1_cube.  The
//                 file itself never restates the slices.
//   role      --- t_in / t_out, selecting the Sigma0/Sigma1 align-block columns.
//   dict      --- the SHARED spot::bdd_dict; t_in, t_out and (later) phi's
//                 automaton must all register APs in one dict, or the (v & guard)
//                 tests across them are meaningless.
//
// The HOA acceptance condition is parsed by Spot but IGNORED --- a transducer
// has no F (main.tex §108); only states, initial state, and edge guards are
// used.  delta and lambda may be partial: a missing HOA edge is an undefined
// delta, a `state q: false` entry an undefined lambda (main.tex §114-115,
// \cref{def:consistency}).
//
// Throws std::invalid_argument (with context) on any malformed input:
// non-deterministic delta, a non-functional lambda, an AP outside Sigma0 ∪
// Sigma1 in a lambda formula, a missing/duplicate/out-of-range state entry, or
// a missing --END-- / %%LAMBDA sentinel.
OutputLabeledTransducer parse_transducer(std::istream& in,
                                         const VariablePartition& partition,
                                         Role role, spot::bdd_dict_ptr dict);

// The determinacy witness (docs/GLOSSARY.md): decide whether `relation`, read
// as a relation from its non-`produced` variables to `produced`, is
// functional --- and if not, return the name of a `produced` variable that
// some observation leaves undetermined (both polarities reachable).  nullopt
// iff functional.  Per-variable cofactor form: no fresh BDD variables, no
// renamed copy of `relation`, |produced| operations.
//
// relation      --- a bdd over (observed ∪ produced) only.
// produced      --- variable NAMES of the produced slice (Sigma1, or Xdep).
// produced_cube --- the variable-cube of the same set (docs/GLOSSARY.md
//                   "Cube"), used to project the cofactors back onto
//                   `produced`.
// aut           --- supplies register_ap / the shared bdd_dict `produced`'s
//                   variables live on.
//
// Two callers, one implementation: parse_transducer's lambda-functionality
// validation below (extracted from the former inline loop at
// src/transducer_io.cpp:191-203, whose error text it preserves) and
// docs/prd/output-dependencies-tool.md's output-dependency test, which passes
// a live-letter region rather than a lambda.
std::optional<std::string> undetermined_variable(
    bdd relation, const std::set<std::string>& produced, bdd produced_cube,
    const spot::twa_graph_ptr& aut);

// Print a transducer (docs/GLOSSARY.md): the exact inverse of parse_transducer
// above --- write `t` to its file representation, a Spot HOA automaton for
// delta (acceptance ignored, via OutputLabeledTransducer::delta_dfa()), then
// --- after HOA's `--END--` --- a `%%LAMBDA` block giving lambda as one
// boolean formula per state.  As with the reader, Sigma0/Sigma1 are NOT
// written: the format does not carry them (they come from role + partition).
// Acceptance is normalised away (a transducer has no F): the emitted HOA always
// carries the canonical `Acceptance: 0 t`, never whatever the delta twa held.
//
// Round-trips --- parse_transducer(print_transducer(t)) reproduces t (same
// states, BDD-equal guards and lambda) --- under two preconditions, which hold
// for every t this project builds but are worth stating for the first caller
// that constructs one by hand rather than by parsing:
//   1. it is re-parsed under the SAME (partition, role), since those and not the
//      file supply Sigma0/Sigma1; and
//   2. every AP of t.delta_dfa() lies in partition.universe(), or the reader's
//      closed-universe check rejects the file it just wrote.
void print_transducer(std::ostream& out, const OutputLabeledTransducer& t);

}  // namespace ltlf_ek
