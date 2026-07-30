#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include <gtest/gtest.h>
#include <bddx.h>
#include <spot/twa/bdddict.hh>
#include <spot/twa/twagraph.hh>

#include "ltlf_ek/consistency.hpp"
#include "ltlf_ek/output_labeled_transducer.hpp"
#include "ltlf_ek/transducer_io.hpp"
#include "ltlf_ek/variables.hpp"

// Unit fixtures for parse_transducer(...) (docs/GLOSSARY.md: "parse a
// transducer", "transducer file format", "role").  A file is a Spot HOA
// automaton for delta, then --- after --END-- --- a %%LAMBDA block giving lambda
// as one boolean formula per HOA state.  Sigma0/Sigma1 are NOT in the file; they
// are derived from (partition, role) and orient lambda.
namespace {

using ltlf_ek::consistent;
using ltlf_ek::OutputLabeledTransducer;
using ltlf_ek::parse_transducer;
using ltlf_ek::print_transducer;
using ltlf_ek::Role;
using ltlf_ek::sigma_slices;
using ltlf_ek::SigmaSlices;
using ltlf_ek::VariablePartition;

// The PRD's running example: 2 states over I∪O = {a, k}.
//   delta:  s0 --[a]--> s1,  s1 --[t]--> s1   (s0 on !a is a MISSING edge)
//   lambda: s0: a <-> k  (observe a, produce k := a),  s1: false (undefined)
constexpr const char* kExample = R"(HOA: v1
States: 2
Start: 0
AP: 2 "a" "k"
acc-name: all
Acceptance: 0 t
--BODY--
State: 0
  [0] 1                 /* edge guard over I∪O; missing edge = delta partial */
State: 1
  [t] 1
--END--

%%LAMBDA
state 0: a <-> k        /* observe a, produce k = a */
state 1: false          /* lambda undefined at q1   */
)";

// The remaining fixture texts, hoisted to namespace scope (rather than their
// original TEST-local scope) so the O2 round-trip section below can reuse
// them BY REFERENCE instead of re-typing the HOA text --- each still backs
// its original TEST unchanged.

// EmptySigma1ProducesEmptyCube: V = empty set ==> Sigma1 = empty set.
constexpr const char* kEmptySigma1Text = R"(HOA: v1
States: 1
Start: 0
AP: 1 "a"
acc-name: all
Acceptance: 0 t
--BODY--
State: 0
  [t] 0
--END--
%%LAMBDA
state 0: true
)";

// EmptySigma0GivesConstantOutput: Ifree = empty set ==> Sigma0 = empty set.
constexpr const char* kEmptySigma0Text = R"(HOA: v1
States: 1
Start: 0
AP: 1 "a"
acc-name: all
Acceptance: 0 t
--BODY--
State: 0
  [t] 0
--END--
%%LAMBDA
state 0: a
)";

// HoaAcceptanceIsIgnored: a Buchi acceptance condition parses but is never
// read as transducer finality (main.tex §101).
constexpr const char* kBuchiAcceptanceText = R"(HOA: v1
States: 1
Start: 0
AP: 1 "a"
acc-name: Buchi
Acceptance: 1 Inf(0)
--BODY--
State: 0
  [t] 0 {0}
--END--
%%LAMBDA
state 0: a
)";

// SharedDictFeedsConsistent: t_in governs k (produces k := a).
constexpr const char* kSharedDictTinText = R"(HOA: v1
States: 1
Start: 0
AP: 2 "a" "k"
acc-name: all
Acceptance: 0 t
--BODY--
State: 0
  [t] 0
--END--
%%LAMBDA
state 0: a <-> k
)";

// SharedDictFeedsConsistent: t_out governs e (produces e := true).
constexpr const char* kSharedDictToutText = R"(HOA: v1
States: 1
Start: 0
AP: 3 "a" "k" "e"
acc-name: all
Acceptance: 0 t
--BODY--
State: 0
  [t] 0
--END--
%%LAMBDA
state 0: e
)";

// EndTokenInsideCommentDoesNotSplitEarly: a decoy `--END--` lives inside an
// HOA comment and must not split the file early.
constexpr const char* kEndTokenInCommentText = R"(HOA: v1
States: 1
Start: 0
AP: 2 "a" "k"
acc-name: all
/* a decoy --END-- lives inside this comment */
Acceptance: 0 t
--BODY--
State: 0
  [t] 0
