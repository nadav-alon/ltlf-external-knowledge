// O4-in -- controller-verifier oracle (docs/prd/input-dependencies-tool.md
// "Test oracles"): "Where the emitted Tin is non-trivial and synthesis
// succeeds, verify_controller(phi, vars, t_in, trivial_t_out, t_c) must
// accept -- the internal linchpin, independent of the method that produced
// t_c, and the first time the verifier runs against a Tin that was DERIVED
// rather than hand-authored."
//
// Library-level throughout, mirroring
// tests/output_dependencies_controller_test.cpp's O4 structure exactly but
// for the input direction: `dependent_inputs` (Phase 1, already landed)
// returns a fully-formed OutputLabeledTransducer, so this drives
// DfaProduct::synthesize and verify_controller directly and needs neither the
// `ltlf-ek-deps` binary nor `ltlfsynt`. Fully runnable today (unlike the
// Phase-2 CLI oracles in tests/ltlf_ek_deps_input_test.cpp, which bind to the
// not-yet-landed `--direction` flag).

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

#include "ltlf_ek/dependent_inputs.hpp"
#include "ltlf_ek/dfa_product.hpp"
#include "ltlf_ek/output_labeled_transducer.hpp"
#include "ltlf_ek/role.hpp"
#include "ltlf_ek/synthesis.hpp"
#include "ltlf_ek/variables.hpp"
#include "ltlf_ek/verify_controller.hpp"

#include "support/fixtures.hpp"

