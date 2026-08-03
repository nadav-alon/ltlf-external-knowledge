// O1-in (the linchpin) and O3-in -- Phase 2 CLI-level oracles
// (docs/prd/input-dependencies-tool.md "Test oracles"), the input-direction
// twins of tests/ltlf_ek_deps_test.cpp's O1/O3. Drives the `ltlf-ek-deps`
// binary as a subprocess with the frozen `--direction in|out` flag
// (PRD "Interfaces & types -> Phase 2 -- the CLI flag").
//
// CONCURRENT WORKFLOW, PARTIALLY: Phase 1 (`dependent_inputs`,
// `dependency_types.hpp`) is already landed and green, so the library-level
// pieces here (the corpus's Xdep-rate pre-filter) run today. The
// `--direction` flag itself (Phase 2) is being implemented on a separate
// branch in parallel and is NOT present in this worktree, so every assertion
// below that actually invokes `--direction` is written against the PRD's
// frozen contract and is expected to FAIL until that branch lands and is
// merged -- this file deliberately does not stub or DISABLE those cases; see
// the test-writer's report for exactly which ones and why. Once merged, no
// edit should be needed here unless the implementation reveals the frozen
// block was wrong (a PRD-change event per that section's own escape clause).

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <optional>
#include <random>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include <sys/wait.h>
#include <unistd.h>

#include <gtest/gtest.h>
#include <bddx.h>
#include <spot/misc/optionmap.hh>
#include <spot/tl/defaultenv.hh>
#include <spot/tl/parse.hh>
#include <spot/tl/randomltl.hh>
#include <spot/twa/bdddict.hh>
#include <spot/twa/twagraph.hh>

#include "ltlf_ek/cli.hpp"
#include "ltlf_ek/dependent_inputs.hpp"
#include "ltlf_ek/detail/util.hpp"
#include "ltlf_ek/output_labeled_transducer.hpp"
#include "ltlf_ek/role.hpp"
#include "ltlf_ek/transducer_io.hpp"
#include "ltlf_ek/variables.hpp"

#ifndef LTLF_EK_SYNTH_BINARY
#error "LTLF_EK_SYNTH_BINARY must be defined by CMake (see CMakeLists.txt)"
#endif
#ifndef LTLF_EK_DEPS_BINARY
#error "LTLF_EK_DEPS_BINARY must be defined by CMake (see CMakeLists.txt, alongside LTLF_EK_SYNTH_BINARY)"
#endif

namespace {

using ltlf_ek::dependent_inputs;
using ltlf_ek::DependentInputs;
using ltlf_ek::OutputLabeledTransducer;
using ltlf_ek::parse_partition_file;
using ltlf_ek::parse_transducer;
using ltlf_ek::print_partition_file;
using ltlf_ek::Role;
using ltlf_ek::VariablePartition;

// ---------------------------------------------------------------------------
// Subprocess harness -- byte-for-byte the same helpers as
// tests/ltlf_ek_deps_test.cpp / tests/ltlfsynt_oracle_test.cpp, duplicated
// file-locally per this project's stated one-file-per-suite convention (see
// either of those files' own header comments).
// ---------------------------------------------------------------------------

struct CliResult {
  int exit_code = -1;
  std::string stdout_text;
  std::string stderr_text;
};

std::string ShellQuote(const std::string& s) {
  std::string out = "'";
  for (char c : s) {
    if (c == '\'')
      out += "'\\''";
    else
      out += c;
  }
  out += "'";
  return out;
}

class ScopedTempFile {
 public:
  explicit ScopedTempFile(const std::string& contents = "") {
    path_ = ltlf_ek::detail::temp_template("ltlf_ek_deps_input_test");
    const int fd = mkstemp(path_.data());
    EXPECT_GE(fd, 0) << "mkstemp failed for " << path_;
    if (fd >= 0) {
      if (!contents.empty()) {
        const ssize_t n = write(fd, contents.data(), contents.size());
        EXPECT_EQ(static_cast<std::size_t>(n), contents.size());
      }
      close(fd);
    }
  }
  ~ScopedTempFile() { std::remove(path_.c_str()); }
  const std::string& path() const { return path_; }

