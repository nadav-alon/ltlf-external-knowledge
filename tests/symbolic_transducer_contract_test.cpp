#include <optional>
#include <utility>
#include <vector>

#include <gtest/gtest.h>
#include <bddx.h>
#include <spot/tl/parse.hh>
#include <spot/twa/bdddict.hh>
#include <spot/twa/twagraph.hh>
#include <spot/twaalgos/synthesis.hh>

#include "ltlf_ek/consistency.hpp"
#include "ltlf_ek/dfa_product.hpp"
#include "ltlf_ek/output_labeled_transducer.hpp"
#include "ltlf_ek/product.hpp"
#include "ltlf_ek/role.hpp"
#include "ltlf_ek/synthesis.hpp"
#include "ltlf_ek/transducer.hpp"
#include "ltlf_ek/variables.hpp"

#include "support/fixtures.hpp"

// Symbolic-transducer contract-equivalence unit tests
// (docs/prd/symbolic-dfa-product.md "Test oracles" #1, Phase 1):
// `emits_region`/`delta_edges` are the whole-region /
// symbolic-partition forms of the existing per-letter `emits`/`delta` atoms,
// so for every letter of a LetterAlphabet the two views must agree:
//
//   (v & emits_region(q)) != bddfalse  <=>  emits(t, q, v)
//   the unique delta_edges(q) guard satisfied by v names the same dst as
//     delta(q, v); an undefined delta(q, v) <=> no delta_edges guard covers v.
//
// Run over a spread of Transducer implementations/constructions
// (OutputLabeledTransducer built directly from fixtures, trivial_transducer,
// controller_as_transducer), including a partial transducer (undefined delta
// and/or undefined lambda at some state) so the undefined <=> no-guard /
// bddfalse branches are exercised.  Phase 2 (build_product_symbolic,
// to_guard_map, the build-equivalence oracle) is explicitly out of scope
// here --- those functions do not exist yet.
namespace {

using ltlf_ek::Controller;
using ltlf_ek::controller_as_transducer;
using ltlf_ek::DfaProduct;
using ltlf_ek::emits;
using ltlf_ek::LetterAlphabet;
using ltlf_ek::OutputLabeledTransducer;
using ltlf_ek::Role;
using ltlf_ek::trivial_transducer;
using ltlf_ek::Transducer;
using ltlf_ek::VariablePartition;

using ltlf_ek::test_support::IoFreeVars;
using ltlf_ek::test_support::Phi;

// --- Shared oracle: the contract itself -------------------------------------

// PRD bullet 1: emits_region's whole-set membership test agrees with the
// per-letter `emits` atom on every letter of `alphabet`, at state q.
void ExpectEmitsRegionAgreesWithEmits(const Transducer& t, unsigned q,
                                      const LetterAlphabet& alphabet) {
  const bdd region = t.emits_region(q);
  for (const bdd& v : alphabet.letters()) {
    const bool region_says = (v & region) != bddfalse;
    const bool per_letter_says = emits(t, q, v);
    EXPECT_EQ(region_says, per_letter_says) << "letter mismatch at state " << q;
  }
}

// PRD bullet 2: the unique delta_edges(q) guard satisfied by v names the same
// dst as delta(q, v); an undefined delta(q, v) <=> no delta_edges guard
// covers v.  Also checks the "unique" premise itself (a deterministic
// delta_edges partition satisfies at most one guard per letter) rather than
// silently trusting it.
void ExpectDeltaEdgesAgreesWithDelta(const Transducer& t, unsigned q,
                                     const LetterAlphabet& alphabet) {
  const std::vector<std::pair<bdd, unsigned>> edges = t.delta_edges(q);
  for (const bdd& v : alphabet.letters()) {
    std::optional<unsigned> matched;
    for (const auto& [guard, dst] : edges) {
      if ((v & guard) != bddfalse) {
        ASSERT_FALSE(matched.has_value())
            << "letter satisfies more than one delta_edges guard at state "
            << q << " (non-deterministic partition)";
        matched = dst;
      }
    }
    EXPECT_EQ(matched, t.delta(q, v)) << "state " << q;
  }
}

// Runs both halves of the contract over every state 0..n_states-1.
void ExpectSymbolicContractOnEveryState(const Transducer& t, unsigned n_states,
                                      const LetterAlphabet& alphabet) {
  for (unsigned q = 0; q < n_states; ++q) {
    SCOPED_TRACE(q);
    ExpectEmitsRegionAgreesWithEmits(t, q, alphabet);
    ExpectDeltaEdgesAgreesWithDelta(t, q, alphabet);
  }
}

// --- Fixture 1: a hand-built OutputLabeledTransducer with BOTH partial delta
// and partial lambda (the same shape as output_labeled_transducer_test.cpp's
// MakeFixture, duplicated file-locally per this repo's fixture-duplication
// norm) -----------------------------------------------------------------

struct PartialVars {
  int a, b, c;
};

// delta:  s0 --a--> s1,  s0 --!a--> s0,  s1 --a--> s2,  s2 (no edges).
//         => delta(s1, !a) and delta(s2, *) are undefined (partial).
// lambda: out_[0] = (a<->b), out_[1] = b, out_[2] = false (undefined at s2).
OutputLabeledTransducer MakePartialFixture(PartialVars* v) {
  auto aut = spot::make_twa_graph(spot::make_bdd_dict());
  v->a = aut->register_ap("a");
  v->b = aut->register_ap("b");
  v->c = aut->register_ap("c");
  const bdd av = bdd_ithvar(v->a);
  const bdd bv = bdd_ithvar(v->b);

  aut->new_states(3);
  aut->set_init_state(0);
  aut->new_edge(0, 1, av);
  aut->new_edge(0, 0, bdd_nithvar(v->a));
  aut->new_edge(1, 2, av);
  // state 2: no outgoing edges -> delta always undefined.

  std::vector<bdd> lambda_by_state = {
      bdd_biimp(av, bv),
      bv,
      bddfalse,
  };
  return OutputLabeledTransducer(aut, std::move(lambda_by_state), av, bv);
}

TEST(SymbolicTransducerContract,
     OutputLabeledTransducerFromFixtureWithPartialDeltaAndLambda) {
  PartialVars v;
  auto t = MakePartialFixture(&v);
  // Universe = {a, b, c}, matching the fixture's registered APs; the free/
  // known split is irrelevant to the contract check (LetterAlphabet
  // enumerates the full I∪O universe regardless).
  auto vars = VariablePartition::split({"a", "c"}, {"b"}, /*governed=*/{});
  auto registrar = spot::make_twa_graph(t.dict());
  const LetterAlphabet alphabet(vars, registrar);

  ExpectSymbolicContractOnEveryState(t, /*n_states=*/3, alphabet);
}

// --- Fixture 2: isolate a total-delta / partial-lambda state ---------------

TEST(SymbolicTransducerContract, TotalDeltaButPartialLambdaOneStateFixture) {
  auto dict = spot::make_bdd_dict();
  auto aut = spot::make_twa_graph(dict);
  const int a = aut->register_ap("a");
  const int b = aut->register_ap("b");
  const bdd av = bdd_ithvar(a);
  const bdd bv = bdd_ithvar(b);

  aut->new_states(1);
  aut->set_init_state(0);
  aut->new_edge(0, 0, bddtrue);  // delta total: always self-loops.
  // lambda commits b := true only on the observation a = true; a = false has
  // no completion (partial lambda at a state that IS otherwise defined).
  std::vector<bdd> lambda_by_state = {av & bv};
  OutputLabeledTransducer t(aut, std::move(lambda_by_state), /*sigma0=*/av,
                            /*sigma1=*/bv);

  auto vars = VariablePartition::split({"a"}, {"b"}, /*governed=*/{});
  auto registrar = spot::make_twa_graph(dict);
  const LetterAlphabet alphabet(vars, registrar);

  ExpectSymbolicContractOnEveryState(t, /*n_states=*/1, alphabet);
}

// --- Fixture 3: isolate a partial-delta / total-lambda state ---------------

TEST(SymbolicTransducerContract, PartialDeltaButTotalLambdaOneStateFixture) {
  auto dict = spot::make_bdd_dict();
  auto aut = spot::make_twa_graph(dict);
  const int a = aut->register_ap("a");
  const bdd av = bdd_ithvar(a);

  aut->new_states(1);
  aut->set_init_state(0);
  aut->new_edge(0, 0, av);  // delta undefined when a is false.
  std::vector<bdd> lambda_by_state = {bddtrue};  // lambda total (Sigma1 = ∅).
  OutputLabeledTransducer t(aut, std::move(lambda_by_state), /*sigma0=*/bddtrue,
                            /*sigma1=*/bddtrue);

  auto vars = VariablePartition::split({"a"}, {}, /*governed=*/{});
  auto registrar = spot::make_twa_graph(dict);
  const LetterAlphabet alphabet(vars, registrar);

  ExpectSymbolicContractOnEveryState(t, /*n_states=*/1, alphabet);
}

// --- Fixture 4: trivial_transducer (totally-defined, empty knowledge) ------

TEST(SymbolicTransducerContract, TrivialTransducerTIn) {
  auto dict = spot::make_bdd_dict();
  auto vars = IoFreeVars();
  auto t = trivial_transducer(vars, Role::t_in, dict);

  auto registrar = spot::make_twa_graph(dict);
  const LetterAlphabet alphabet(vars, registrar);

  ExpectSymbolicContractOnEveryState(t, /*n_states=*/1, alphabet);
}

TEST(SymbolicTransducerContract, TrivialTransducerTOut) {
  auto dict = spot::make_bdd_dict();
  auto vars = IoFreeVars();
  auto t = trivial_transducer(vars, Role::t_out, dict);

  auto registrar = spot::make_twa_graph(dict);
  const LetterAlphabet alphabet(vars, registrar);

  ExpectSymbolicContractOnEveryState(t, /*n_states=*/1, alphabet);
}

// --- Fixture 5: controller_as_transducer, on a real synthesized strategy ---

TEST(SymbolicTransducerContract, ControllerAsTransducerFromDfaProductSynthesis) {
  auto dict = spot::make_bdd_dict();
  auto vars = IoFreeVars();
  auto t_in = trivial_transducer(vars, Role::t_in, dict);
  auto t_out = trivial_transducer(vars, Role::t_out, dict);

  DfaProduct method;
  const std::optional<Controller> controller =
      method.synthesize(Phi("G(i -> o)"), vars, t_in, t_out);
  ASSERT_TRUE(controller.has_value());

  const OutputLabeledTransducer t_c =
      controller_as_transducer(*controller, vars);
  // Independent re-collapse of the same split strategy graph
  // controller_as_transducer built t_c from, purely to learn its state count
  // (Transducer exposes no num_states()) --- deterministic, so this is not a
  // second implementation to trust, just a state-count probe.
  const spot::twa_graph_ptr collapsed =
      spot::unsplit_2step(controller->strategy);

  auto registrar = spot::make_twa_graph(dict);
  const LetterAlphabet alphabet(vars, registrar);

  ExpectSymbolicContractOnEveryState(t_c, collapsed->num_states(), alphabet);
}

}  // namespace
