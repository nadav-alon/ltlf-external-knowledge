#pragma once

#include <utility>

#include <bddx.h>
#include <spot/tl/formula.hh>
#include <spot/twa/bdddict.hh>
#include <spot/twaalgos/ltlf2dfa.hh>

namespace ltlf_ek {

// Forward progression (main.tex \cref{alg:fp}) --- the project's single seam
// over spot::ltlf_translator, whose own header marks it "Semi-internal ... Do
// not rely on the interface to be stable".  Confining it here means a Spot
// bump breaks ONE file, not the product BFS (docs/prd/otf-mtdfa-product.md).
class ForwardProgression {
 public:
  // simplify_terms is pinned true (Spot's default) and deliberately NOT a
  // parameter: layer-1 of the [psi] canonicalization (docs/GLOSSARY.md
  // "Canonical representative") depends on it, and a false setting would
  // silently coarsen nothing and inflate state counts.
  explicit ForwardProgression(const spot::bdd_dict_ptr& dict);

  ForwardProgression(const ForwardProgression&) = delete;
  ForwardProgression& operator=(const ForwardProgression&) = delete;

  // FP over the WHOLE letter alphabet at once: the one-step successor MTBDD
  // of psi.  Leaves are bddfalse | bddtrue | a terminal 2*idx+b (see the
  // PRD's I5).  Memoized inside the translator, so repeated calls on the
  // same formula are free.  NOT const: it memoizes and may register APs on
  // the dict.
  bdd progress_row(const spot::formula& psi);

  // Decode one terminal leaf value into ([psi'], b).  psi' is already the
  // propeq representative (docs/GLOSSARY.md "Canonical representative").
  // Precondition: `terminal` came from bdd_get_terminal on a genuine
  // terminal leaf (bdd_is_terminal(node) true, i.e. NOT bddfalse/bddtrue) of
  // a row THIS object produced.
  //
  // TRAP: spot::ltlf_translator::leaf_to_formula(int b, int v) is NOT the
  // terminal decoder, despite the name.  Its FIRST parameter is the leaf
  // KIND, not
  // the acceptance bit: b==0 returns {ff,false}, b==1 returns {tt,true}, and
  // only b>=2 falls through to {terminal_to_formula(v), v & 1}.  So
  // leaf_to_formula(0, t) silently returns (false, false) for EVERY
  // terminal -- a plausible-looking wrong answer, not a crash.  The two
  // constant leaves (bddfalse/bddtrue) must be handled by the CALLER before
  // this is invoked; decode() itself goes straight to terminal_to_formula +
  // (t & 1).
  std::pair<spot::formula, bool> decode(int terminal) const;

 private:
  spot::ltlf_translator translator_;
};

// There is no per-letter entry point: the per-letter form of \cref{alg:fp}
// is derived as decode(bdd_get_terminal(bdd_restrict(progress_row(psi), v))),
// with the two constant leaves (bddfalse/bddtrue) handled first --- the same
// symbolic-vs-per-letter split as emits/emits_region and delta/delta_edges,
// except that here only the symbolic form is an API.

}  // namespace ltlf_ek