--END--
%%LAMBDA
state 0: a <-> k
)";

OutputLabeledTransducer Parse(const std::string& text,
                              const VariablePartition& part, Role role,
                              const spot::bdd_dict_ptr& dict) {
  std::istringstream in(text);
  return parse_transducer(in, part, role, dict);
}

// Variable number of `name` on the shared dict (idempotent registration --- the
// parser already registered the AP header, this returns the same variable).
bdd VarBdd(const spot::twa_graph_ptr& probe, const std::string& name) {
  return bdd_ithvar(probe->register_ap(name));
}

// Full letter over {a, k} with the given polarities.
bdd LetterAK(const spot::twa_graph_ptr& probe, bool a, bool k) {
  return (a ? VarBdd(probe, "a") : !VarBdd(probe, "a")) &
         (k ? VarBdd(probe, "k") : !VarBdd(probe, "k"));
}

// a = free input, k = known input (governed).  Role::t_in ⇒ Sigma0={a}, Sigma1={k}.
VariablePartition InFreeKnown() {
  return VariablePartition::split(/*inputs=*/{"a", "k"}, /*outputs=*/{},
                                  /*governed=*/{"k"});
}

// ---------------------------------------------------------------------------
// sigma_slices --- the (partition, role) -> (Sigma0, Sigma1) derivation,
// promoted to public so callers other than parse_transducer (the CLI's
// trivial_transducer, docs/prd/cli-wrapper.md) can reuse it directly.
// ---------------------------------------------------------------------------

TEST(SigmaSlicesDirect, TInIsIfreeAndIknown) {
  // Ifree = {a}, Iknown = {k}.
  const SigmaSlices s = sigma_slices(InFreeKnown(), Role::t_in);
  EXPECT_EQ(s.sigma0, (std::set<std::string>{"a"}));
  EXPECT_EQ(s.sigma1, (std::set<std::string>{"k"}));
}

TEST(SigmaSlicesDirect, TOutIsIUnionOfreeAndOknown) {
  // I = {a, k}, Ofree = {x}, Oknown = {y}.
  const VariablePartition part =
      VariablePartition::split({"a", "k"}, {"x", "y"}, /*governed=*/{"y"});
  const SigmaSlices s = sigma_slices(part, Role::t_out);
  EXPECT_EQ(s.sigma0, (std::set<std::string>{"a", "k", "x"}));
  EXPECT_EQ(s.sigma1, (std::set<std::string>{"y"}));
}

TEST(SigmaSlicesDirect, EmptyKnownSetGivesEmptySigma1) {
  const VariablePartition part = VariablePartition::split({"a"}, {}, {});
  const SigmaSlices s = sigma_slices(part, Role::t_in);
  EXPECT_TRUE(s.sigma1.empty());
}

// Role::t_c (docs/prd/controller-verifier.md, main.tex:125): Sigma0 = I,
// Sigma1 = Ofree --- the controller's own align-block row.
TEST(SigmaSlicesDirect, TCIsIAndOfree) {
  // I = {a, k}, Ofree = {x}, Oknown = {y}.
  const VariablePartition part =
      VariablePartition::split({"a", "k"}, {"x", "y"}, /*governed=*/{"y"});
  const SigmaSlices s = sigma_slices(part, Role::t_c);
  EXPECT_EQ(s.sigma0, (std::set<std::string>{"a", "k"}));
  EXPECT_EQ(s.sigma1, (std::set<std::string>{"x"}));
}

// ---------------------------------------------------------------------------
// Round-trip / unit fixture.
// ---------------------------------------------------------------------------

TEST(ParseTransducer, InitialStateIsHoaStart) {
  auto dict = spot::make_bdd_dict();
  auto t = Parse(kExample, InFreeKnown(), Role::t_in, dict);
  EXPECT_EQ(t.initial_state(), 0u);
}

