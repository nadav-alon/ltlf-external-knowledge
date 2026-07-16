#pragma once

#include <spot/tl/formula.hh>
#include <spot/twa/bdddict.hh>
#include <spot/twa/twagraph.hh>

namespace ltlf_ek {

// LtlfToNfa(phi) --- main.tex Method 1 (alg:ltlftonfa), black-boxed there as
// \algname{LtlfToNfa}.
//
// Build the NONDETERMINISTIC finite automaton N for the LTLf Goal formula
// phi, returned as a spot::twa_graph on `dict` (the automaton *object* of the
// glossary entry "NFA / DFA for the Goal"). Pipeline:
// detail::past_ltlf_to_dfa(phi, dict) [mirror(phi) folded in, MONA] --> D
// with L(D)=rev(L(phi)) --> detail::reverse_dfa_to_nfa(D) --> N with
// L(N)=L(phi). Finiteness is carried in *acceptance marks, not an extra AP*
// (as ltlf_to_dfa): the sole accepting state is the reversal's F_N={s_{D,0}}.
// N is nondeterministic and NOT completed (alg:nfa_product tolerates an
// empty delta_N(s,v)).
//
// Precondition: `dict` MUST be the same spot::bdd_dict as T_in, T_out so N,
// the transducers and every letter share one variable numbering.
spot::twa_graph_ptr ltlf_to_nfa(const spot::formula& phi,
                                const spot::bdd_dict_ptr& dict);

}  // namespace ltlf_ek
