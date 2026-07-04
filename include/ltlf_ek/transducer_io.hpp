#pragma once

#include <istream>
#include <set>
#include <string>

#include <spot/twa/bdddict.hh>

#include "ltlf_ek/output_labeled_transducer.hpp"
#include "ltlf_ek/variables.hpp"

namespace ltlf_ek {

// Which external knowledge strategy a transducer file materialises --- it
// selects the align-block columns that give the observed/produced slices
// (main.tex §124-133).  See docs/GLOSSARY.md ("role").
//   t_in  --- Sigma0 = Ifree,        Sigma1 = Iknown.
//   t_out --- Sigma0 = I ∪ Ofree,    Sigma1 = Oknown.
enum class Role { t_in, t_out };

// Observed (Sigma0) and produced (Sigma1) variable names for (partition, role),
// per the align block (main.tex §124-133, docs/GLOSSARY.md "Role", "Observed /
// produced slice").  Exposed so callers other than parse_transducer (e.g. the
// CLI's trivial-transducer factory, docs/prd/cli-wrapper.md) can derive the
// same slices without duplicating the align-block logic.
struct SigmaSlices {
  std::set<std::string> sigma0;
  std::set<std::string> sigma1;
};
SigmaSlices sigma_slices(const VariablePartition& partition, Role role);

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
// has no F (main.tex §101); only states, initial state, and edge guards are
// used.  delta and lambda may be partial: a missing HOA edge is an undefined
// delta, a `state q: false` entry an undefined lambda (main.tex §107,
// \cref{def:enabled}).
//
// Throws std::invalid_argument (with context) on any malformed input:
// non-deterministic delta, a non-functional lambda, an AP outside Sigma0 ∪
// Sigma1 in a lambda formula, a missing/duplicate/out-of-range state entry, or
// a missing --END-- / %%LAMBDA sentinel.
OutputLabeledTransducer parse_transducer(std::istream& in,
                                         const VariablePartition& partition,
                                         Role role, spot::bdd_dict_ptr dict);

}  // namespace ltlf_ek