namespace {

using ltlf_ek::Controller;
using ltlf_ek::dependent_inputs;
using ltlf_ek::DependentInputs;
using ltlf_ek::DfaProduct;
using ltlf_ek::OutputLabeledTransducer;
using ltlf_ek::Role;
using ltlf_ek::trivial_transducer;
using ltlf_ek::VariablePartition;
using ltlf_ek::verify_controller;
using ltlf_ek::test_support::Phi;

// ---------------------------------------------------------------------------
// Hand-picked fixtures. Reuses the exact (phi, partition) shapes already
// hand-verified for `dependent_inputs` (U1-in, U3-in) in
// tests/dependent_inputs_test.cpp -- NOT duplicating those tests, only their
// concrete data, to check a DIFFERENT property here (that the emitted T_in
// survives synthesis + verification) -- plus two shapes pushed past that
// file's two-input axis to a wider partition.
// ---------------------------------------------------------------------------

struct O4InFixture {
  std::string name;
  std::string phi;
  std::set<std::string> inputs;
  std::set<std::string> outputs;
};

void PrintTo(const O4InFixture& f, std::ostream* os) { *os << f.name; }

// NOTE on realizability: U1-in's own phi (F(a^b), I={a,b}, O={x}, x unused)
// is a non-empty-Xdep witness for `dependent_inputs` alone (Phase 1's unit
// test), but it is NOT realizable here -- x never appears in it, so the
// environment can simply play a=b forever and win, and empirically
// `DfaProduct::synthesize` (correctly) agrees it is unrealizable both at
// baseline and with the emitted Tin. O4-in needs REALIZABLE fixtures (the
// oracle only fires "where synthesis succeeds"), so every fixture below is
// built on U3-in's shape instead (F(!a | (b^x))), where the system can always
// win by setting x != b at its very first move -- x genuinely participates,
// unlike U1-in's.
std::vector<O4InFixture> RealizableFixtures() {
  using Names = std::set<std::string>;
  return {
      // U3-in shape verbatim: the exists/forall linchpin (I3) -- Xdep={a},
      // with the output x genuinely appearing in !phi's live region before
      // projection, and realizable via x != b at the first move.
      {"ExistsProjectionLinchpin", "F(!a | (b ^ x))", Names{"a", "b"},
       Names{"x"}},
      // The dual fixture (a, b roles swapped) -- Xdep={b} by symmetry, a
      // genuinely different (phi, Xdep) pair on the same construction.
      {"SwappedVariableExistsProjection", "F(!b | (a ^ x))", Names{"a", "b"},
       Names{"x"}},
      // Pushed past the single-output axis: a second free output y that never
      // appears in phi at all -- the projection still existentially
      // quantifies out both x and y at the live state, and Xdep is unchanged.
      {"TwoOutputsOneUnconstrained", "F(!a | (b ^ x)) & G(y | !y)",
       Names{"a", "b"}, Names{"x", "y"}},
      // Pushed past the two-input axis: a third free input c that never
      // appears in phi at all (via a tautological conjunct) -- c must stay
      // OUT of Xdep (it is unconstrained in Aneg, so every live region admits
      // both values of c for any fixed (a,b), failing the functionality
      // test), exercising that the greedy loop correctly rejects an
      // irrelevant candidate on a wider partition.
      {"ThreeInputsOneUnconstrained", "F(!a | (b ^ x)) & G(c | !c)",
       Names{"a", "b", "c"}, Names{"x"}},
  };
}

class ControllerVerifierOracleIn
    : public ::testing::TestWithParam<O4InFixture> {};

TEST_P(ControllerVerifierOracleIn, SynthesizedControllerVerifiesSafe) {
  const O4InFixture& f = GetParam();
  auto dict = spot::make_bdd_dict();
  const VariablePartition part =
      VariablePartition::split(f.inputs, f.outputs, /*governed=*/{});
  const spot::formula phi = Phi(f.phi);

  const DependentInputs result = dependent_inputs(phi, part, dict);
  ASSERT_FALSE(result.dependent.empty())
      << "fixture is expected to have a non-empty Xdep";
  ASSERT_TRUE(result.t_in.has_value());

  const OutputLabeledTransducer t_out =
      trivial_transducer(result.partition, Role::t_out, dict);
  DfaProduct method;
  const std::optional<Controller> controller =
      method.synthesize(phi, result.partition, *result.t_in, t_out);
  ASSERT_TRUE(controller.has_value())
      << "expected synthesis to succeed against the emitted Tin for phi="
      << f.phi;

  const auto verdict = verify_controller(phi, result.partition, *result.t_in,
                                         t_out, *controller);
  EXPECT_TRUE(verdict.ok)
      << "verify_controller rejected a synthesized T_C against the emitted "
         "Tin for phi="
      << f.phi;
}

INSTANTIATE_TEST_SUITE_P(
    Fixtures, ControllerVerifierOracleIn,
    ::testing::ValuesIn(RealizableFixtures()),
    [](const ::testing::TestParamInfo<O4InFixture>& info) {
      return info.param.name;
    });

// ---------------------------------------------------------------------------
// I6-in companion: the totality requirement fails in the OPPOSITE direction
// from the output tool's I4 (PRD "Behaviour" I6). A partial lambda_in would
// delete the environment's LOSING moves and hand it a strictly stronger
// position, turning a realizable phi into an apparently unrealizable one --
// so the risk here is a false UNREALIZABLE, not a false REALIZABLE. Reuses
// U3-in's phi (realizable, see the fixture-list comment above): its Aneg has
// exactly the "one live state + one dead sink" shape I6 calls the common
// path, so totality at the dead sink (U4-in's own fixture) is genuinely
// exercised here at the synthesis/verification layer rather than only at the
// transducer layer.
// ---------------------------------------------------------------------------

TEST(ControllerVerifierOracleInI6Companion,
    TotalizedTinDoesNotFalselyLoseARealizableFormula) {
  auto dict = spot::make_bdd_dict();
  const VariablePartition part =
      VariablePartition::split(/*inputs=*/{"a", "b"}, /*outputs=*/{"x"},
                               /*governed=*/{});
  const spot::formula phi = Phi("F(!a | (b ^ x))");

  const DependentInputs result = dependent_inputs(phi, part, dict);
  ASSERT_EQ(result.dependent, (std::set<std::string>{"a"}));
  ASSERT_TRUE(result.t_in.has_value());

  const OutputLabeledTransducer t_out =
      trivial_transducer(result.partition, Role::t_out, dict);
  DfaProduct method;
  const std::optional<Controller> controller_with_deps =
      method.synthesize(phi, result.partition, *result.t_in, t_out);

  // Cross-check against the no-external-knowledge baseline: a wrongly-partial
  // Tin would report this UNREALIZABLE against the emitted (necessarily
  // total) transducer even though the baseline (and ExistsProjectionLinchpin
  // above) agree it is realizable.
  const VariablePartition baseline_part =
      VariablePartition::split(/*inputs=*/{"a", "b"}, /*outputs=*/{"x"},
                               /*governed=*/{});
  const OutputLabeledTransducer t_in_baseline =
      trivial_transducer(baseline_part, Role::t_in, dict);
  const OutputLabeledTransducer t_out_baseline =
      trivial_transducer(baseline_part, Role::t_out, dict);
  const std::optional<Controller> controller_baseline =
      method.synthesize(phi, baseline_part, t_in_baseline, t_out_baseline);

  ASSERT_TRUE(controller_baseline.has_value())
      << "sanity: F(!a | (b^x)) must be realizable with no external "
         "knowledge at all";
  EXPECT_TRUE(controller_with_deps.has_value())
      << "I6: a correctly totalized Tin must not falsely flip a realizable "
         "phi to unrealizable";
  if (controller_with_deps.has_value()) {
    const auto verdict = verify_controller(phi, result.partition,
                                           *result.t_in, t_out,
                                           *controller_with_deps);
    EXPECT_TRUE(verdict.ok);
  }
}

// ---------------------------------------------------------------------------
// Generated corpus (library-level only -- no ltlfsynt, no ltlf-ek-deps
// binary): same fixed-seed technique as
// tests/output_dependencies_controller_test.cpp's
// ControllerVerifierOracleGeneratedCorpus (duplicated per this project's
// one-file-per-suite convention), restricted post hoc to cases where
// `dependent_inputs` finds a non-empty Xdep AND DfaProduct::synthesize
// succeeds against the emitted Tin. On every such case, verify_controller
// must accept.
// ---------------------------------------------------------------------------

constexpr unsigned kO4InCorpusSeed = 20260803;
constexpr std::size_t kO4InCorpusCaseCount = 6000;
constexpr int kO4InTreeSizeMin = 1;
constexpr int kO4InTreeSizeMax = 8;
constexpr int kO4InInputMax = 3;
constexpr int kO4InOutputMax = 3;

spot::formula GenerateRandomFormula(const VariablePartition& partition,
                                    std::mt19937& rng) {
  std::set<std::string> ap_names = partition.inputs();
  for (const std::string& name : partition.outputs()) ap_names.insert(name);

  spot::atomic_prop_set aprops;
  for (const std::string& name : ap_names)
    aprops.insert(spot::default_environment::instance().require(name));

  spot::option_map opts;
  opts.set("output", spot::randltlgenerator::LTL);
  opts.set("tree_size_min", kO4InTreeSizeMin);
  opts.set("tree_size_max", kO4InTreeSizeMax);
  opts.set("seed", static_cast<int>(rng()));

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
// requires an empty input_known on input; this oracle needs only the
// analysis, not a pre-existing V).
VariablePartition RandomPartition(std::mt19937& rng) {
  std::uniform_int_distribution<int> input_count(1, kO4InInputMax);
  std::uniform_int_distribution<int> output_count(1, kO4InOutputMax);
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

TEST(ControllerVerifierOracleInGeneratedCorpus,
    DependentAndRealizableCasesAlwaysVerify) {
  std::mt19937 rng(kO4InCorpusSeed);
  std::size_t dependent_count = 0;
  std::size_t realizable_count = 0;

  for (std::size_t i = 0; i < kO4InCorpusCaseCount; ++i) {
    const VariablePartition partition = RandomPartition(rng);
    const spot::formula phi = GenerateRandomFormula(partition, rng);
    auto dict = spot::make_bdd_dict();

    std::optional<DependentInputs> result;
    try {
      result = dependent_inputs(phi, partition, dict);
    } catch (const std::invalid_argument&) {
      continue;  // phi valid (PRD "Edge cases", I11) -- not this oracle's
                 // target.
    }
    if (result->dependent.empty()) continue;
    ++dependent_count;
    ASSERT_TRUE(result->t_in.has_value());

    std::ostringstream phi_os;
    phi_os << phi;
    SCOPED_TRACE("case " + std::to_string(i) + ": phi=" + phi_os.str());

    const OutputLabeledTransducer t_out =
        trivial_transducer(result->partition, Role::t_out, dict);
    DfaProduct method;
    const std::optional<Controller> controller =
        method.synthesize(phi, result->partition, *result->t_in, t_out);
    if (!controller.has_value()) continue;  // "where synthesis succeeds" (O4-in).
    ++realizable_count;

    const auto verdict = verify_controller(phi, result->partition,
                                           *result->t_in, t_out, *controller);
    EXPECT_TRUE(verdict.ok)
        << "verify_controller rejected a synthesized T_C for a generated "
           "dependent-input case";
  }

  RecordProperty("dependent_count", static_cast<int>(dependent_count));
  RecordProperty("realizable_count", static_cast<int>(realizable_count));
  // Vacuousness guard (mirrors O1-in's PRD-required assertion): a non-trivial
  // fraction of the corpus must actually reach the dependent+realizable
  // subset this oracle exercises, else it never runs its core assertion.
  EXPECT_GT(dependent_count, 0u);
  EXPECT_GT(realizable_count, 0u);
  // The measured dependent+realizable rate on this generator/seed is ~0.55%
  // (33/6000) -- lower than the output tool's own 1% floor
  // (tests/output_dependencies_controller_test.cpp), because a random
  // formula's inputs are more often "free enough" to escape being pinned by
  // Aneg than its outputs are to be pinned by A_phi. >=0.3% is a loose floor
  // matching the measured rate with headroom, not a target.
  EXPECT_GE(realizable_count, kO4InCorpusCaseCount * 3 / 1000)
      << "dependent+realizable cases too rare (" << realizable_count << "/"
      << kO4InCorpusCaseCount << ") -- oracle risks being near-vacuous";
}

}  // namespace