TEST(ParseTransducer, DeltaFollowsHoaEdges) {
  auto dict = spot::make_bdd_dict();
  auto probe = spot::make_twa_graph(dict);
  auto t = Parse(kExample, InFreeKnown(), Role::t_in, dict);
  EXPECT_EQ(t.delta(0, LetterAK(probe, /*a=*/true, /*k=*/false)),
            std::optional<unsigned>(1));
  EXPECT_EQ(t.delta(1, LetterAK(probe, /*a=*/true, /*k=*/true)),
            std::optional<unsigned>(1));
}

TEST(ParseTransducer, MissingHoaEdgeIsNulloptDelta) {
  auto dict = spot::make_bdd_dict();
  auto probe = spot::make_twa_graph(dict);
  auto t = Parse(kExample, InFreeKnown(), Role::t_in, dict);
  // s0 has an edge only on a; !a satisfies no guard (partial delta, main.tex §107).
  EXPECT_EQ(t.delta(0, LetterAK(probe, /*a=*/false, /*k=*/false)), std::nullopt);
}

TEST(ParseTransducer, LambdaCommitsSigma1FromSigma0) {
  auto dict = spot::make_bdd_dict();
  auto probe = spot::make_twa_graph(dict);
  auto t = Parse(kExample, InFreeKnown(), Role::t_in, dict);
  const bdd kv = VarBdd(probe, "k");
  // s0: a <-> k commits k := a.
  EXPECT_EQ(t.lambda(0, LetterAK(probe, /*a=*/true, /*k=*/false)),
            std::optional<bdd>(kv));
  EXPECT_EQ(t.lambda(0, LetterAK(probe, /*a=*/false, /*k=*/true)),
            std::optional<bdd>(!kv));
}

TEST(ParseTransducer, LambdaFalseEntryIsNullopt) {
  auto dict = spot::make_bdd_dict();
  auto probe = spot::make_twa_graph(dict);
  auto t = Parse(kExample, InFreeKnown(), Role::t_in, dict);
  // s1: false ⇒ lambda undefined everywhere (partial lambda).
  EXPECT_EQ(t.lambda(1, LetterAK(probe, /*a=*/true, /*k=*/true)), std::nullopt);
  EXPECT_EQ(t.lambda(1, LetterAK(probe, /*a=*/false, /*k=*/false)), std::nullopt);
}

// parse_transducer wired to Role::t_c (docs/prd/controller-verifier.md): a =
// input, k = output-free --- Sigma0 = I = {a}, Sigma1 = Ofree = {k}.
TEST(ParseTransducer, RoleTcSigma0IsInputsSigma1IsOutputFree) {
  auto dict = spot::make_bdd_dict();
  auto probe = spot::make_twa_graph(dict);
  auto part = VariablePartition::split(/*inputs=*/{"a"}, /*outputs=*/{"k"},
                                       /*governed=*/{});
  auto t = Parse(kExample, part, Role::t_c, dict);
  EXPECT_EQ(t.sigma0_cube(), VarBdd(probe, "a"));
  EXPECT_EQ(t.sigma1_cube(), VarBdd(probe, "k"));
  // state 0: a <-> k --- lambda_C commits Ofree k := a (main.tex:125).
  EXPECT_EQ(t.lambda(0, LetterAK(probe, /*a=*/true, /*k=*/false)),
            std::optional<bdd>(VarBdd(probe, "k")));
}

// ---------------------------------------------------------------------------
// Orientation: the SAME file yields swapped Sigma0/Sigma1 (and transposed
// lambda) under a partition that reclassifies `a` vs `k` --- orientation comes
// from the partition, not the formula.
// ---------------------------------------------------------------------------

TEST(ParseTransducer, OrientationComesFromPartitionNotFormula) {
  auto dict = spot::make_bdd_dict();
  auto probe = spot::make_twa_graph(dict);
  const bdd av = VarBdd(probe, "a"), kv = VarBdd(probe, "k");

  // P1: a free, k known ⇒ Sigma0={a}, Sigma1={k}; a<->k commits k := a.
  auto t1 = Parse(kExample, InFreeKnown(), Role::t_in, dict);
  // P2: a known, k free ⇒ Sigma0={k}, Sigma1={a}; a<->k commits a := k.
  auto p2 = VariablePartition::split({"a", "k"}, {}, {"a"});
  auto t2 = Parse(kExample, p2, Role::t_in, dict);

  // Slices are swapped.
  EXPECT_EQ(t1.sigma0_cube(), t2.sigma1_cube());
  EXPECT_EQ(t1.sigma1_cube(), t2.sigma0_cube());

  // lambda is transposed: t1 produces k from a, t2 produces a from k.
  EXPECT_EQ(t1.lambda(0, LetterAK(probe, /*a=*/true, /*k=*/false)),
            std::optional<bdd>(kv));
  EXPECT_EQ(t2.lambda(0, LetterAK(probe, /*a=*/false, /*k=*/true)),
            std::optional<bdd>(av));
}

