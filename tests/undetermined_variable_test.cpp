#include <optional>
#include <set>
#include <string>

#include <gtest/gtest.h>
#include <bddx.h>
#include <spot/twa/bdddict.hh>
#include <spot/twa/twagraph.hh>

#include "ltlf_ek/transducer_io.hpp"

// U6 (docs/prd/output-dependencies-tool.md "Test oracles", Phase 1): a direct
// table test for `undetermined_variable`, the shared per-variable
// functionality predicate (docs/GLOSSARY.md: "determinacy witness") extracted
// from the inline check formerly at src/transducer_io.cpp:191-203.  Given
// `relation`, read as a relation from its non-`produced` variables to
// `produced`, `undetermined_variable` returns the name of a `produced`
// variable that some observation leaves undetermined, or nullopt iff
// `relation` is functional (I7).
namespace {

using ltlf_ek::undetermined_variable;

bdd VarBdd(const spot::twa_graph_ptr& aut, const std::string& name) {
  return bdd_ithvar(aut->register_ap(name));
}

// ---------------------------------------------------------------------------
// Functional relations: nullopt.
// ---------------------------------------------------------------------------

TEST(UndeterminedVariable, FunctionalRelationIsNullopt) {
  auto dict = spot::make_bdd_dict();
  auto aut = spot::make_twa_graph(dict);
  const bdd av = VarBdd(aut, "a"), kv = VarBdd(aut, "k");
  // a <-> k: k is exactly determined by a.
  const bdd relation = bdd_biimp(av, kv);
  EXPECT_EQ(undetermined_variable(relation, {"k"}, kv, aut), std::nullopt);
}

TEST(UndeterminedVariable, EmptyProducedSetIsVacuouslyNullopt) {
  auto dict = spot::make_bdd_dict();
  auto aut = spot::make_twa_graph(dict);
  const bdd av = VarBdd(aut, "a");
  // Nothing is produced, so nothing can be left undetermined.
  EXPECT_EQ(undetermined_variable(av, /*produced=*/{}, bddtrue, aut),
            std::nullopt);
}

// ---------------------------------------------------------------------------
// Non-functional relations: the offending produced-variable name.
// ---------------------------------------------------------------------------

TEST(UndeterminedVariable, NonFunctionalRelationReturnsOffendingName) {
  auto dict = spot::make_bdd_dict();
  auto aut = spot::make_twa_graph(dict);
  const bdd av = VarBdd(aut, "a"), kv = VarBdd(aut, "k");
  // a | k: at a=true, k is free (both k and !k stay live).
  const bdd relation = av | kv;
  EXPECT_EQ(undetermined_variable(relation, {"k"}, kv, aut),
            std::optional<std::string>("k"));
}

// A multi-variable produced set where only one of the two is undetermined:
// `m` does not appear in `relation` at all, so it is free regardless of the
// observation, while `k` alone (a <-> k) is still functional --- the witness
// must correctly single out `m`, not stop at `k` or report nothing.
TEST(UndeterminedVariable, PicksTheActuallyOffendingVariableAmongSeveral) {
  auto dict = spot::make_bdd_dict();
  auto aut = spot::make_twa_graph(dict);
  const bdd av = VarBdd(aut, "a"), kv = VarBdd(aut, "k"), mv = VarBdd(aut, "m");
  const bdd relation = bdd_biimp(av, kv);  // m unconstrained.
  EXPECT_EQ(undetermined_variable(relation, {"k", "m"}, kv & mv, aut),
            std::optional<std::string>("m"));
}

// ---------------------------------------------------------------------------
// The set case (I7): functional per-variable-alone, but not jointly ---
// exactly the property that makes the predicate correct for sets and not
// just singletons.  relation = (x <-> y) throughout.
// ---------------------------------------------------------------------------

// produced = {x} alone, with y implicitly OBSERVED (outside produced_cube):
// x is perfectly determined by y.  "Looks fine" in isolation.
TEST(UndeterminedVariable, XAloneIsFunctionalOfYWhenYIsObserved) {
  auto dict = spot::make_bdd_dict();
  auto aut = spot::make_twa_graph(dict);
  const bdd xv = VarBdd(aut, "x"), yv = VarBdd(aut, "y");
  const bdd relation = bdd_biimp(xv, yv);
  EXPECT_EQ(undetermined_variable(relation, {"x"}, xv, aut), std::nullopt);
}

// Symmetric: produced = {y} alone, x implicitly observed. Also "looks fine".
TEST(UndeterminedVariable, YAloneIsFunctionalOfXWhenXIsObserved) {
  auto dict = spot::make_bdd_dict();
  auto aut = spot::make_twa_graph(dict);
  const bdd xv = VarBdd(aut, "x"), yv = VarBdd(aut, "y");
  const bdd relation = bdd_biimp(xv, yv);
  EXPECT_EQ(undetermined_variable(relation, {"y"}, yv, aut), std::nullopt);
}

// produced = {x, y} JOINTLY, with an EMPTY observed set (Ydep = emptyset):
// at the (unique, empty) observation both (x,y)=(T,T) and (x,y)=(F,F) stay
// live, so neither x nor y is pinned down --- non-functional, and a name
// MUST come back even though each variable checked alone (above) looked
// fine.  This is the case a naive per-singleton implementation would miss.
TEST(UndeterminedVariable, SetCaseWithEmptyObservedReturnsAName) {
  auto dict = spot::make_bdd_dict();
  auto aut = spot::make_twa_graph(dict);
  const bdd xv = VarBdd(aut, "x"), yv = VarBdd(aut, "y");
  const bdd relation = bdd_biimp(xv, yv);
  const std::optional<std::string> result =
      undetermined_variable(relation, {"x", "y"}, xv & yv, aut);
  ASSERT_TRUE(result.has_value());
  EXPECT_TRUE(*result == "x" || *result == "y")
      << "expected the offending variable to be one of {x, y}, got "
      << *result;
}

}  // namespace
