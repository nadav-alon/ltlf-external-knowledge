// O4 -- controller-verifier oracle (docs/prd/output-dependencies-tool.md
// "Test oracles"): "Where the emitted Tout is non-trivial and synthesis
// succeeds, verify_controller(phi, vars, trivial_t_in, t_out, t_c) must
// accept -- the internal linchpin oracle, independent of the method that
// produced t_c."
//
// Library-level throughout: `dependent_outputs` returns a fully-formed
// OutputLabeledTransducer, so O4 drives DfaProduct::synthesize and
// verify_controller directly and needs neither the `ltlf-ek-deps` binary nor
// ltlfsynt. The CLI-level twin of the same property is O1, in
// tests/ltlf_ek_deps_test.cpp.

#include <optional>
#include <random>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include <gtest/gtest.h>
#include <bddx.h>
#include <spot/misc/optionmap.hh>
#include <spot/tl/defaultenv.hh>
#include <spot/tl/parse.hh>
#include <spot/tl/randomltl.hh>
#include <spot/twa/bdddict.hh>

#include "ltlf_ek/dependent_outputs.hpp"
#include "ltlf_ek/dfa_product.hpp"
#include "ltlf_ek/output_labeled_transducer.hpp"
#include "ltlf_ek/role.hpp"
#include "ltlf_ek/synthesis.hpp"
#include "ltlf_ek/variables.hpp"
#include "ltlf_ek/verify_controller.hpp"

#include "support/fixtures.hpp"

