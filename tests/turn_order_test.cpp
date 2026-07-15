#include <optional>
#include <stdexcept>

#include <gtest/gtest.h>
#include <bddx.h>
#include <spot/tl/parse.hh>
#include <spot/twa/bdddict.hh>
#include <spot/twa/twagraph.hh>

#include "ltlf_ek/dfa_product.hpp"
#include "ltlf_ek/mtdfa_product.hpp"
#include "ltlf_ek/output_labeled_transducer.hpp"
#include "ltlf_ek/role.hpp"
#include "ltlf_ek/synthesis.hpp"
#include "ltlf_ek/turn_order.hpp"
#include "ltlf_ek/variables.hpp"

// AP-order tests (docs/prd/mtdfa-product.md "Test oracles" #3, Phase 0/Q2):
// "the one bug class here that is silent" --- a bad BDD variable order does
// not crash, it returns a plausible, WRONG "unrealizable".
// register_turn_order_aps / require_turn_order_aps / MtdfaProduct are all
// frozen-but-unimplemented as of this file (concurrent workflow); it will not
// compile/link until include/ltlf_ek/turn_order.hpp,
// include/ltlf_ek/mtdfa_product.hpp and their src/*.cpp land.
//
// Per the task brief: register_turn_order_aps must be called at every
// dict-setup site in TESTS too, not just the CLI --- every fixture below that
// exercises a *good* order calls it immediately after spot::make_bdd_dict(),
// before building any transducer.
namespace {

using ltlf_ek::DfaProduct;
using ltlf_ek::MtdfaProduct;
using ltlf_ek::OutputLabeledTransducer;
using ltlf_ek::register_turn_order_aps;
using ltlf_ek::require_turn_order_aps;
using ltlf_ek::Role;
using ltlf_ek::trivial_transducer;
using ltlf_ek::VariablePartition;

// ---------------------------------------------------------------------------
// register_turn_order_aps / require_turn_order_aps, in isolation (no
// MtdfaProduct involved) --- the direct unit-level half of oracle #3.
// ---------------------------------------------------------------------------

// A controllable AP registered strictly ABOVE (i.e. before, register_ap's
// registration order is the BDD variable order --- PRD "Interfaces & types")
// an Ifree AP violates the rule "every Ifree variable strictly above every
// controllable" (PRD "Interfaces & types", Phase 0/Q2).
TEST(TurnOrder, RequireThrowsWhenAnIfreeVariableSitsBelowAControllable) {
  auto dict = spot::make_bdd_dict();
  // Ifree = {z}, Ofree = {a} (controllable): 'a' registered first, so it
  // lands strictly above 'z' --- the bad order, built deliberately.
  auto vars = VariablePartition::split({"z"}, {"a"}, /*governed=*/{});
  auto g = spot::make_twa_graph(dict);
  g->register_ap("a");
  g->register_ap("z");
  EXPECT_THROW(require_turn_order_aps(vars, dict), std::invalid_argument);
}

TEST(TurnOrder, RequireAcceptsTheOrderRegisterEstablishes) {
  auto dict = spot::make_bdd_dict();
  auto vars = VariablePartition::split({"z"}, {"a"}, /*governed=*/{});
  register_turn_order_aps(vars, dict);
  EXPECT_NO_THROW(require_turn_order_aps(vars, dict));
}

// Order among the controllables is free (PRD "necessary and sufficient... the
// controllables may be in any relative order among themselves") --- both an
// Iknown-before-Ofree and an Ofree-before-Iknown registration must pass, as
// long as every Ifree variable sits strictly above BOTH.
TEST(TurnOrder, OrderAmongControllablesIsFree) {
  // Ifree={z}, Iknown={b}, Ofree={a}: a real known input, so both Iknown and
  // Ofree are genuinely controllable (decision 2).
  auto real_vars = VariablePartition::split({"z", "b"}, {"a"}, {"b"});
  {
    auto dict = spot::make_bdd_dict();
    register_turn_order_aps(real_vars, dict);
    EXPECT_NO_THROW(require_turn_order_aps(real_vars, dict))
        << "canonical order (Ifree, Iknown, Ofree, Oknown) must pass";
  }
  {
    // Manually register Ifree first, then Ofree before Iknown --- still
    // legal, since only the Ifree-above-controllables rule is enforced.
    auto dict = spot::make_bdd_dict();
    auto g = spot::make_twa_graph(dict);
    g->register_ap("z");
    g->register_ap("a");
    g->register_ap("b");
    EXPECT_NO_THROW(require_turn_order_aps(real_vars, dict))
        << "order AMONG controllables (Ofree vs Iknown) must be free";
  }
}

// Idempotence (PRD "Interfaces & types": "No-op for an AP already
// registered --- which is exactly why it cannot repair a bad order, only
// establish a good one"): once a bad order is manually established,
// register_turn_order_aps must NOT fix it, and require_turn_order_aps must
// still throw.
TEST(TurnOrder, RegisterIsANoOpOnAlreadyRegisteredNamesAndCannotRepairABadOrder) {
  auto dict = spot::make_bdd_dict();
  auto vars = VariablePartition::split({"z"}, {"a"}, /*governed=*/{});
  auto g = spot::make_twa_graph(dict);
  g->register_ap("a");  // controllable first: bad order, established early.
  g->register_ap("z");
  register_turn_order_aps(vars, dict);  // no-op: both names already exist.
  EXPECT_THROW(require_turn_order_aps(vars, dict), std::invalid_argument)
      << "register_turn_order_aps must not silently repair a pre-existing "
         "bad order --- idempotence is exactly why it cannot";
}

// ---------------------------------------------------------------------------
// The EK regression the order exists for (PRD "Test oracles" #3 bullet 2):
// Ifree={z}, Iknown={b}, Ofree={a}, phi Mealy-realizable --- MtdfaProduct
// must agree with DfaProduct, not report a spurious unrealizable.
// t_in commits b := z (Sigma0={z}, Sigma1={b}); phi = G(a <-> b) is
// realizable because the controller reads the known b (which equals the
// PREVIOUS z observed by t_in, i.e. the currently-known value) and copies
// it into a.
// ---------------------------------------------------------------------------

OutputLabeledTransducer CopyZIntoB(const spot::bdd_dict_ptr& dict) {
  auto g = spot::make_twa_graph(dict);
  const int z = g->register_ap("z");
  const int b = g->register_ap("b");
  g->new_states(1);
  g->set_init_state(0);
  g->new_edge(0, 0, bddtrue);
  const bdd relation =
      (bdd_ithvar(z) & bdd_ithvar(b)) | (bdd_nithvar(z) & bdd_nithvar(b));
  return OutputLabeledTransducer(g, {relation}, /*sigma0_cube=*/bdd_ithvar(z),
                                 /*sigma1_cube=*/bdd_ithvar(b));
}

TEST(TurnOrder, MtdfaProductAgreesWithDfaProductOnEkRegressionWhenOrderIsRegistered) {
  auto vars = VariablePartition::split({"z", "b"}, {"a"}, /*governed=*/{"b"});
  auto dict = spot::make_bdd_dict();
  register_turn_order_aps(vars, dict);  // dict-setup site: BEFORE any parse.
  const OutputLabeledTransducer t_in = CopyZIntoB(dict);
  const OutputLabeledTransducer t_out = trivial_transducer(vars, Role::t_out, dict);
  const spot::formula phi = spot::parse_formula("G(a <-> b)");

  DfaProduct dfa_method;
  MtdfaProduct mtdfa_method;
  const bool dfa_realizable =
      dfa_method.synthesize(phi, vars, t_in, t_out).has_value();
  const bool mtdfa_realizable =
      mtdfa_method.synthesize(phi, vars, t_in, t_out).has_value();

  EXPECT_TRUE(dfa_realizable)
      << "sanity: this EK spec is realizable by construction (the controller "
         "copies the known b, itself a copy of z, into a)";
  EXPECT_EQ(mtdfa_realizable, dfa_realizable)
      << "MtdfaProduct must agree with DfaProduct here, not report a "
         "spurious unrealizable (Phase 0/Q2)";
}

// Without a registered order, MtdfaProduct::synthesize must FAIL LOUDLY
// (require_turn_order_aps throws), never silently return the wrong verdict
// (PRD "Interfaces & types" turn_order.hpp: "Violating it does not crash: it
// silently returns a WRONG 'unrealizable'" --- true of the bare mtdfa
// machinery, but MtdfaProduct's frozen contract calls require_turn_order_aps
// FIRST specifically to convert that silent wrong answer into a loud one).
TEST(TurnOrder, MtdfaProductThrowsOnBadApOrderInsteadOfSilentlyMisreporting) {
  auto vars = VariablePartition::split({"z", "b"}, {"a"}, /*governed=*/{"b"});
  auto dict = spot::make_bdd_dict();
  // Reproduce today's (pre-fix) CLI bug: inputs() = {b, z} as one sorted set
  // (alphabetically b < z) registered before outputs() = {a} --- 'b'
  // (controllable) lands above 'z' (Ifree), the exact spurious-unrealizable
  // shape Phase 0/Q2 probed (PRD table: "b, z, a (today's CLI) ->
  // unrealizable spurious").
  auto registrar = spot::make_twa_graph(dict);
  registrar->register_ap("b");
  registrar->register_ap("z");
  registrar->register_ap("a");

  const OutputLabeledTransducer t_in = CopyZIntoB(dict);
  const OutputLabeledTransducer t_out = trivial_transducer(vars, Role::t_out, dict);
  const spot::formula phi = spot::parse_formula("G(a <-> b)");

  MtdfaProduct method;
  EXPECT_THROW(method.synthesize(phi, vars, t_in, t_out), std::invalid_argument)
      << "MtdfaProduct must call require_turn_order_aps FIRST (frozen "
         "contract) and fail loudly on a bad AP order, never silently "
         "return a wrong (un)realizability verdict";
}

}  // namespace