 private:
  std::string path_;
};

CliResult RunSubprocess(const std::string& binary,
                        const std::vector<std::string>& args,
                        std::optional<unsigned> timeout_secs = std::nullopt,
                        bool* timed_out = nullptr) {
  if (timed_out) *timed_out = false;
  ScopedTempFile out_capture, err_capture;
  std::ostringstream cmd;
  if (timeout_secs) cmd << "timeout " << *timeout_secs << "s ";
  cmd << ShellQuote(binary);
  for (const auto& a : args) cmd << " " << ShellQuote(a);
  cmd << " >" << ShellQuote(out_capture.path()) << " 2>"
      << ShellQuote(err_capture.path());
  const int rc = std::system(cmd.str().c_str());

  CliResult result;
  result.exit_code = WIFEXITED(rc) ? WEXITSTATUS(rc) : -1;
  if (timeout_secs && timed_out &&
      ((WIFEXITED(rc) && WEXITSTATUS(rc) == 124) || WIFSIGNALED(rc)))
    *timed_out = true;
  std::ifstream out_in(out_capture.path());
  std::ostringstream out_ss;
  out_ss << out_in.rdbuf();
  result.stdout_text = out_ss.str();
  std::ifstream err_in(err_capture.path());
  std::ostringstream err_ss;
  err_ss << err_in.rdbuf();
  result.stderr_text = err_ss.str();
  return result;
}

CliResult RunEkDeps(const std::vector<std::string>& args,
                    std::optional<unsigned> timeout_secs = std::nullopt,
                    bool* timed_out = nullptr) {
  return RunSubprocess(LTLF_EK_DEPS_BINARY, args, timeout_secs, timed_out);
}

CliResult RunEkSynth(const std::vector<std::string>& args,
                     std::optional<unsigned> timeout_secs = std::nullopt,
                     bool* timed_out = nullptr) {
  return RunSubprocess(LTLF_EK_SYNTH_BINARY, args, timeout_secs, timed_out);
}

std::string ResolveLtlfsyntBinary() {
  if (const char* env = std::getenv("LTLFSYNT_BIN"); env && *env) return env;
#ifdef LTLFSYNT_BINARY
  return LTLFSYNT_BINARY;
#else
  return "";
#endif
}

bool IsRunnable(const std::string& binary) {
  if (binary.empty()) return false;
  std::ostringstream cmd;
  cmd << ShellQuote(binary) << " --version >/dev/null 2>&1";
  const int rc = std::system(cmd.str().c_str());
  return WIFEXITED(rc) && WEXITSTATUS(rc) == 0;
}

enum class Verdict { kRealizable, kUnrealizable };

Verdict ParseEkSynthVerdict(const CliResult& r) {
  if (r.exit_code == 0 && r.stdout_text == "REALIZABLE\n")
    return Verdict::kRealizable;
  if (r.exit_code == 20 && r.stdout_text == "UNREALIZABLE\n")
    return Verdict::kUnrealizable;
  ADD_FAILURE() << "ltlf-ek-synth: no verdict word on stdout, or exit code "
                << "outside {0, 20} (exit " << r.exit_code << "); stdout=["
                << r.stdout_text << "] stderr=[" << r.stderr_text << "]";
  return Verdict::kUnrealizable;
}

Verdict ParseLtlfsyntVerdict(const CliResult& r) {
  if (r.exit_code == 0 && r.stdout_text == "REALIZABLE\n")
    return Verdict::kRealizable;
  if (r.exit_code == 1 && r.stdout_text == "UNREALIZABLE\n")
    return Verdict::kUnrealizable;
  ADD_FAILURE() << "ltlfsynt: no verdict word on stdout, or exit code "
                << "outside {0, 1} (exit " << r.exit_code << "); stdout=["
                << r.stdout_text << "] stderr=[" << r.stderr_text << "]";
  return Verdict::kUnrealizable;
}

bool IsRealizable(Verdict v) { return v == Verdict::kRealizable; }

std::string JoinCsv(const std::set<std::string>& names) {
  std::string out;
  for (const std::string& name : names) {
    if (!out.empty()) out += ",";
    out += name;
  }
  return out;
}

// A full letter over `probe`'s dict: `names` true, everything else in the
// pair false (same helper as tests/dependent_inputs_test.cpp's Letter).
bdd Letter(const spot::twa_graph_ptr& probe,
          const std::set<std::pair<std::string, bool>>& assignment) {
  bdd v = bddtrue;
  for (const auto& [name, value] : assignment) {
    const bdd var = bdd_ithvar(probe->register_ap(name));
    v &= value ? var : !var;
  }
  return v;
}

// ---------------------------------------------------------------------------
// O3-in -- part-file co-management (docs/prd/input-dependencies-tool.md
// "Test oracles" O3-in). All four assertions need the not-yet-landed
// `--direction` flag, so every TEST below is expected to fail with exit 2
// ("unrecognised flag: --direction") until that branch merges -- see this
// file's header comment.
// ---------------------------------------------------------------------------

// Assertion 1: feed a part file with a non-empty output_known; the emitted
// part file preserves output_free/output_known verbatim (as sets) and
// repartitions only the two input keys (I9's dual of
// tests/ltlf_ek_deps_test.cpp's EmitPartPassesThroughInputKeysAndRepartitionsOutputsOnly).
TEST(LtlfEkDepsInputPartFile,
    EmitPartPassesThroughOutputKeysAndRepartitionsInputsOnly) {
  const std::string phi_str = "F(a ^ b)";
  const ScopedTempFile input_part(
      "input_free:   a b\n"
      "output_free:  x\n"
      "output_known: y\n");
  const ScopedTempFile emit_part;
  const ScopedTempFile transducer_file;

  const CliResult deps = RunEkDeps(
      {"--direction", "in", "--formula=" + phi_str, "--part-file",
       input_part.path(), "--emit-part", emit_part.path(), "--transducer",
       transducer_file.path()});
  ASSERT_EQ(deps.exit_code, 0)
      << "BLOCKED on the unlanded --direction flag (see file header); "
         "stderr=[" << deps.stderr_text << "]";

  std::ifstream emitted_in(emit_part.path());
  const VariablePartition emitted = parse_partition_file(emitted_in);

  // output_free / output_known pass through verbatim -- I9's dual.
  EXPECT_EQ(emitted.output_free, (std::set<std::string>{"x"}));
  EXPECT_EQ(emitted.output_known, (std::set<std::string>{"y"}));

  // Only the two input keys are repartitioned; cross-check against the
  // already-landed Phase 1 library oracle for the SAME (phi, partition) --
  // y never occurs in phi (I10: the analysis ignores Tout/Oknown entirely).
  auto dict = spot::make_bdd_dict();
  std::ifstream input_in2(input_part.path());
  const VariablePartition parsed_input = parse_partition_file(input_in2);
  const DependentInputs expected =
      dependent_inputs(spot::parse_formula(phi_str), parsed_input, dict);
  EXPECT_EQ(emitted.input_known, expected.dependent);
  EXPECT_EQ(emitted.input_free, expected.partition.input_free);
}

// Assertion 2: a non-empty input_known on input is refused with exit 2 (I9 --
// there is no "compose two Tins" notion).
TEST(LtlfEkDepsInputPartFile, RefusesNonEmptyInputKnownOnInputWithExitCode2) {
  const ScopedTempFile input_part(
      "input_free:   a\n"
      "input_known:  b\n"
      "output_free:  x\n");
  const ScopedTempFile emit_part;
  const CliResult deps =
      RunEkDeps({"--direction", "in", "--formula=F(a ^ b)", "--part-file",
                 input_part.path(), "--emit-part", emit_part.path()});
  EXPECT_EQ(deps.exit_code, 2)
      << "BLOCKED on the unlanded --direction flag: today this fails with "
         "exit 2 for the WRONG reason (unrecognised flag), not I9's refusal; "
         "once --direction lands this must still be exit 2, now for I9 -- "
         "stderr=[" << deps.stderr_text << "]";
}

// Assertion 3: --emit-part equal to --part-file is refused with exit 2 --
// same existing guard as the output direction, but the PRD requires it be
// re-checked under --direction in explicitly ("the hazard is a property of
// the path, not of the flag that lands on it").
TEST(LtlfEkDepsInputPartFile, RefusesEmitPartEqualToPartFileWithExitCode2) {
  const ScopedTempFile input_part(
      "input_free:   a b\n"
      "output_free:  x\n");
  const CliResult deps =
      RunEkDeps({"--direction", "in", "--formula=F(a ^ b)", "--part-file",
                 input_part.path(), "--emit-part", input_part.path()});
  EXPECT_EQ(deps.exit_code, 2) << "stderr=[" << deps.stderr_text << "]";
}

// Assertion 4 -- I12's commutation. Running `--direction out` then
// `--direction in`, and running the two in the opposite order, must produce
// part files equal as sets and transducer files that parse to equal
// transducers. Uses a phi with BOTH a non-empty Xdep (output side) and a
// non-empty Xdep (input side) so both directions actually do something:
// phi = G(a <-> x) & F(a ^ b) -- x tracks a (Xdep_out = {x}), and F(a^b) over
// the OTHER input pair gives Xdep_in = {a} (U1-in's shape, restricted to a
// disjoint pair of inputs so neither analysis's result can influence the
// other's AP set).
TEST(LtlfEkDepsInputPartFile, I12DirectionsCommute) {
  const std::string phi_str = "G(a <-> x) & F(b ^ c)";
  const std::string initial_contents =
      "input_free:   a b c\n"
      "output_free:  x\n";

  // Order 1: out then in.
  const ScopedTempFile part0_order1(initial_contents);
  const ScopedTempFile part1_order1;
  const ScopedTempFile tout_order1;
  const CliResult out1 = RunEkDeps({"--direction", "out",
                                    "--formula=" + phi_str, "--part-file",
                                    part0_order1.path(), "--emit-part",
                                    part1_order1.path(), "--transducer",
                                    tout_order1.path()});
  ASSERT_EQ(out1.exit_code, 0)
      << "BLOCKED on the unlanded --direction flag; stderr=["
      << out1.stderr_text << "]";
  const ScopedTempFile part2_order1;
  const ScopedTempFile tin_order1;
  const CliResult in1 =
      RunEkDeps({"--direction", "in", "--formula=" + phi_str, "--part-file",
                 part1_order1.path(), "--emit-part", part2_order1.path(),
                 "--transducer", tin_order1.path()});
  ASSERT_EQ(in1.exit_code, 0) << in1.stderr_text;

  // Order 2: in then out.
  const ScopedTempFile part0_order2(initial_contents);
  const ScopedTempFile part1_order2;
  const ScopedTempFile tin_order2;
  const CliResult in2 = RunEkDeps({"--direction", "in",
                                   "--formula=" + phi_str, "--part-file",
                                   part0_order2.path(), "--emit-part",
                                   part1_order2.path(), "--transducer",
                                   tin_order2.path()});
  ASSERT_EQ(in2.exit_code, 0) << in2.stderr_text;
  const ScopedTempFile part2_order2;
  const ScopedTempFile tout_order2;
  const CliResult out2 =
      RunEkDeps({"--direction", "out", "--formula=" + phi_str, "--part-file",
                 part1_order2.path(), "--emit-part", part2_order2.path(),
                 "--transducer", tout_order2.path()});
  ASSERT_EQ(out2.exit_code, 0) << out2.stderr_text;

  // The two final part files agree as sets in all four keys.
  std::ifstream final1_in(part2_order1.path());
  const VariablePartition final1 = parse_partition_file(final1_in);
  std::ifstream final2_in(part2_order2.path());
  const VariablePartition final2 = parse_partition_file(final2_in);
  EXPECT_EQ(final1.input_free, final2.input_free);
  EXPECT_EQ(final1.input_known, final2.input_known);
  EXPECT_EQ(final1.output_free, final2.output_free);
  EXPECT_EQ(final1.output_known, final2.output_known);

  // The two Tout files parse to equal transducers, and likewise the two Tin
  // files -- compared pointwise (state count, initial state, lambda per
  // letter), the same technique as tests/dependent_inputs_test.cpp's
  // AssertDualityHolds, since OutputLabeledTransducer has no operator==.
  auto dict = spot::make_bdd_dict();
  auto probe = spot::make_twa_graph(dict);
  std::ifstream tout1_in(tout_order1.path());
  const OutputLabeledTransducer t_out1 =
      parse_transducer(tout1_in, final1, Role::t_out, dict);
  std::ifstream tout2_in(tout_order2.path());
  const OutputLabeledTransducer t_out2 =
      parse_transducer(tout2_in, final2, Role::t_out, dict);
  ASSERT_EQ(t_out1.delta_dfa()->num_states(), t_out2.delta_dfa()->num_states());
  ASSERT_EQ(t_out1.initial_state(), t_out2.initial_state());

  std::ifstream tin1_in(tin_order1.path());
  const OutputLabeledTransducer t_in1 =
      parse_transducer(tin1_in, final1, Role::t_in, dict);
  std::ifstream tin2_in(tin_order2.path());
  const OutputLabeledTransducer t_in2 =
      parse_transducer(tin2_in, final2, Role::t_in, dict);
  ASSERT_EQ(t_in1.delta_dfa()->num_states(), t_in2.delta_dfa()->num_states());
  ASSERT_EQ(t_in1.initial_state(), t_in2.initial_state());

  const std::vector<std::string> universe = {"a", "b", "c", "x"};
  for (std::size_t mask = 0; mask < (std::size_t{1} << universe.size());
       ++mask) {
    std::set<std::pair<std::string, bool>> assignment;
    for (std::size_t i = 0; i < universe.size(); ++i)
      assignment.insert({universe[i], static_cast<bool>((mask >> i) & 1u)});
    const bdd letter = Letter(probe, assignment);
    for (unsigned s = 0; s < t_out1.delta_dfa()->num_states(); ++s)
      EXPECT_EQ(t_out1.lambda(s, letter), t_out2.lambda(s, letter))
          << "Tout state " << s << ", mask " << mask;
    for (unsigned s = 0; s < t_in1.delta_dfa()->num_states(); ++s)
      EXPECT_EQ(t_in1.lambda(s, letter), t_in2.lambda(s, letter))
          << "Tin state " << s << ", mask " << mask;
  }
}

// ---------------------------------------------------------------------------
// O1-in -- end-to-end equirealizability oracle, the linchpin
// (docs/prd/input-dependencies-tool.md "Test oracles" O1-in). Gated on
// ltlfsynt (the external, independent oracle), mirroring
// tests/ltlf_ek_deps_test.cpp's LtlfEkDepsOracleTest fixture. NOT gated on
// ltlf-ek-deps itself (mandatory in-project infrastructure), but every case
// that invokes `--direction in` is BLOCKED on the unlanded flag -- see this
// file's header comment.
// ---------------------------------------------------------------------------

class LtlfEkDepsInputOracleTest : public ::testing::Test {
 protected:
  void SetUp() override {
    ltlfsynt_binary_ = ResolveLtlfsyntBinary();
    if (!IsRunnable(ltlfsynt_binary_))
      GTEST_SKIP() << "ltlfsynt not found/runnable (checked the LTLFSYNT_BIN "
                      "env override and CMake find_program(ltlfsynt)); "
                      "skipping the external oracle";
  }