// ---------------------------------------------------------------------------
// Derived-slice equality: parsed cubes equal cubes built directly from
// (partition, role).
// ---------------------------------------------------------------------------

TEST(ParseTransducer, DerivedSlicesMatchAlignBlock) {
  auto dict = spot::make_bdd_dict();
  auto probe = spot::make_twa_graph(dict);
  auto t = Parse(kExample, InFreeKnown(), Role::t_in, dict);
  EXPECT_EQ(t.sigma0_cube(), VarBdd(probe, "a"));  // Sigma0 = Ifree = {a}
  EXPECT_EQ(t.sigma1_cube(), VarBdd(probe, "k"));  // Sigma1 = Iknown = {k}
}

// ---------------------------------------------------------------------------
// Abuse-of-notation (main.tex §87): lambda reads only its Sigma0 slice, so
// flipping a variable outside Sigma0 (here the Sigma1 var k) never changes it.
// ---------------------------------------------------------------------------

TEST(ParseTransducer, LambdaIgnoresVariablesOutsideSigma0) {
  auto dict = spot::make_bdd_dict();
  auto probe = spot::make_twa_graph(dict);
  auto t = Parse(kExample, InFreeKnown(), Role::t_in, dict);
  EXPECT_EQ(t.lambda(0, LetterAK(probe, /*a=*/true, /*k=*/false)),
            t.lambda(0, LetterAK(probe, /*a=*/true, /*k=*/true)));
}

// ---------------------------------------------------------------------------
// Edge cases: empty Sigma1 (monolithic baseline) and empty Sigma0 (constant).
// ---------------------------------------------------------------------------

// V = ∅ ⇒ Sigma1 = ∅: `state 0: true` produces the empty cube (bddtrue).
TEST(ParseTransducer, EmptySigma1ProducesEmptyCube) {
  auto dict = spot::make_bdd_dict();
  auto probe = spot::make_twa_graph(dict);
  // a free input, nothing governed ⇒ Sigma0={a}, Sigma1=∅.
  auto part = VariablePartition::split({"a"}, {}, {});
  auto t = Parse(kEmptySigma1Text, part, Role::t_in, dict);
  EXPECT_EQ(t.sigma1_cube(), bddtrue);
  const bdd v = VarBdd(probe, "a");
  EXPECT_EQ(t.lambda(0, v), std::optional<bdd>(bddtrue));
  EXPECT_EQ(t.lambda(0, !v), std::optional<bdd>(bddtrue));
}

// Ifree = ∅ ⇒ Sigma0 = ∅: lambda is a constant per state, independent of input.
TEST(ParseTransducer, EmptySigma0GivesConstantOutput) {
  auto dict = spot::make_bdd_dict();
  auto probe = spot::make_twa_graph(dict);
  // a known input, nothing free ⇒ Sigma0=∅, Sigma1={a}.
  auto part = VariablePartition::split({"a"}, {}, {"a"});
  auto t = Parse(kEmptySigma0Text, part, Role::t_in, dict);
  EXPECT_EQ(t.sigma0_cube(), bddtrue);
  const bdd av = VarBdd(probe, "a");
  // `state 0: a` fixes a := true regardless of the (empty) observation.
  EXPECT_EQ(t.lambda(0, av), std::optional<bdd>(av));
  EXPECT_EQ(t.lambda(0, !av), std::optional<bdd>(av));
}

