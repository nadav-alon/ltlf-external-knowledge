#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include <sys/wait.h>
#include <unistd.h>

#include <gtest/gtest.h>
#include <spot/parseaut/public.hh>
#include <spot/tl/parse.hh>
#include <spot/twa/bdddict.hh>

#include "ltlf_ek/cli.hpp"
#include "ltlf_ek/dfa_product.hpp"
#include "ltlf_ek/output_labeled_transducer.hpp"
#include "ltlf_ek/synthesis.hpp"
#include "ltlf_ek/variables.hpp"

#ifndef LTLF_EK_SYNTH_BINARY
#error "LTLF_EK_SYNTH_BINARY must be defined by CMake (see CMakeLists.txt)"
#endif

// End-to-end / subprocess oracles for the `ltlf-ek-synth` executable
// (docs/prd/cli-wrapper.md "Test oracles", "Edge cases").  main() is a thin
// orchestration layer not linked into `unit_tests` (per the PRD's "Developer
// comments"), so it is exercised here as a subprocess.  The library-level
// pieces it wires together (parse_partition_file, trivial_transducer,
// make_synthesis_method) have direct unit fixtures in tests/cli_test.cpp.
namespace {

using ltlf_ek::DfaProduct;
using ltlf_ek::OutputLabeledTransducer;
using ltlf_ek::Role;
using ltlf_ek::trivial_transducer;
using ltlf_ek::VariablePartition;

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

// A self-deleting temp file, used both to capture a subprocess's stdout/
// stderr and to hold fixture input (part-files, transducer files).
class ScopedTempFile {
 public:
  explicit ScopedTempFile(const std::string& contents = "") {
    path_ = "/tmp/ltlf_ek_synth_test_XXXXXX";
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

// Runs the built ltlf-ek-synth binary with `args` via a POSIX shell (so each
// stream can be redirected to its own temp file), returning its exit code,
// stdout, and stderr.
CliResult RunCli(const std::vector<std::string>& args) {
  ScopedTempFile out_capture, err_capture;
  std::ostringstream cmd;
  cmd << ShellQuote(LTLF_EK_SYNTH_BINARY);
  for (const auto& a : args) cmd << " " << ShellQuote(a);
  cmd << " >" << ShellQuote(out_capture.path()) << " 2>"
      << ShellQuote(err_capture.path());
  const int rc = std::system(cmd.str().c_str());

  CliResult result;
  result.exit_code = WIFEXITED(rc) ? WEXITSTATUS(rc) : -1;
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

// True iff `text` parses back as a well-formed Spot automaton (structural
// check on print_hoa's output, not a string comparison).
bool ParsesAsHoa(const std::string& text) {
  auto dict = spot::make_bdd_dict();
  spot::automaton_stream_parser parser(text.c_str(), "<cli-stdout>");
  spot::parsed_aut_ptr pa = parser.parse(dict);
  return pa && !pa->aborted && pa->errors.empty() && pa->aut != nullptr;
}

// ---------------------------------------------------------------------------
// Golden realizable output.
// ---------------------------------------------------------------------------

TEST(LtlfEkSynth, RealizableFormulaPrintsAParsableHoaControllerOnStdout) {
  const CliResult r = RunCli({"--dfa-product", "--formula=G(i -> o)",
                              "--inputs", "i", "--outputs", "o"});
  EXPECT_EQ(r.exit_code, 0);
  EXPECT_TRUE(ParsesAsHoa(r.stdout_text)) << r.stdout_text;
}

// ---------------------------------------------------------------------------
// Verdict codes (PRD "Behaviour" #6).
// ---------------------------------------------------------------------------

TEST(LtlfEkSynth, RealizableFlagPrintsRealizableVerdict) {
  const CliResult r = RunCli({"--dfa-product", "--formula=G(i -> o)",
                              "--inputs", "i", "--outputs", "o",
                              "--realizable"});
  EXPECT_EQ(r.exit_code, 0);
  EXPECT_EQ(r.stdout_text, "REALIZABLE\n");
}

// X[!] i on a free i is unrealizable (tests/dfa_product_test.cpp
// StrongNextOnFreeInputIsUnrealizable).
TEST(LtlfEkSynth, RealizableFlagPrintsUnrealizableVerdictOnStdout) {
  const CliResult r = RunCli({"--dfa-product", "--formula=X[!] i", "--inputs",
                              "i", "--outputs", "o", "--realizable"});
  EXPECT_EQ(r.exit_code, 20);
  EXPECT_EQ(r.stdout_text, "UNREALIZABLE\n");
}

TEST(LtlfEkSynth, DefaultModeUnrealizablePrintsToStderrNotStdout) {
  const CliResult r = RunCli(
      {"--dfa-product", "--formula=X[!] i", "--inputs", "i", "--outputs", "o"});
  EXPECT_EQ(r.exit_code, 20);
  EXPECT_TRUE(r.stdout_text.empty());
  EXPECT_EQ(r.stderr_text, "UNREALIZABLE\n");
}

// ---------------------------------------------------------------------------
// Exit-code matrix: usage errors -> 2 (PRD "Edge cases").
// ---------------------------------------------------------------------------

TEST(LtlfEkSynthExitCodes, NoMethodFlagIsUsageError) {
  EXPECT_EQ(RunCli({"--formula=1", "--inputs", "i"}).exit_code, 2);
}

TEST(LtlfEkSynthExitCodes, TwoMethodFlagsIsUsageError) {
  EXPECT_EQ(RunCli({"--dfa-product", "--nfa-product", "--formula=1",
                    "--inputs", "i"})
                .exit_code,
            2);
}

TEST(LtlfEkSynthExitCodes, MissingFormulaIsUsageError) {
  EXPECT_EQ(RunCli({"--dfa-product", "--inputs", "i"}).exit_code, 2);
}

TEST(LtlfEkSynthExitCodes, BothPartitionSourcesIsUsageError) {
  EXPECT_EQ(RunCli({"--dfa-product", "--formula=1", "--inputs", "i",
                    "--part-file", "/nonexistent/does-not-matter.partfile"})
                .exit_code,
            2);
}

TEST(LtlfEkSynthExitCodes, NeitherPartitionSourceIsUsageError) {
  EXPECT_EQ(RunCli({"--dfa-product", "--formula=1"}).exit_code, 2);
}

TEST(LtlfEkSynthExitCodes, OverlappingInputsOutputsIsUsageError) {
  EXPECT_EQ(RunCli({"--dfa-product", "--formula=1", "--inputs", "i",
                    "--outputs", "i"})
                .exit_code,
            2);
}

TEST(LtlfEkSynthExitCodes, MalformedFormulaIsUsageError) {
  EXPECT_EQ(RunCli({"--dfa-product", "--formula=G(i ->", "--inputs", "i",
                    "--outputs", "o"})
                .exit_code,
            2);
}

// z is neither an input nor an output; DfaProduct::synthesize rejects it
// (dfa_product_test.cpp ThrowsOnFormulaApOutsideInputsOutputs), surfaced by
// the CLI as a usage error.
TEST(LtlfEkSynthExitCodes, FormulaApOutsideInputsOutputsIsUsageError) {
  EXPECT_EQ(RunCli({"--dfa-product", "--formula=z", "--inputs", "i",
                    "--outputs", "o"})
                .exit_code,
            2);
}

TEST(LtlfEkSynthExitCodes, TransducerFlagGivenButKnownSetEmptyIsUsageError) {
  // --inputs/--outputs shorthand always makes V = ∅; supplying a known-input
  // transducer flag on top of that is ambiguous (the file need not even
  // exist --- validation rejects the flag combination first).
  EXPECT_EQ(RunCli({"--dfa-product", "--formula=1", "--inputs", "i",
                    "--outputs", "o", "--known-input-transducer",
                    "/nonexistent/does-not-matter.hoa"})
                .exit_code,
            2);
}

TEST(LtlfEkSynthExitCodes, KnownSetNonEmptyMissingTransducerFlagIsUsageError) {
  const ScopedTempFile part_file(
      "input_free:\n"
      "input_known: i\n"
      "output_free: o\n"
      "output_known:\n");
  EXPECT_EQ(RunCli({"--dfa-product", "--formula=1", "--part-file",
                    part_file.path()})
                .exit_code,
            2);
}

TEST(LtlfEkSynthExitCodes, MalformedPartFileIsUsageError) {
  const ScopedTempFile part_file("not_a_real_key: a\n");
  EXPECT_EQ(RunCli({"--dfa-product", "--formula=1", "--part-file",
                    part_file.path()})
                .exit_code,
            2);
}

TEST(LtlfEkSynthExitCodes, CannotOpenPartFileIsUsageError) {
  EXPECT_EQ(RunCli({"--dfa-product", "--formula=1", "--part-file",
                    "/nonexistent/path/to/nothing.partfile"})
                .exit_code,
            2);
}

TEST(LtlfEkSynthExitCodes, MalformedTransducerFileIsUsageError) {
  const ScopedTempFile part_file(
      "input_free:\n"
      "input_known: i\n"
      "output_free: o\n"
      "output_known:\n");
  const ScopedTempFile transducer_file("not a valid automaton\n--END--\n");
  EXPECT_EQ(RunCli({"--dfa-product", "--formula=1", "--part-file",
                    part_file.path(), "--known-input-transducer",
                    transducer_file.path()})
                .exit_code,
            2);
}

// --- not-yet-implemented deferrals -> exit 1 --------------------------------

TEST(LtlfEkSynthExitCodes, UnwiredMethodFlagExitsOne) {
  const CliResult r = RunCli({"--nfa-product", "--formula=1", "--inputs", "i",
                              "--outputs", "o"});
  EXPECT_EQ(r.exit_code, 1);
  EXPECT_NE(r.stderr_text.find("not yet implemented"), std::string::npos);
}

TEST(LtlfEkSynthExitCodes, ModelCheckFlagExitsOne) {
  const CliResult r = RunCli({"--dfa-product", "--model-check",
                              "--formula=1", "--inputs", "i", "--outputs",
                              "o"});
  EXPECT_EQ(r.exit_code, 1);
  EXPECT_NE(r.stderr_text.find("model-check"), std::string::npos);
}

// PRD "Developer comments": --model-check short-circuits before method
// dispatch, so it reports the deferral even when paired with an unwired
// method flag, not "method not yet implemented".
TEST(LtlfEkSynthExitCodes, ModelCheckTakesPriorityOverUnwiredMethod) {
  const CliResult r = RunCli({"--nfa-product", "--model-check",
                              "--formula=1", "--inputs", "i", "--outputs",
                              "o"});
  EXPECT_EQ(r.exit_code, 1);
  EXPECT_NE(r.stderr_text.find("model-check"), std::string::npos);
  EXPECT_EQ(r.stderr_text.find("nfa-product"), std::string::npos);
}

// ---------------------------------------------------------------------------
// Empty outputs (PRD "Edge cases": legal, no system moves).
// ---------------------------------------------------------------------------

TEST(LtlfEkSynth, EmptyOutputsShorthandTriviallyTruePhiIsRealizable) {
  EXPECT_EQ(RunCli({"--dfa-product", "--formula=1", "--inputs", "i"})
                .exit_code,
            0);
}

TEST(LtlfEkSynth, EmptyOutputsShorthandFreeInputGoalIsUnrealizable) {
  EXPECT_EQ(RunCli({"--dfa-product", "--formula=i", "--inputs", "i"})
                .exit_code,
            20);
}

// ---------------------------------------------------------------------------
// Known-transducer path (PRD "Test oracles": hand-computed expectation).
// Mirrors tests/dfa_product_test.cpp KnowledgeTurnsUnrealizableIntoRealizable:
// phi ties o at step 0 to the input at step 1; free i loses, i known (T_in
// always committing i) wins.
// ---------------------------------------------------------------------------

TEST(LtlfEkSynth, FreeIGoalIsUnrealizable) {
  const std::string phi = "X[!] 1 & (o <-> X i)";
  const CliResult r = RunCli({"--dfa-product", "--formula=" + phi, "--inputs",
                              "i", "--outputs", "o", "--realizable"});
  EXPECT_EQ(r.exit_code, 20);
}

TEST(LtlfEkSynth, KnownInputTransducerTurnsUnrealizableIntoRealizable) {
  const std::string phi = "X[!] 1 & (o <-> X i)";
  const ScopedTempFile part_file(
      "input_free:\n"
      "input_known: i\n"
      "output_free: o\n"
      "output_known:\n");
  // T_in: single state, delta self-loops, lambda always commits i (Sigma0 =
  // Ifree = ∅, Sigma1 = Iknown = {i}) --- the file's analogue of
  // dfa_product_test.cpp's TinAlwaysI.
  const ScopedTempFile transducer_file(
      "HOA: v1\n"
      "States: 1\n"
      "Start: 0\n"
      "AP: 1 \"i\"\n"
      "acc-name: all\n"
      "Acceptance: 0 t\n"
      "--BODY--\n"
      "State: 0\n"
      "  [t] 0\n"
      "--END--\n"
      "%%LAMBDA\n"
      "state 0: i\n");
  const CliResult r =
      RunCli({"--dfa-product", "--formula=" + phi, "--part-file",
              part_file.path(), "--known-input-transducer",
              transducer_file.path(), "--realizable"});
  EXPECT_EQ(r.exit_code, 0);
  EXPECT_EQ(r.stdout_text, "REALIZABLE\n");
}

// ---------------------------------------------------------------------------
// Cross-check vs the library (PRD "Test oracles"): the CLI adds no semantics,
// so its verdict over the empty-V shorthand must match DfaProduct::synthesize
// invoked directly with Trivial transducers on the same formulas.
// ---------------------------------------------------------------------------

bool DirectDfaProductRealizable(const std::string& phi) {
  auto dict = spot::make_bdd_dict();
  const VariablePartition part =
      VariablePartition::split({"i"}, {"o"}, /*governed=*/{});
  const OutputLabeledTransducer t_in =
      trivial_transducer(part, Role::t_in, dict);
  const OutputLabeledTransducer t_out =
      trivial_transducer(part, Role::t_out, dict);
  DfaProduct method;
  return method.synthesize(spot::parse_formula(phi), part, t_in, t_out)
      .has_value();
}

TEST(LtlfEkSynth, CliVerdictMatchesDirectDfaProductAcrossFormulas) {
  const std::vector<std::string> phis = {"o",   "0",         "1",
                                         "i",   "G(i -> o)", "X[!] i",
                                         "X[!] o"};
  for (const auto& phi : phis) {
    SCOPED_TRACE(phi);
    const CliResult r = RunCli({"--dfa-product", "--formula=" + phi,
                                "--inputs", "i", "--outputs", "o",
                                "--realizable"});
    ASSERT_TRUE(r.exit_code == 0 || r.exit_code == 20)
        << "unexpected exit code " << r.exit_code << ", stderr: "
        << r.stderr_text;
    EXPECT_EQ(r.exit_code == 0, DirectDfaProductRealizable(phi));
  }
}

}  // namespace