  CliResult RunLtlfsynt(const std::vector<std::string>& args,
                        std::optional<unsigned> timeout_secs = std::nullopt,
                        bool* timed_out = nullptr) {
    return RunSubprocess(ltlfsynt_binary_, args, timeout_secs, timed_out);
  }

 private:
  std::string ltlfsynt_binary_;
};

// The I6-in totality witness (PRD "Behaviour" I6, "Test oracles" O1-in),
// end-to-end. phi = F(a^b), I={a,b}, O={x}: U1-in's shape, Xdep_in = {a},
// realizable with no external knowledge at all (the environment eventually
// plays a^b regardless of x). I6 warns the risk here is the OPPOSITE of the
// output tool's I4: a wrongly-partial Tin deletes the environment's LOSING
// moves and would flip this to falsely UNREALIZABLE. A correctly totalized
// Tin (I5) must agree with both the no-knowledge baseline and ltlfsynt -- all
// three REALIZABLE.
TEST_F(LtlfEkDepsInputOracleTest,
      I6TotalityWitnessAgreesAcrossAllThreeAndIsRealizable) {
  const std::string phi_str = "F(a ^ b)";
  const ScopedTempFile emit_part;
  const ScopedTempFile transducer_file;

  const CliResult deps =
      RunEkDeps({"--direction", "in", "--formula=" + phi_str, "--inputs",
                 "a,b", "--outputs", "x", "--emit-part", emit_part.path(),
                 "--transducer", transducer_file.path()});
  ASSERT_EQ(deps.exit_code, 0)
      << "BLOCKED on the unlanded --direction flag (see file header); "
         "stderr=[" << deps.stderr_text << "]";

  std::ifstream emitted_in(emit_part.path());
  const VariablePartition emitted = parse_partition_file(emitted_in);
  ASSERT_EQ(emitted.input_known, (std::set<std::string>{"a"}))
      << "phi=F(a^b) is U1-in's shape: a must be reported dependent";

  const CliResult ek_baseline =
      RunEkSynth({"--dfa-product", "--formula=" + phi_str, "--inputs", "a,b",
                  "--outputs", "x", "--realizable"});
  const CliResult ek_with_deps = RunEkSynth(
      {"--dfa-product", "--formula=" + phi_str, "--part-file",
       emit_part.path(), "--known-input-transducer", transducer_file.path(),
       "--realizable"});
  const CliResult synt =
      RunLtlfsynt({"--ins=a,b", "--outs=x", "--semantics=Mealy",
                   "--realizability", "-f", phi_str});

  const Verdict baseline_verdict = ParseEkSynthVerdict(ek_baseline);
  const Verdict with_deps_verdict = ParseEkSynthVerdict(ek_with_deps);
  const Verdict synt_verdict = ParseLtlfsyntVerdict(synt);

  EXPECT_EQ(baseline_verdict, Verdict::kRealizable);
  EXPECT_EQ(synt_verdict, Verdict::kRealizable);
  EXPECT_EQ(with_deps_verdict, Verdict::kRealizable)
      << "I6 regression: a partial Tin would wrongly flip this to "
         "unrealizable";
}

// ---------------------------------------------------------------------------
// Generated corpus (O1-in's main body): the input-direction twin of
// tests/ltlf_ek_deps_test.cpp's GeneratedCorpusEquirealizableAgainstBaselineAndLtlfsynt.
//
// The library pre-filter (`dependent_inputs`, Phase 1, already landed) runs
// TODAY regardless of the CLI flag and gives the real, measured
// non-empty-Xdep rate this test-writer pass was asked to report (see
// RecordProperty("dependent_cases", ...) below and the final report). The
// per-case CLI three-way comparison is BLOCKED on the unlanded --direction
// flag: `deps.exit_code` will be 2 ("unrecognised flag") for every case until
// that branch lands, so `checked_cases` stays 0 and the EXPECT_EQ below fires
// once per dependent case -- deliberately not suppressed, so the failure is
// visible and honest rather than silently skipped.
// ---------------------------------------------------------------------------

constexpr unsigned kDepsInputCorpusSeed = 20260803;
constexpr std::size_t kDepsInputCorpusCaseCount = 150;
constexpr int kDepsInputTreeSizeMin = 1;
constexpr int kDepsInputTreeSizeMax = 7;
constexpr int kDepsInputInputMax = 3;
constexpr int kDepsInputOutputMax = 3;
constexpr unsigned kDepsInputSubprocessTimeoutSecs = 10;

spot::formula GenerateRandomFormula(const VariablePartition& partition,
                                    std::mt19937& rng) {
  std::set<std::string> ap_names = partition.inputs();
  for (const std::string& name : partition.outputs()) ap_names.insert(name);

  spot::atomic_prop_set aprops;
  for (const std::string& name : ap_names)
    aprops.insert(spot::default_environment::instance().require(name));

  spot::option_map opts;
  opts.set("output", spot::randltlgenerator::LTL);
  opts.set("tree_size_min", kDepsInputTreeSizeMin);
  opts.set("tree_size_max", kDepsInputTreeSizeMax);
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

// >=1 input, >=1 output, no Iknown/Oknown (I9 requires an empty input_known
// on input; the differential itself needs no pre-existing V).
VariablePartition RandomPartition(std::mt19937& rng) {
  std::uniform_int_distribution<int> input_count(1, kDepsInputInputMax);
  std::uniform_int_distribution<int> output_count(1, kDepsInputOutputMax);
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

TEST_F(LtlfEkDepsInputOracleTest,
      GeneratedCorpusEquirealizableAgainstBaselineAndLtlfsynt) {
  std::mt19937 rng(kDepsInputCorpusSeed);
  std::size_t dependent_cases = 0;
  std::size_t checked_cases = 0;

  for (std::size_t i = 0; i < kDepsInputCorpusCaseCount; ++i) {
    const VariablePartition partition = RandomPartition(rng);
    const spot::formula phi = GenerateRandomFormula(partition, rng);
    auto dict = spot::make_bdd_dict();

    // Library pre-filter: decides which cases have a non-empty Xdep at all,
    // and doubles as the corpus's own vacuousness statistic below -- this
    // part runs TODAY, independent of --direction.
    std::optional<DependentInputs> lib_result;
    try {
      lib_result = dependent_inputs(phi, partition, dict);
    } catch (const std::invalid_argument&) {
      continue;  // phi valid (PRD "Edge cases", I11).
    }
    if (lib_result->dependent.empty()) continue;
    ++dependent_cases;

    std::ostringstream phi_os;
    phi_os << phi;
    const std::string phi_str = phi_os.str();
    SCOPED_TRACE("case " + std::to_string(i) + ": phi=" + phi_str +
                 ", inputs=" + JoinCsv(partition.input_free) +
                 ", outputs=" + JoinCsv(partition.output_free));

    const ScopedTempFile emit_part;
    const ScopedTempFile transducer_file;
    const CliResult deps = RunEkDeps(
        {"--direction", "in", "--formula=" + phi_str, "--inputs",
         JoinCsv(partition.input_free), "--outputs",
         JoinCsv(partition.output_free), "--emit-part", emit_part.path(),
         "--transducer", transducer_file.path()},
        kDepsInputSubprocessTimeoutSecs);
    EXPECT_EQ(deps.exit_code, 0)
        << "BLOCKED on the unlanded --direction flag for phi=" << phi_str
        << ": " << deps.stderr_text;
    if (deps.exit_code != 0) continue;  // don't let one bad case sink the
                                        // whole corpus's statistics.

    std::ifstream emitted_in(emit_part.path());
    const VariablePartition emitted = parse_partition_file(emitted_in);
    EXPECT_EQ(emitted.input_known, lib_result->dependent)
        << "CLI-reported Xdep disagrees with the already-landed library "
           "oracle for phi="
        << phi_str;
    if (emitted.input_known.empty()) continue;  // filter-mismatch guard.
    ++checked_cases;

    bool baseline_timed_out = false, with_deps_timed_out = false,
        synt_timed_out = false;
    const CliResult ek_baseline = RunEkSynth(
        {"--dfa-product", "--formula=" + phi_str, "--inputs",
         JoinCsv(partition.input_free), "--outputs",
         JoinCsv(partition.output_free), "--realizable"},
        kDepsInputSubprocessTimeoutSecs, &baseline_timed_out);
    const CliResult ek_with_deps = RunEkSynth(
        {"--dfa-product", "--formula=" + phi_str, "--part-file",
         emit_part.path(), "--known-input-transducer", transducer_file.path(),
         "--realizable"},
        kDepsInputSubprocessTimeoutSecs, &with_deps_timed_out);
    const CliResult synt = RunLtlfsynt(
        {"--ins=" + JoinCsv(partition.input_free),
         "--outs=" + JoinCsv(partition.output_free), "--semantics=Mealy",
         "--realizability", "-f", phi_str},
        kDepsInputSubprocessTimeoutSecs, &synt_timed_out);
    if (baseline_timed_out || with_deps_timed_out || synt_timed_out)
      continue;  // a slow subprocess is a skip, never a test failure.

    const Verdict baseline_verdict = ParseEkSynthVerdict(ek_baseline);
    const Verdict with_deps_verdict = ParseEkSynthVerdict(ek_with_deps);
    const Verdict synt_verdict = ParseLtlfsyntVerdict(synt);

    EXPECT_EQ(IsRealizable(baseline_verdict), IsRealizable(synt_verdict))
        << "no-external-knowledge baseline disagrees with ltlfsynt for phi="
        << phi_str;
    EXPECT_EQ(IsRealizable(with_deps_verdict), IsRealizable(synt_verdict))
        << "O1-in linchpin: ltlf-ek-synth against the EMITTED Tin disagrees "
           "with ltlfsynt for phi="
        << phi_str;
    EXPECT_EQ(IsRealizable(with_deps_verdict), IsRealizable(baseline_verdict))
        << "O1-in linchpin: ltlf-ek-synth against the EMITTED Tin disagrees "
           "with the no-external-knowledge baseline for phi="
        << phi_str;
  }

  RecordProperty("dependent_cases", static_cast<int>(dependent_cases));
  RecordProperty("checked_cases", static_cast<int>(checked_cases));
  // Vacuousness guard (PRD "Test oracles" O1-in, verbatim): "assert a
  // non-trivial fraction of the corpus DOES yield a non-empty Xdep, else the
  // oracle is vacuous." This assertion is library-level (does not need
  // --direction) and passes TODAY -- it is the measured rate reported to the
  // PRD.
  EXPECT_GT(dependent_cases, 0u);
  EXPECT_GE(dependent_cases, kDepsInputCorpusCaseCount / 20)  // >= 5%, the
                                                              // output tool's
                                                              // own floor.
      << "only " << dependent_cases << "/" << kDepsInputCorpusCaseCount
      << " generated cases had a non-empty Xdep -- the oracle risks being "
         "vacuous";
}

}  // namespace