// HOA acceptance is parsed but never read as finality (main.tex §101): a Büchi
// automaton parses and its delta works exactly as an `all`-acceptance one.
TEST(ParseTransducer, HoaAcceptanceIsIgnored) {
  auto dict = spot::make_bdd_dict();
  auto probe = spot::make_twa_graph(dict);
  auto part = VariablePartition::split({"a"}, {}, {"a"});
  auto t = Parse(kBuchiAcceptanceText, part, Role::t_in, dict);
  EXPECT_EQ(t.delta(0, VarBdd(probe, "a")), std::optional<unsigned>(0));
}

// ---------------------------------------------------------------------------
// Shared-dict integration: parse t_in and t_out on ONE dict and check that
// consistent(...) fires on the intended letters (the from-disk path).
// ---------------------------------------------------------------------------

TEST(ParseTransducer, SharedDictFeedsConsistent) {
  // t_in governs k (produces k := a); t_out governs e (produces e := true).
  auto dict = spot::make_bdd_dict();
  auto probe = spot::make_twa_graph(dict);
  // inputs={a,k}, outputs={e}, governed V={k,e}.
  auto part = VariablePartition::split({"a", "k"}, {"e"}, {"k", "e"});
  auto t_in = Parse(kSharedDictTinText, part, Role::t_in, dict);
  auto t_out = Parse(kSharedDictToutText, part, Role::t_out, dict);

  auto L = [&](bool a, bool k, bool e) {
    return (a ? VarBdd(probe, "a") : !VarBdd(probe, "a")) &
           (k ? VarBdd(probe, "k") : !VarBdd(probe, "k")) &
           (e ? VarBdd(probe, "e") : !VarBdd(probe, "e"));
  };
  // cons ⇔ (k == a, per lambda_in) ∧ (e == true, per lambda_out).
  EXPECT_TRUE(consistent(t_in, 0, t_out, 0, L(/*a=*/true, /*k=*/true, /*e=*/true)));
  EXPECT_TRUE(consistent(t_in, 0, t_out, 0, L(/*a=*/false, /*k=*/false, /*e=*/true)));
  EXPECT_FALSE(consistent(t_in, 0, t_out, 0, L(/*a=*/true, /*k=*/false, /*e=*/true)));
  EXPECT_FALSE(consistent(t_in, 0, t_out, 0, L(/*a=*/true, /*k=*/true, /*e=*/false)));
}

// ---------------------------------------------------------------------------
// Well-formedness / parse rejections --- each throws std::invalid_argument.
// ---------------------------------------------------------------------------

// Non-functional lambda: `a | k` under Sigma0={a} leaves k free when a=1.
TEST(ParseTransducer, RejectsNonFunctionalLambda) {
  constexpr const char* kFile = R"(HOA: v1
States: 1
Start: 0
AP: 2 "a" "k"
acc-name: all
Acceptance: 0 t
--BODY--
State: 0
  [t] 0
--END--
%%LAMBDA
state 0: a | k
)";
  auto dict = spot::make_bdd_dict();
  EXPECT_THROW(Parse(kFile, InFreeKnown(), Role::t_in, dict),
               std::invalid_argument);
}

// Non-deterministic delta: two overlapping guards out of s0.
TEST(ParseTransducer, RejectsNonDeterministicDelta) {
  constexpr const char* kFile = R"(HOA: v1
States: 2
Start: 0
AP: 2 "a" "k"
acc-name: all
Acceptance: 0 t
--BODY--
State: 0
  [0] 1
  [t] 0
State: 1
  [t] 1
--END--
%%LAMBDA
state 0: a <-> k
state 1: a <-> k
)";
  auto dict = spot::make_bdd_dict();
  EXPECT_THROW(Parse(kFile, InFreeKnown(), Role::t_in, dict),
               std::invalid_argument);
}

// Missing %%LAMBDA entry for a state (a missing entry is an error, not an
// implicit undefined).
TEST(ParseTransducer, RejectsMissingStateEntry) {
  constexpr const char* kFile = R"(HOA: v1
States: 2
Start: 0
AP: 2 "a" "k"
acc-name: all
Acceptance: 0 t
--BODY--
State: 0
  [0] 1
State: 1
  [t] 1
--END--
%%LAMBDA
state 0: a <-> k
)";
  auto dict = spot::make_bdd_dict();
  EXPECT_THROW(Parse(kFile, InFreeKnown(), Role::t_in, dict),
               std::invalid_argument);
}

