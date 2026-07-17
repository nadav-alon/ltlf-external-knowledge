#include <optional>

#include <gtest/gtest.h>
#include <spot/tl/parse.hh>
#include <spot/twa/bdddict.hh>

#include "ltlf_ek/nfa_product.hpp"
#include "ltlf_ek/output_labeled_transducer.hpp"
#include "ltlf_ek/role.hpp"
#include "ltlf_ek/variables.hpp"
#include "ltlf_ek/verify_controller.hpp"

#include "support/fixtures.hpp"

// End-to-end edge-case fixtures for NfaProduct (docs/prd/nfa-product.md
// Phase 2, "Edge cases"): phi=1 trivially realizable, phi=0 unrealizable
// without crashing, and a free-input goal propagating nullopt (also
// exercising the non-cons-vs-cons-dead distinction, Behaviour §1 -- the
// load-bearing case the PRD flags as regression-prone).  The internal pieces
// (nfa_to_dfa, build_product_nondet) already have their own unit fixtures
// (tests/nfa_to_dfa_test.cpp, tests/product_test.cpp); the corpus-scale
// cross-method / metamorphic / differential oracles and the benchmark-report
// shape check live in tests/ltlfsynt_oracle_test.cpp and
// tests/nfa_bench_test.cpp respectively.
//
// NfaProduct::synthesize always shells out to mona (via ltlf_to_nfa ->
// detail::past_ltlf_to_dfa), so every TEST here is MONA_FOUND-gated
// (docs/prd/nfa-product.md "Edge cases" "MONA absent").
namespace {

using ltlf_ek::Controller;
using ltlf_ek::NfaProduct;
using ltlf_ek::OutputLabeledTransducer;
using ltlf_ek::Role;
using ltlf_ek::trivial_transducer;
using ltlf_ek::VariablePartition;
using ltlf_ek::verify_controller;

using ltlf_ek::test_support::IoFreeVars;
using ltlf_ek::test_support::Phi;

TEST(NfaProduct, TriviallyTruePhiIsRealizable) {
#ifndef MONA_FOUND
  GTEST_SKIP() << "mona not found (CMake find_program(mona)); NfaProduct "
                  "needs it via ltlf_to_nfa";
#endif
  const spot::bdd_dict_ptr dict = spot::make_bdd_dict();
  const VariablePartition vars = IoFreeVars();
  const OutputLabeledTransducer t_in =
      trivial_transducer(vars, Role::t_in, dict);
  const OutputLabeledTransducer t_out =
      trivial_transducer(vars, Role::t_out, dict);
  NfaProduct method;
  const spot::formula phi = Phi("1");
  const std::optional<Controller> result =
      method.synthesize(phi, vars, t_in, t_out);
  ASSERT_TRUE(result.has_value())
      << "phi=1 accepts every non-empty trace -- trivially realizable "
        "(docs/prd/nfa-product.md \"Edge cases\")";
  EXPECT_TRUE(verify_controller(phi, vars, t_in, t_out, *result).ok)
      << "the returned controller must itself pass the universal "
        "post-condition verifier";
}

TEST(NfaProduct, TriviallyFalsePhiIsUnrealizableNoCrash) {
#ifndef MONA_FOUND
  GTEST_SKIP() << "mona not found (CMake find_program(mona)); NfaProduct "
                  "needs it via ltlf_to_nfa";
#endif
  const spot::bdd_dict_ptr dict = spot::make_bdd_dict();
  const VariablePartition vars = IoFreeVars();
  const OutputLabeledTransducer t_in =
      trivial_transducer(vars, Role::t_in, dict);
  const OutputLabeledTransducer t_out =
      trivial_transducer(vars, Role::t_out, dict);
  NfaProduct method;
  // L(0) = empty: nfa_to_dfa(ltlf_to_nfa(0)) must yield a single
  // non-accepting initial DFA state with no edges (PRD "Edge cases" "phi=0")
  // -- must not crash / not return a bogus controller.
  EXPECT_FALSE(method.synthesize(Phi("0"), vars, t_in, t_out).has_value())
      << "phi=0 has the empty language -- unrealizable, and must not crash";
}

TEST(NfaProduct, UnrealizableWhenGoalDependsOnFreeInputPropagatesNullopt) {
#ifndef MONA_FOUND
  GTEST_SKIP() << "mona not found (CMake find_program(mona)); NfaProduct "
                  "needs it via ltlf_to_nfa";
#endif
  const spot::bdd_dict_ptr dict = spot::make_bdd_dict();
  const VariablePartition vars = IoFreeVars();
  const OutputLabeledTransducer t_in =
      trivial_transducer(vars, Role::t_in, dict);
  const OutputLabeledTransducer t_out =
      trivial_transducer(vars, Role::t_out, dict);
  NfaProduct method;
  // phi=i, i free: the environment can always set i false at step 1 --
  // unrealizable (mirrors tests/dfa_product_test.cpp
  // UnrealizableWhenGoalDependsOnFreeInput); NfaProduct::synthesize must
  // propagate solve_dfa's nullopt (PRD "Edge cases" "Unrealizable"), not
  // throw or return a spurious controller. This also exercises the
  // non-cons-vs-cons-dead distinction (Behaviour §1): the i=false letter IS
  // cons (t_in/t_out are trivial, so nothing blocks it) yet the
  // (incomplete) goal N dies on it -- N_c's completed sink must be reached
  // and stay non-accepting for solve_dfa to correctly report unrealizable.
  EXPECT_FALSE(method.synthesize(Phi("i"), vars, t_in, t_out).has_value());
}

}  // namespace
