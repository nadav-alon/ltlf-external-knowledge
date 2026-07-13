#pragma once

#include <string>
#include <vector>

#include <spot/tl/formula.hh>
#include <spot/twa/bdddict.hh>
#include <spot/twa/twagraph.hh>

// P2 of docs/prd/ltlf-to-nfa.md ("Novel mechanisms (a)"): the folded
// mirror -> M2L-Str encoder and the PastLtlfToDfa black box it feeds
// (glossary: "Formula mirror", "Reverse-language DFA (PastLtlfToDfa)").
// Internal `detail` helpers, not the public ltlf_to_nfa contract (Phase 3) --
// exposed via this header, rather than kept file-local, only so P2's
// language-equivalence checkpoint tests can drive the encoder and
// past_ltlf_to_dfa directly.
namespace ltlf_ek::detail {

// The M2L-Str source for phi's *mirror* (def:mirror), folded directly from a
// future-operator walk over `phi` -- never materialising a past-LTLf formula
// (Spot's spot::op has no Y/S/O/H).  `var_order` is the free `var2`
// declaration order used in `source`'s "var2 ...;" line (phi's AP support on
// the caller's dict, sorted for determinism); pass the SAME var_order to
// mona_output_to_dfa so guard bit positions map onto the right APs.
struct MirrorEncoding {
  std::string source;
  std::vector<std::string> var_order;
};

// Encodes phi's mirror as M2L-Str source (PRD "Novel mechanisms (a)"): a
// recursive spot::formula walk emitting, for each *future* operator, the
// FO/M2L-Str clause of its *past* dual (Y/O/H/S/Trigger) evaluated against
// *decreasing* positions -- literally the same source a genuine
// mirror-formula-then-forward-translate pipeline would produce, since M2L-Str
// is first-order-over-positions and the two direction conventions coincide.
// The wrapping formula existentially quantifies the string's *last* position
// and evaluates the walk there (def:mirror: "rev(w), |w|-1 |= mirror(phi)"),
// which has no witness for the empty string -- the non-empty-trace exclusion
// falls out of the encoding rather than being special-cased.
//
// Per-operator clauses (parameter `pos`, decreasing = later in the mirror's
// *past* reading = earlier in phi's original future reading):
//   ap p       : "pos in p"
//   X  f (weak): pos=0 (no predecessor -- vacuously true) or the
//                predecessor satisfies f's clause.
//   strong_X f : pos!=0 and the predecessor satisfies f's clause.
//   F f        : Once -- exists j<=pos satisfying f's clause.
//   G f        : Historically -- every j<=pos satisfies f's clause.
//   f1 U f2    : Since -- exists j<=pos satisfying f2, f1 holding on (j,pos].
//   f1 R f2    : Trigger, R's own De Morgan dual of U's clause.
//   f1 W f2    : (f1 U f2) or (G f1) (weak until = until-or-globally).
//   f1 M f2    : exists j<=pos satisfying f1, f2 holding on [j,pos]
//                (strong release = dual of W, mirrors "f2 U (f1 & f2)").
// Throws std::runtime_error on an LTLf operator this table does not cover
// (e.g. Star/Concat -- PSL-only, never produced by an LTLf formula).
MirrorEncoding encode_mirror(const spot::formula& phi);

// PastLtlfToDfa(Mirror(phi)) folded into one black box (alg:ltlftonfa lines
// alg:ltlftonfa:mirror + alg:ltlftonfa:mona): encode_mirror -> run_mona ->
// mona_output_to_dfa, then correct for a MONA M2L-Str compilation artifact
// (verified empirically, not documented by MONA): every M2L-Str automaton has
// exactly one extra, formula-independent leading state whose single
// unconditional (guard=true) out-edge consumes one letter before the real
// per-position processing begins (mona's "Initial state" reads one dummy
// letter that has no counterpart in the represented string).  Left alone,
// this shifts D's accepted length by one; past_ltlf_to_dfa strips it by
// re-pointing D's initial state at that single successor and purging the
// now-unreachable original initial state.
//
// Result: D, deterministic + complete, over `dict`'s registration of phi's AP
// support, with L(D) = { rev(w) : w,0 |= phi } (main.tex "PastLtlfToDfa
// (S154-159)").  Throws std::runtime_error if `mona` fails/is unparseable, or
// if the leading-state artifact does not have the assumed shape (a single
// guard=true out-edge) -- a signal the MONA compilation assumption above no
// longer holds, not something to silently paper over.
spot::twa_graph_ptr past_ltlf_to_dfa(const spot::formula& phi,
                                     const spot::bdd_dict_ptr& dict);

}  // namespace ltlf_ek::detail