// %%LAMBDA state index beyond num_states.
TEST(ParseTransducer, RejectsOutOfRangeStateEntry) {
  constexpr const char* kFile = R"(HOA: v1
States: 1
Start: 0
AP: 2 "a" "k"
acc-name: all
Acceptance: 0 t
--BODY--
State: 0
  [t] 0
--END--
%%LAMBDA
state 5: a <-> k
)";
  auto dict = spot::make_bdd_dict();
  EXPECT_THROW(Parse(kFile, InFreeKnown(), Role::t_in, dict),
               std::invalid_argument);
}

// Two entries for the same state.
TEST(ParseTransducer, RejectsDuplicateStateEntry) {
  constexpr const char* kFile = R"(HOA: v1
States: 1
Start: 0
AP: 2 "a" "k"
acc-name: all
Acceptance: 0 t
--BODY--
State: 0
  [t] 0
--END--
%%LAMBDA
state 0: a <-> k
state 0: a <-> k
)";
  auto dict = spot::make_bdd_dict();
  EXPECT_THROW(Parse(kFile, InFreeKnown(), Role::t_in, dict),
               std::invalid_argument);
}

// A lambda formula naming an AP outside Sigma0 ∪ Sigma1.
TEST(ParseTransducer, RejectsApOutsideScope) {
  constexpr const char* kFile = R"(HOA: v1
States: 1
Start: 0
AP: 2 "a" "k"
acc-name: all
Acceptance: 0 t
--BODY--
State: 0
  [t] 0
--END--
%%LAMBDA
state 0: k & o
)";
  auto dict = spot::make_bdd_dict();
  // Sigma0∪Sigma1 = {a,k}; `o` is out of scope.
  EXPECT_THROW(Parse(kFile, InFreeKnown(), Role::t_in, dict),
               std::invalid_argument);
}

// A non-boolean (temporal) lambda formula.
TEST(ParseTransducer, RejectsNonBooleanLambda) {
  constexpr const char* kFile = R"(HOA: v1
States: 1
Start: 0
AP: 2 "a" "k"
acc-name: all
Acceptance: 0 t
--BODY--
State: 0
  [t] 0
--END--
%%LAMBDA
state 0: X a
)";
  auto dict = spot::make_bdd_dict();
  EXPECT_THROW(Parse(kFile, InFreeKnown(), Role::t_in, dict),
               std::invalid_argument);
}

// No HOA --END-- terminator.
TEST(ParseTransducer, RejectsMissingEndTerminator) {
  constexpr const char* kFile = R"(HOA: v1
States: 1
Start: 0
AP: 2 "a" "k"
acc-name: all
Acceptance: 0 t
--BODY--
State: 0
  [t] 0
)";
  auto dict = spot::make_bdd_dict();
  EXPECT_THROW(Parse(kFile, InFreeKnown(), Role::t_in, dict),
               std::invalid_argument);
}

// --END-- present but no %%LAMBDA block after it.
TEST(ParseTransducer, RejectsMissingLambdaBlock) {
  constexpr const char* kFile = R"(HOA: v1
States: 1
Start: 0
AP: 2 "a" "k"
acc-name: all
Acceptance: 0 t
--BODY--
State: 0
  [t] 0
--END--
)";
  auto dict = spot::make_bdd_dict();
  EXPECT_THROW(Parse(kFile, InFreeKnown(), Role::t_in, dict),
               std::invalid_argument);
}

// An HOA-declared AP outside I∪O (the partition is the closed universe of APs).
TEST(ParseTransducer, RejectsHoaApOutsideUniverse) {
  constexpr const char* kFile = R"(HOA: v1
States: 2
Start: 0
AP: 3 "a" "k" "z"
acc-name: all
Acceptance: 0 t
--BODY--
State: 0
  [2] 1
  [!2] 0
State: 1
  [t] 1
--END--
%%LAMBDA
state 0: a <-> k
state 1: a <-> k
)";
  auto dict = spot::make_bdd_dict();
  // Partition covers only {a,k}; the delta guard on `z` must be rejected.
  EXPECT_THROW(Parse(kFile, InFreeKnown(), Role::t_in, dict),
               std::invalid_argument);
}

