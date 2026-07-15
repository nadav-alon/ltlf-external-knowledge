#include <optional>
#include <vector>

#include <gtest/gtest.h>
#include <bddx.h>
#include <spot/twa/twagraph.hh>

#include "ltlf_ek/consistency.hpp"
#include "ltlf_ek/output_labeled_transducer.hpp"

// Unit fixtures for consistent(...) (docs/GLOSSARY.md: "consistency (cons)")
// and emits(...) (docs/GLOSSARY.md: "Output agreement (emits)", the
// per-transducer lambda-agreement atom consistent now delegates to,
// docs/prd/transducer-product.md Phase 1).
//   cons := (v ∩ Iknown = lambda_in(q_in, v)) ∧ (v ∩ Oknown = lambda_out(q_out, v)).
// Both transducers MUST live on the same bdd_dict so a letter's variable
// numbers mean the same thing to each.
namespace {

using ltlf_ek::consistent;
using ltlf_ek::emits;
using ltlf_ek::OutputLabeledTransducer;

struct Fixture {
  spot::bdd_dict_ptr dict;
  int a, b, e;
  OutputLabeledTransducer t_in;    // lambda_in commits b := a   (Iknown = {b})
  OutputLabeledTransducer t_out;   // lambda_out commits e := 1  (Oknown = {e})
};

OutputLabeledTransducer OneStateSelfLoop(spot::bdd_dict_ptr dict, bdd out,
                                         bdd sigma0, bdd sigma1) {
  auto aut = spot::make_twa_graph(dict);
  aut->new_states(1);
  aut->set_init_state(0);
  aut->new_edge(0, 0, bddtrue);
  std::vector<bdd> lambda_by_state = {out};
  return OutputLabeledTransducer(aut, std::move(lambda_by_state), sigma0, sigma1);
}

Fixture MakeFixture() {
  auto dict = spot::make_bdd_dict();
  auto probe = spot::make_twa_graph(dict);
  int a = probe->register_ap("a");
  int b = probe->register_ap("b");
  int e = probe->register_ap("e");
  const bdd av = bdd_ithvar(a), bv = bdd_ithvar(b), ev = bdd_ithvar(e);

  return Fixture{
      dict, a, b, e,
      // t_in: sigma0={a}, sigma1={b}, commits b <-> a.
      OneStateSelfLoop(dict, bdd_biimp(av, bv), /*sigma0=*/av, /*sigma1=*/bv),
      // t_out: sigma0=∅ (observes nothing), sigma1={e}, commits e := true.
      OneStateSelfLoop(dict, ev, /*sigma0=*/bddtrue, /*sigma1=*/ev),
  };
}

bdd Letter(const Fixture& f, bool a, bool b, bool e) {
  return (a ? bdd_ithvar(f.a) : bdd_nithvar(f.a)) &
         (b ? bdd_ithvar(f.b) : bdd_nithvar(f.b)) &
         (e ? bdd_ithvar(f.e) : bdd_nithvar(f.e));
}

// consistent iff (b matches a, per lambda_in) AND (e is true, per lambda_out).
TEST(Consistent, TrueWhenBothKnownSlicesMatchOutputs) {
  auto f = MakeFixture();
  EXPECT_TRUE(consistent(f.t_in, 0, f.t_out, 0, Letter(f, /*a=*/true, /*b=*/true, /*e=*/true)));
  EXPECT_TRUE(consistent(f.t_in, 0, f.t_out, 0, Letter(f, /*a=*/false, /*b=*/false, /*e=*/true)));
}

TEST(Consistent, FalseWhenInputKnownSliceDisagrees) {
  auto f = MakeFixture();
  // lambda_in commits b := a, but the letter has b != a.
  EXPECT_FALSE(consistent(f.t_in, 0, f.t_out, 0, Letter(f, /*a=*/true, /*b=*/false, /*e=*/true)));
  EXPECT_FALSE(consistent(f.t_in, 0, f.t_out, 0, Letter(f, /*a=*/false, /*b=*/true, /*e=*/true)));
}

TEST(Consistent, FalseWhenOutputKnownSliceDisagrees) {
  auto f = MakeFixture();
  // lambda_out commits e := true, but the letter has e false.
  EXPECT_FALSE(consistent(f.t_in, 0, f.t_out, 0, Letter(f, /*a=*/true, /*b=*/true, /*e=*/false)));
}

// Partiality (main.tex \cref{def:consistency}): an undefined lambda makes the
// letter non-enabled, so consistent is false even on an otherwise-matching
// letter.
TEST(Consistent, FalseWhenLambdaUndefined) {
  auto f = MakeFixture();
  // A t_out whose lambda is undefined everywhere (out_[0] = bddfalse).
  auto t_out_undef = OneStateSelfLoop(f.dict, bddfalse, /*sigma0=*/bddtrue,
                                      /*sigma1=*/bdd_ithvar(f.e));
  EXPECT_FALSE(consistent(f.t_in, 0, t_out_undef, 0,
                          Letter(f, /*a=*/true, /*b=*/true, /*e=*/true)));
}

// --- emits(t, q, v): the per-transducer atom cons now delegates to ---------
// (docs/prd/transducer-product.md "Test oracles"): lambda defined & letter in
// the committed Sigma1 cube => true; lambda defined & letter outside => false;
// lambda nullopt => false. Reuses the Consistent fixture's t_in (Sigma1={b},
// commits b<->a) and t_out (Sigma1={e}, commits e:=true) directly, one
// transducer at a time instead of through the conjunction.

TEST(Emits, TrueWhenLetterAgreesWithCommittedCube) {
  auto f = MakeFixture();
  // t_in commits b<->a: b matches a here.
  EXPECT_TRUE(emits(f.t_in, 0, Letter(f, /*a=*/true, /*b=*/true, /*e=*/true)));
  EXPECT_TRUE(emits(f.t_in, 0, Letter(f, /*a=*/false, /*b=*/false, /*e=*/false)));
  // t_out commits e:=true: e is true here.
  EXPECT_TRUE(emits(f.t_out, 0, Letter(f, /*a=*/false, /*b=*/false, /*e=*/true)));
}

TEST(Emits, FalseWhenLetterFallsOutsideCommittedCube) {
  auto f = MakeFixture();
  // t_in commits b<->a: b disagrees with a here.
  EXPECT_FALSE(emits(f.t_in, 0, Letter(f, /*a=*/true, /*b=*/false, /*e=*/true)));
  EXPECT_FALSE(emits(f.t_in, 0, Letter(f, /*a=*/false, /*b=*/true, /*e=*/true)));
  // t_out commits e:=true: e is false here.
  EXPECT_FALSE(emits(f.t_out, 0, Letter(f, /*a=*/true, /*b=*/true, /*e=*/false)));
}

TEST(Emits, FalseWhenLambdaUndefined) {
  auto f = MakeFixture();
  // Same nullopt-lambda t_out as Consistent.FalseWhenLambdaUndefined.
  auto t_out_undef = OneStateSelfLoop(f.dict, bddfalse, /*sigma0=*/bddtrue,
                                      /*sigma1=*/bdd_ithvar(f.e));
  EXPECT_FALSE(emits(t_out_undef, 0,
                     Letter(f, /*a=*/true, /*b=*/true, /*e=*/true)));
  // lambda undefined regardless of the letter.
  EXPECT_FALSE(emits(t_out_undef, 0,
                     Letter(f, /*a=*/false, /*b=*/false, /*e=*/false)));
}

}  // namespace