namespace {

using ltlf_ek::Controller;
using ltlf_ek::dependent_outputs;
using ltlf_ek::DependentOutputs;
using ltlf_ek::DfaProduct;
using ltlf_ek::OutputLabeledTransducer;
using ltlf_ek::Role;
using ltlf_ek::trivial_transducer;
using ltlf_ek::VariablePartition;
using ltlf_ek::verify_controller;
using ltlf_ek::test_support::Phi;

// ---------------------------------------------------------------------------
// Hand-picked fixtures.  Reuses the exact (phi, partition) shapes already
// hand-verified for `dependent_outputs` (U1, U3/U5) in
// tests/dependent_outputs_test.cpp -- NOT duplicating those tests, only their
// concrete data, to check a DIFFERENT property here (that the emitted T_out
// survives synthesis + verification) -- plus three shapes pushed past that
// file's "single free input, one output" axis: two G(pure boolean condition)
// fixtures widened to a second free input (same "one live state" shape as
// U1, so the expected Xdep is hand-verifiable the same way), and the I =
// empty edge case.
// ---------------------------------------------------------------------------

struct O4Fixture {
  std::string name;
  std::string phi;
  std::set<std::string> inputs;
  std::set<std::string> outputs;
};

void PrintTo(const O4Fixture& f, std::ostream* os) { *os << f.name; }

std::vector<O4Fixture> RealizableFixtures() {
  using Names = std::set<std::string>;
  return {
      // U1 shape: x tracks a: Xdep={x}, Ofree becomes empty once x moves to
      // Oknown.
      {"CopyInputToOutput", "G(a <-> x)", Names{"a"}, Names{"x"}},
      // U3/U5 shape: only {x} is dependent (lexicographic-greedy); {y} stays
      // Ofree and the controller must still mirror the now-known x every
      // step.
      {"EqualOutputsOneDependentOneFree", "G(x <-> y)", Names{"a"},
       Names{"x", "y"}},
      // Two free inputs (pushed past U1/U3's single-input axis), same
      // "one-live-state G(pure boolean condition)" shape as U1 --
      // (a & b) <-> x is a total function of (a,b), so the live-letter
      // region at the sole live state is functional: x = a & b.
      {"TwoInputsConjunction", "G((a & b) <-> x)", Names{"a", "b"},
       Names{"x"}},
      // A deeper boolean nesting (double biconditional) on the same shape:
      // x = (a <-> b), still a total function of (a,b).
      {"NestedBiconditionalTwoInputs", "G(x <-> (a <-> b))", Names{"a", "b"},
       Names{"x"}},
      // I = empty edge case (dependent_outputs_test.cpp
      // EmptyInputSetStillAnalysesOutputs): x is pinned outright, no input at
      // all.
      {"EmptyInputs", "G(x)", Names{}, Names{"x"}},
  };
}

class ControllerVerifierOracle : public ::testing::TestWithParam<O4Fixture> {
};

TEST_P(ControllerVerifierOracle, SynthesizedControllerVerifiesSafe) {
  const O4Fixture& f = GetParam();
  auto dict = spot::make_bdd_dict();
  const VariablePartition part =
      VariablePartition::split(f.inputs, f.outputs, /*governed=*/{});
  const spot::formula phi = Phi(f.phi);

  const DependentOutputs result = dependent_outputs(phi, part, dict);
  ASSERT_FALSE(result.dependent.empty())
      << "fixture is expected to have a non-empty Xdep";
  ASSERT_TRUE(result.t_out.has_value());

  const OutputLabeledTransducer t_in =
      trivial_transducer(result.partition, Role::t_in, dict);
  DfaProduct method;
  const std::optional<Controller> controller =
      method.synthesize(phi, result.partition, t_in, *result.t_out);
  ASSERT_TRUE(controller.has_value())
      << "expected synthesis to succeed against the emitted Tout for phi="
      << f.phi;

  const auto verdict = verify_controller(phi, result.partition, t_in,
                                        *result.t_out, *controller);
  EXPECT_TRUE(verdict.ok)
      << "verify_controller rejected a synthesized T_C against the emitted "
         "Tout for phi="
      << f.phi;
}

INSTANTIATE_TEST_SUITE_P(
    Fixtures, ControllerVerifierOracle,
    ::testing::ValuesIn(RealizableFixtures()),
    [](const ::testing::TestParamInfo<O4Fixture>& info) {
      return info.param.name;
    });

// ---------------------------------------------------------------------------
// I4 companion at the O4 layer (U4, phi = G(!a) & G(x)): plain synthesis of
// phi is UNREALIZABLE (the environment can always play a). Here Xdep == O,
// so Ofree becomes empty and the whole outcome rests on the emitted,
// correctly-totalized Tout alone -- there is no controller left to save it.
// A correct totalization must NOT let that fixed strategy accidentally win:
// synthesize must still report unrealizable (nullopt), matching the bare-phi
// baseline -- this is the library-level twin of the equirealizability
// property tests/ltlf_ek_deps_test.cpp's O1 checks end-to-end via the CLI.
// ---------------------------------------------------------------------------

TEST(ControllerVerifierOracleI4Companion,
    TotalizedToutStaysUnrealizableNotFalselyWon) {
  auto dict = spot::make_bdd_dict();
  const VariablePartition part = VariablePartition::split(
      /*inputs=*/{"a"}, /*outputs=*/{"x"}, /*governed=*/{});
  const spot::formula phi = Phi("G(!a) & G(x)");

  const DependentOutputs result = dependent_outputs(phi, part, dict);
  ASSERT_EQ(result.dependent, (std::set<std::string>{"x"}));
  ASSERT_TRUE(result.t_out.has_value());
  ASSERT_TRUE(result.partition.output_free.empty())
      << "Xdep == O here, so Ofree must be empty and the controller has "
         "nothing left to contribute";

  const OutputLabeledTransducer t_in =
      trivial_transducer(result.partition, Role::t_in, dict);
  DfaProduct method;
  const std::optional<Controller> controller =
      method.synthesize(phi, result.partition, t_in, *result.t_out);
  EXPECT_FALSE(controller.has_value())
      << "a correctly totalized Tout must not falsely flip G(!a) & G(x) to "
         "realizable (I4)";
}

// ---------------------------------------------------------------------------
// Generated corpus (library-level only -- no ltlfsynt, no ltlf-ek-deps
// binary): a small, fixed-seed random-formula / random-partition generator
// (same technique as tests/ltlfsynt_oracle_test.cpp's GeneratedCorpus,
// duplicated per this project's one-file-per-suite convention, since that
// file's helpers are anonymous-namespace TU-locals), restricted post hoc to
// cases where `dependent_outputs` finds a non-empty Xdep AND
// DfaProduct::synthesize succeeds against the emitted Tout. On every such
// case, verify_controller must accept. Reports its own vacuousness guard
// (mirrors O1's PRD-required assertion): a non-trivial fraction of the
// corpus must reach the dependent+realizable subset this oracle exercises.
// ---------------------------------------------------------------------------

constexpr unsigned kO4CorpusSeed = 20260731;
constexpr std::size_t kO4CorpusCaseCount = 300;
constexpr int kO4TreeSizeMin = 1;
constexpr int kO4TreeSizeMax = 8;
constexpr int kO4InputMax = 3;
constexpr int kO4OutputMax = 3;

// Same technique as tests/ltlfsynt_oracle_test.cpp's generate_random_formula:
// a thin wrapper over spot::randltlgenerator, APs drawn from `partition`'s
// exact I union O (partition-first generation), operator palette restricted
// to the LTLf-safe set via priorities (xor/M disabled).
spot::formula GenerateRandomFormula(const VariablePartition& partition,
                                    std::mt19937& rng) {
  std::set<std::string> ap_names = partition.inputs();
  for (const std::string& name : partition.outputs()) ap_names.insert(name);

  spot::atomic_prop_set aprops;
  for (const std::string& name : ap_names)
    aprops.insert(spot::default_environment::instance().require(name));

  spot::option_map opts;
  opts.set("output", spot::randltlgenerator::LTL);
  opts.set("tree_size_min", kO4TreeSizeMin);
  opts.set("tree_size_max", kO4TreeSizeMax);
  opts.set("seed", static_cast<int>(rng()));

  // parse_options (called by the randltlgenerator ctor) needs a mutable
  // char* buffer (it strtok()s in place), not a string literal.
  std::string priorities_str = "xor=0,M=0";
  std::vector<char> priorities(priorities_str.begin(), priorities_str.end());
  priorities.push_back('\0');

  spot::randltlgenerator rg(aprops, opts, priorities.data());
  const spot::formula phi = rg.next();
  if (!phi)
    throw std::runtime_error(
        "GenerateRandomFormula: randltlgenerator produced no formula");
  return phi;
}

// Partition-first: >=1 input, >=1 output, no Iknown/Oknown at all (I9
// requires an empty output_known on input; this oracle needs only the
// analysis, not a pre-existing V).
VariablePartition RandomPartition(std::mt19937& rng) {
  std::uniform_int_distribution<int> input_count(1, kO4InputMax);
  std::uniform_int_distribution<int> output_count(1, kO4OutputMax);
  const int n_inputs = input_count(rng);
  const int n_outputs = output_count(rng);

  std::set<std::string> inputs, outputs;
  int next_id = 0;
  for (int i = 0; i < n_inputs; ++i)
    inputs.insert("p" + std::to_string(next_id++));
  for (int i = 0; i < n_outputs; ++i)
    outputs.insert("p" + std::to_string(next_id++));
  return VariablePartition::split(inputs, outputs, /*governed=*/{});
}

TEST(ControllerVerifierOracleGeneratedCorpus,
    DependentAndRealizableCasesAlwaysVerify) {
  std::mt19937 rng(kO4CorpusSeed);
  std::size_t dependent_count = 0;
  std::size_t realizable_count = 0;

  for (std::size_t i = 0; i < kO4CorpusCaseCount; ++i) {
    const VariablePartition partition = RandomPartition(rng);
    const spot::formula phi = GenerateRandomFormula(partition, rng);
    auto dict = spot::make_bdd_dict();

    std::optional<DependentOutputs> result;
    try {
      result = dependent_outputs(phi, partition, dict);
    } catch (const std::invalid_argument&) {
      continue;  // unsatisfiable phi (PRD "Edge cases") -- not this oracle's
                 // target.
    }
    if (result->dependent.empty()) continue;
    ++dependent_count;
    ASSERT_TRUE(result->t_out.has_value());

    std::ostringstream phi_os;
    phi_os << phi;
    SCOPED_TRACE("case " + std::to_string(i) + ": phi=" + phi_os.str());

    const OutputLabeledTransducer t_in =
        trivial_transducer(result->partition, Role::t_in, dict);
    DfaProduct method;
    const std::optional<Controller> controller =
        method.synthesize(phi, result->partition, t_in, *result->t_out);
    if (!controller.has_value()) continue;  // "where synthesis succeeds" (O4).
    ++realizable_count;

    const auto verdict = verify_controller(phi, result->partition, t_in,
                                           *result->t_out, *controller);
    EXPECT_TRUE(verdict.ok)
        << "verify_controller rejected a synthesized T_C for a generated "
           "dependent-output case";
  }

  RecordProperty("dependent_count", static_cast<int>(dependent_count));
  RecordProperty("realizable_count", static_cast<int>(realizable_count));
  // Vacuousness guard (mirrors O1's PRD-required assertion): a non-trivial
  // fraction of the corpus must actually reach the dependent+realizable
  // subset this oracle exercises, else it never runs its core assertion.
  EXPECT_GT(dependent_count, 0u);
  EXPECT_GT(realizable_count, 0u);
  EXPECT_GE(realizable_count, kO4CorpusCaseCount / 100)  // >= 1%, a loose
                                                         // floor not a target.
      << "dependent+realizable cases too rare (" << realizable_count << "/"
      << kO4CorpusCaseCount << ") -- oracle risks being near-vacuous";
}

}  // namespace