// A `--END--` inside an HOA block comment must NOT split the file early: the
// real terminator is the one on its own line.
TEST(ParseTransducer, EndTokenInsideCommentDoesNotSplitEarly) {
  auto dict = spot::make_bdd_dict();
  auto probe = spot::make_twa_graph(dict);
  auto t = Parse(kEndTokenInCommentText, InFreeKnown(), Role::t_in, dict);
  // Parsed past the decoy: delta and lambda come from the real body.
  EXPECT_EQ(t.delta(0, LetterAK(probe, /*a=*/true, /*k=*/true)),
            std::optional<unsigned>(0));
  EXPECT_EQ(t.lambda(0, LetterAK(probe, /*a=*/true, /*k=*/false)),
            std::optional<bdd>(VarBdd(probe, "k")));
}

// Garbage where the HOA should be.
TEST(ParseTransducer, RejectsMalformedHoa) {
  constexpr const char* kFile = R"(not a valid automaton
--END--
%%LAMBDA
state 0: a <-> k
)";
  auto dict = spot::make_bdd_dict();
  EXPECT_THROW(Parse(kFile, InFreeKnown(), Role::t_in, dict),
               std::invalid_argument);
}

// ---------------------------------------------------------------------------
// O2 --- round-trip oracle (Phase 1, docs/prd/output-dependencies-tool.md
// "Test oracles"): parse_transducer(print_transducer(t)) == t, for every
// transducer fixture above that successfully PARSES into a `t` (the
// rejection fixtures throw during parsing and produce no `t` to round-trip;
// out of scope here).  U1/U4's emitted transducers are Phase 2, also out of
// scope --- Phase 1 only exercises the fixtures parse_transducer already had.
//
// "==" is checked structurally rather than via an operator==, since
// OutputLabeledTransducer has none: same state count (delta_dfa()'s new
// accessor), same initial state, BDD-equal edge guards per (src,dst)
// (delta_edges), BDD-equal lambda per state (emits_region, documented as
// exactly the stored Sigma0∪Sigma1 relation --- transducer.hpp).  BuDDy
// canonicalises bdd values, so `==` on them is semantic equality (same
// argument as the Build-equivalence metamorphic oracle).
// ---------------------------------------------------------------------------

// delta_edges(q) as one guard per destination state, ORing together any
// (src,dst) pair split across several HOA edges --- comparison is over
// (src,dst), not raw edge count.
std::vector<bdd> GuardsByDst(const OutputLabeledTransducer& t, unsigned q,
                              unsigned n_states) {
  std::vector<bdd> by_dst(n_states, bddfalse);
  for (const auto& [guard, dst] : t.delta_edges(q)) by_dst[dst] |= guard;
  return by_dst;
}

void ExpectRoundTrips(const OutputLabeledTransducer& t,
                      const VariablePartition& part, Role role,
                      const spot::bdd_dict_ptr& dict) {
  std::ostringstream printed;
  print_transducer(printed, t);
  std::istringstream in(printed.str());
  const OutputLabeledTransducer t2 = parse_transducer(in, part, role, dict);

  const unsigned n = t.delta_dfa()->num_states();
  ASSERT_EQ(t2.delta_dfa()->num_states(), n) << "state count";
  EXPECT_EQ(t2.initial_state(), t.initial_state());

  for (unsigned q = 0; q < n; ++q) {
    EXPECT_EQ(GuardsByDst(t2, q, n), GuardsByDst(t, q, n))
        << "edge guards at state " << q;
    EXPECT_EQ(t2.emits_region(q), t.emits_region(q))
        << "lambda at state " << q;
  }
}

TEST(RoundTrip, KExampleUnderTIn) {
  auto dict = spot::make_bdd_dict();
  ExpectRoundTrips(Parse(kExample, InFreeKnown(), Role::t_in, dict),
                   InFreeKnown(), Role::t_in, dict);
}

// Same text, Role::t_c orientation (RoleTcSigma0IsInputsSigma1IsOutputFree's
// partition) --- a distinct lambda shape from the t_in case above.
TEST(RoundTrip, KExampleUnderTC) {
  auto dict = spot::make_bdd_dict();
  auto part = VariablePartition::split(/*inputs=*/{"a"}, /*outputs=*/{"k"},
                                       /*governed=*/{});
  ExpectRoundTrips(Parse(kExample, part, Role::t_c, dict), part, Role::t_c,
                   dict);
}

