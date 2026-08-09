#include <memory>
#include <optional>
#include <ostream>
#include <string>
#include <vector>

#include <bddx.h>
#include <gtest/gtest.h>
#include <spot/tl/formula.hh>
#include <spot/tl/parse.hh>
#include <spot/twa/bdddict.hh>
#include <spot/twa/twagraph.hh>

#include "ltlf_ek/cli.hpp"
#include "ltlf_ek/output_labeled_transducer.hpp"
#include "ltlf_ek/role.hpp"
#include "ltlf_ek/synthesis.hpp"
#include "ltlf_ek/turn_order.hpp"
#include "ltlf_ek/variables.hpp"

// O1/O2 (docs/prd/acceptance-mark-on-edgeless-states.md "Test oracles"): the
// partiality matrix {delta-dead, lambda-undefined} x {Tin, Tout}, four
// fixtures.  O1 is cross-method agreement across all five `Synthesis`
// implementations; O2 additionally pins the expected verdict by hand.
//
// Model (O2): "phi = b, Ofree = {b}, delta-dead Tin state 1" is the
// pre-existing repro (tests/mtnfa_product_test.cpp SECTION E,
// MtnfaProductAcceptanceMarkParity, formerly MtnfaProductExpectedDivergence)
// and is reused verbatim as fixture 1
// (TinDeltaDead, below); the other three vary WHICH transducer (Tin/Tout)
// and WHICH partiality source (delta-dead / lambda-undefined) is the one
// that goes edgeless, but share its reasoning: a length-1 trace with b=true
// satisfies phi="b" outright (LTLf: only the FIRST letter matters); the only
// letter consistent with both transducers at the first move is one where
// the transducer under test's own constraint is already satisfied (or, for
// the Tin/Tout-side of the OTHER fixture in the pair, unconstrained); the
// goal DFA reaches its ACCEPTING state on that very letter; and the
// transducer under test then has NO further legal continuation -- a
// legitimate finite-trace win (def:consistency's partiality clause), not an
// unrealizable dead end.  alg:dfa_product:final (Behaviour item 1) says
// F_P = F_D x Q_in x Q_out with no exclusion for edgelessness, so this
// product state IS accepting despite being edgeless: all four fixtures are
// REALIZABLE under reading A.
namespace {

using ltlf_ek::make_synthesis_method;
using ltlf_ek::OutputLabeledTransducer;
using ltlf_ek::register_turn_order_aps;
using ltlf_ek::Role;
using ltlf_ek::Synthesis;
using ltlf_ek::trivial_transducer;
using ltlf_ek::VariablePartition;

spot::formula Phi(const std::string& s) { return spot::parse_formula(s); }

struct PartialityFixture {
  std::string name;
  spot::bdd_dict_ptr dict;
  VariablePartition vars;
  OutputLabeledTransducer t_in;
  OutputLabeledTransducer t_out;
};

void PrintTo(const PartialityFixture& f, std::ostream* os) { *os << f.name; }

// Fixture 1 -- TinDeltaDead (the pre-existing repro, verbatim): t_in commits
// a=true unconditionally at state 0, then state 1 has NO out-edges at all.
// t_out is fully permissive (trivial_transducer, Oknown = empty).
PartialityFixture BuildTinDeltaDead() {
  spot::bdd_dict_ptr dict = spot::make_bdd_dict();
  const VariablePartition vars =
      VariablePartition::split({"a"}, {"b"}, /*governed=*/{"a"});
  register_turn_order_aps(vars, dict);

  spot::twa_graph_ptr g = spot::make_twa_graph(dict);
  const int a = g->register_ap("a");
  g->new_states(2);
  g->set_init_state(0);
  g->new_edge(0, 1, bddtrue);
  // State 1: NO out-edges at all -- delta-dead.
  OutputLabeledTransducer t_in(g, {bdd_ithvar(a), bddfalse},
                               /*sigma0=*/bddtrue, /*sigma1=*/bdd_ithvar(a));
  OutputLabeledTransducer t_out = trivial_transducer(vars, Role::t_out, dict);
  return {"TinDeltaDead", dict, vars, std::move(t_in), std::move(t_out)};
}

// Fixture 2 -- TinLambdaUndefined: SAME delta shape as fixture 1 (state 1
// even gets a raw self-loop edge, unlike fixture 1), but lambda is undefined
// at state 1 (bddfalse) -- `consistent` (def:consistency, "an undefined
// lambda makes v non-enabled") rejects every letter there regardless of the
// raw edge, so the PRODUCT is edgeless from that state even though Tin's OWN
// automaton is not.  The two partiality sources (def:consistency partiality
// clause: missing delta OR missing lambda) are unified by the fix; this
// fixture is the lambda-only half of that unification, isolated from
// delta-deadness by construction.
PartialityFixture BuildTinLambdaUndefined() {
  spot::bdd_dict_ptr dict = spot::make_bdd_dict();
  const VariablePartition vars =
      VariablePartition::split({"a"}, {"b"}, /*governed=*/{"a"});
  register_turn_order_aps(vars, dict);

  spot::twa_graph_ptr g = spot::make_twa_graph(dict);
  const int a = g->register_ap("a");
  g->new_states(2);
  g->set_init_state(0);
  g->new_edge(0, 1, bddtrue);
  g->new_edge(1, 1, bddtrue);  // a raw delta edge DOES exist at state 1...
  // ...but lambda undefined at state 1 means `consistent` rejects every
  // letter there regardless -- the edge is real, the PRODUCT continuation
  // is not.
  OutputLabeledTransducer t_in(g, {bdd_ithvar(a), bddfalse},
                               /*sigma0=*/bddtrue, /*sigma1=*/bdd_ithvar(a));
  OutputLabeledTransducer t_out = trivial_transducer(vars, Role::t_out, dict);
  return {"TinLambdaUndefined", dict, vars, std::move(t_in), std::move(t_out)};
}

// Fixtures 3/4 share this shape: t_in is fully permissive
// (trivial_transducer, Iknown = empty -- "a" is free), and the partiality
// under test moves to t_out instead. Oknown stays empty (t_out commits
// nothing phi depends on), so t_out's own delta graph is a private state
// machine whose edgelessness is exactly what O1's "Partial Tout, not only
// partial Tin" bullet asks to cover -- the PRODUCT still needs t_out's
// continuation to exist for *any* edge to be built, regardless of whether
// t_out's output constrains phi.
VariablePartition TinTrivialVars() {
  return VariablePartition::split({"a"}, {"b"}, /*governed=*/{});
}

// Fixture 3 -- TouDeltaDead: t_out state 0 --(t)--> state 1, state 1 has no
// out-edges at all.
PartialityFixture BuildTouDeltaDead() {
  spot::bdd_dict_ptr dict = spot::make_bdd_dict();
  const VariablePartition vars = TinTrivialVars();
  register_turn_order_aps(vars, dict);

  OutputLabeledTransducer t_in = trivial_transducer(vars, Role::t_in, dict);

  spot::twa_graph_ptr g = spot::make_twa_graph(dict);
  const int a = g->register_ap("a");
  const int b = g->register_ap("b");
  g->new_states(2);
  g->set_init_state(0);
  g->new_edge(0, 1, bddtrue);
  // State 1: NO out-edges at all -- delta-dead.
  OutputLabeledTransducer t_out(g, {bddtrue, bddtrue},
                                /*sigma0=*/bdd_ithvar(a) & bdd_ithvar(b),
                                /*sigma1=*/bddtrue);  // Oknown = empty.
  return {"TouDeltaDead", dict, vars, std::move(t_in), std::move(t_out)};
}

// Fixture 4 -- TouLambdaUndefined: same delta shape as fixture 3 plus a raw
// self-loop at state 1 (like fixture 2's Tin mirror), but lambda undefined
// there.
PartialityFixture BuildTouLambdaUndefined() {
  spot::bdd_dict_ptr dict = spot::make_bdd_dict();
  const VariablePartition vars = TinTrivialVars();
  register_turn_order_aps(vars, dict);

  OutputLabeledTransducer t_in = trivial_transducer(vars, Role::t_in, dict);

  spot::twa_graph_ptr g = spot::make_twa_graph(dict);
  const int a = g->register_ap("a");
  const int b = g->register_ap("b");
  g->new_states(2);
  g->set_init_state(0);
  g->new_edge(0, 1, bddtrue);
  g->new_edge(1, 1, bddtrue);  // raw edge exists...
  OutputLabeledTransducer t_out(g, {bddtrue, bddfalse},  // ...lambda undefined at 1.
                                /*sigma0=*/bdd_ithvar(a) & bdd_ithvar(b),
                                /*sigma1=*/bddtrue);
  return {"TouLambdaUndefined", dict, vars, std::move(t_in), std::move(t_out)};
}

std::vector<PartialityFixture> BuildPartialityMatrix() {
  return {BuildTinDeltaDead(), BuildTinLambdaUndefined(), BuildTouDeltaDead(),
          BuildTouLambdaUndefined()};
}

// The five wired `Synthesis` flags (include/ltlf_ek/cli.hpp
// make_synthesis_method), i.e. all five methods the PRD's Behaviour /Test
// oracles discuss -- three believed broken before this PRD
// (DfaProduct/NfaProduct/MtdfaProduct), two believed immune
// (MtnfaProduct/OtfMtdfaProduct); O1 is exactly the check that this class no
// longer separates them.
const std::vector<std::string> kMethodFlags = {
    "dfa-product", "nfa-product", "mtdfa-product", "otf-mtdfa-product",
    "mtnfa-product"};

class PartialityMatrixTest
    : public ::testing::TestWithParam<PartialityFixture> {};

TEST_P(PartialityMatrixTest, AllFiveSynthesisMethodsAgreeAndMatchReadingA) {
  const PartialityFixture& f = GetParam();
  const spot::formula phi = Phi("b");

  std::optional<bool> first_flag;
  std::string first_name;
  for (const std::string& flag : kMethodFlags) {
#ifndef MONA_FOUND
    if (flag == "mtnfa-product") continue;  // needs mona; skip only this arm.
#endif
    std::unique_ptr<Synthesis> method = make_synthesis_method(flag);
    const bool realizable =
        method->synthesize(phi, f.vars, f.t_in, f.t_out).has_value();

    // O2: the hand-pinned expected verdict (see the file header's model).
    EXPECT_TRUE(realizable)
        << flag << " on " << f.name
        << ": expected REALIZABLE by alg:dfa_product:final (reading A) -- "
           "the edgeless product state's goal component is accepting, so "
           "F_P-membership must survive regardless of edgelessness";

    // O1: cross-method agreement -- the core oracle.
    if (!first_flag) {
      first_flag = realizable;
      first_name = flag;
    } else {
      EXPECT_EQ(realizable, *first_flag)
          << flag << " disagrees with " << first_name << " on " << f.name;
    }
  }
}

INSTANTIATE_TEST_SUITE_P(
    PartialityMatrix, PartialityMatrixTest,
    ::testing::ValuesIn(BuildPartialityMatrix()),
    [](const ::testing::TestParamInfo<PartialityFixture>& info) {
      return info.param.name;
    });

}  // namespace
