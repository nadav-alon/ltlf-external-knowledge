#pragma once

#include <spot/tl/formula.hh>
#include <spot/twa/bdddict.hh>
#include <spot/twa/twagraph.hh>

namespace ltlf_ek {

// LtlfToDfa(phi) --- main.tex Method 2 (alg:dfa_product, line
// alg:dfa_product), black-boxed there as \algname{LtlfToDfa}.
//
// Build the deterministic finite automaton A for the LTLf Goal formula phi,
// returned as a spot::twa_graph on `dict` (the automaton *object* of the
// glossary entry "NFA / DFA for the Goal").  Thin wrapper over Spot:
// spot::ltlf_to_mtdfa followed by mtdfa::as_twa(state_based=true), so finiteness
// is carried in *acceptance marks, not an extra AP* --- the alphabet stays
// exactly 2^{I∪O} and the product / cons construction over it stays clean.  The
// result is state-based Büchi (the abused-DBA convention of as_twa): a state is
// accepting iff it is a *final* DFA state (F_D).  It is completed
// (spot::complete_here) so delta_D is total, as alg:dfa_product's product loop
// assumes.
//
// Precondition: `dict` MUST be the same spot::bdd_dict as T_in, T_out so the
// DFA, the transducers and every letter share one variable numbering.
spot::twa_graph_ptr ltlf_to_dfa(const spot::formula& phi,
                                const spot::bdd_dict_ptr& dict);

}  // namespace ltlf_ek