// Same text again, the orientation-swapped partition from
// OrientationComesFromPartitionNotFormula --- lambda is transposed relative
// to KExampleUnderTIn above.
TEST(RoundTrip, KExampleUnderSwappedPartition) {
  auto dict = spot::make_bdd_dict();
  auto part = VariablePartition::split({"a", "k"}, {}, {"a"});
  ExpectRoundTrips(Parse(kExample, part, Role::t_in, dict), part, Role::t_in,
                   dict);
}

TEST(RoundTrip, EmptySigma1Fixture) {
  auto dict = spot::make_bdd_dict();
  auto part = VariablePartition::split({"a"}, {}, {});
  ExpectRoundTrips(Parse(kEmptySigma1Text, part, Role::t_in, dict), part,
                   Role::t_in, dict);
}

TEST(RoundTrip, EmptySigma0Fixture) {
  auto dict = spot::make_bdd_dict();
  auto part = VariablePartition::split({"a"}, {}, {"a"});
  ExpectRoundTrips(Parse(kEmptySigma0Text, part, Role::t_in, dict), part,
                   Role::t_in, dict);
}

// HOA acceptance (Buchi, in this fixture) is meaningless to a transducer, so
// print_transducer normalises it away and emits the canonical `Acceptance: 0 t`
// instead.  The round-trip holds on delta/lambda alone --- and would hold even
// if acceptance were copied through verbatim, since parse_transducer never
// reads it (see HoaAcceptanceIsIgnored).
TEST(RoundTrip, BuchiAcceptanceFixture) {
  auto dict = spot::make_bdd_dict();
  auto part = VariablePartition::split({"a"}, {}, {"a"});
  ExpectRoundTrips(Parse(kBuchiAcceptanceText, part, Role::t_in, dict), part,
                   Role::t_in, dict);
}

// The normalisation itself, which ExpectRoundTrips cannot see (it compares
// delta/lambda, and parse_transducer never reads acceptance either way).  A
// transducer has no F, so the emitted file must not advertise one --- see
// docs/prd/output-dependencies-tool.md "Developer comments": once Phase 2
// builds a Tout on the Goal DFA, copy-through would publish F_D as
// omega-acceptance for what is finite-word reachability.
TEST(PrintTransducer, NormalisesAcceptanceAway) {
  auto dict = spot::make_bdd_dict();
  auto part = VariablePartition::split({"a"}, {}, {"a"});
  std::ostringstream out;
  print_transducer(out, Parse(kBuchiAcceptanceText, part, Role::t_in, dict));
  const std::string text = out.str();
  EXPECT_NE(text.find("Acceptance: 0 t"), std::string::npos) << text;
  EXPECT_EQ(text.find("Inf(0)"), std::string::npos) << text;
  // The state's `{0}` acceptance mark must be gone too, or the emitted HOA
  // would name a set its Acceptance line does not declare.
  EXPECT_EQ(text.find("{0}"), std::string::npos) << text;
}

TEST(RoundTrip, SharedDictTinFixture) {
  auto dict = spot::make_bdd_dict();
  auto part = VariablePartition::split({"a", "k"}, {"e"}, {"k", "e"});
  ExpectRoundTrips(Parse(kSharedDictTinText, part, Role::t_in, dict), part,
                   Role::t_in, dict);
}

TEST(RoundTrip, SharedDictToutFixture) {
  auto dict = spot::make_bdd_dict();
  auto part = VariablePartition::split({"a", "k"}, {"e"}, {"k", "e"});
  ExpectRoundTrips(Parse(kSharedDictToutText, part, Role::t_out, dict), part,
                   Role::t_out, dict);
}

// The decoy `--END--` inside a comment only matters to the FIRST parse; the
// printed form has no such comment, so this also guards that print_transducer
// does not itself reintroduce anything ambiguous.
TEST(RoundTrip, EndTokenInCommentFixture) {
  auto dict = spot::make_bdd_dict();
  ExpectRoundTrips(Parse(kEndTokenInCommentText, InFreeKnown(), Role::t_in,
                        dict),
                   InFreeKnown(), Role::t_in, dict);
}

}  // namespace
