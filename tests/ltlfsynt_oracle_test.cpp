// External, independent realizability oracle (docs/prd/ltlfsynt-oracle.md):
// drives the built `ltlf-ek-synth` binary (Method 2 / DfaProduct, known-input
// Tin) and Spot's own `ltlfsynt` binary as two *separate* subprocesses on the
// equirealizable assumption-reduction encoding psi_in -> phi, and compares
// only the printed REALIZABLE/UNREALIZABLE verdict word.  The two paths share
// no code -- that is what makes the check independent (PRD "Behaviour" #1);
// this file adds no production C++, only test fixtures.
//
// GTEST_SKIP()s wholesale when `ltlfsynt` cannot be located/run (CMake
// find_program(LTLFSYNT_EXECUTABLE) + the LTLFSYNT_BIN env override), so a
// clean CI box without Spot's CLI tools is a no-op, not a failure.

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
#include <spot/tl/parse.hh>
#include <spot/twa/bdddict.hh>

#include "ltlf_ek/ltlf_to_dfa.hpp"
#include "ltlf_ek/output_labeled_transducer.hpp"
#include "ltlf_ek/transducer_io.hpp"
#include "ltlf_ek/variables.hpp"

#ifndef LTLF_EK_SYNTH_BINARY
#error "LTLF_EK_SYNTH_BINARY must be defined by CMake (see CMakeLists.txt)"
#endif

namespace {

using ltlf_ek::collect_aps;
using ltlf_ek::ltlf_to_dfa;
using ltlf_ek::OutputLabeledTransducer;
using ltlf_ek::parse_transducer;
using ltlf_ek::Role;
using ltlf_ek::VariablePartition;

// ---------------------------------------------------------------------------
// Subprocess harness (mirrors tests/ltlf_ek_synth_test.cpp's RunCli /
// ShellQuote / ScopedTempFile; duplicated rather than shared across
// translation units, matching this project's existing one-file-per-suite
// style). Generalised to run *either* binary, since this suite drives two.
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

// A self-deleting temp file, used both to capture a subprocess's stdout/
// stderr and to hold fixture input (part-files, transducer files).
class ScopedTempFile {
 public:
  explicit ScopedTempFile(const std::string& contents = "") {
    path_ = "/tmp/ltlfsynt_oracle_test_XXXXXX";
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

// Runs `binary` with `args` via a POSIX shell (so each stream can be
// redirected to its own temp file), returning its exit code, stdout, stderr.
CliResult RunSubprocess(const std::string& binary,
                        const std::vector<std::string>& args) {
  ScopedTempFile out_capture, err_capture;
  std::ostringstream cmd;
  cmd << ShellQuote(binary);
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

CliResult RunEkSynth(const std::vector<std::string>& args) {
  return RunSubprocess(LTLF_EK_SYNTH_BINARY, args);
}

// ---------------------------------------------------------------------------
// `ltlfsynt` binary resolution (PRD "Interfaces & types"): the LTLFSYNT_BIN
// runtime env override takes precedence over the CMake-configured
// LTLFSYNT_BINARY compile define.
// ---------------------------------------------------------------------------

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

// ---------------------------------------------------------------------------
// Verdict parsing (PRD "Behaviour" #3 / "Edge cases" "Verdict-vs-error
// ambiguity"): parse the printed word, never the exit code alone.
// ltlf-ek-synth's {realizable, unrealizable} exit-code pair is {0, 20};
// ltlfsynt's is {0, 1} -- deliberately different, and neither tool's *other*
// exit codes (usage/parse errors) may be silently read as a verdict.  An
// unexpected exit code or a missing verdict word fails loudly with the
// captured stderr.
// ---------------------------------------------------------------------------

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

// ---------------------------------------------------------------------------
// Fixture: skips the whole suite when ltlfsynt is not runnable (PRD
// "Interfaces & types" / "Edge cases": never a hard failure).
// ---------------------------------------------------------------------------

class LtlfsyntOracleTest : public ::testing::Test {
 protected:
  void SetUp() override {
    ltlfsynt_binary_ = ResolveLtlfsyntBinary();
    if (!IsRunnable(ltlfsynt_binary_))
      GTEST_SKIP() << "ltlfsynt not found/runnable (checked the LTLFSYNT_BIN "
                      "env override and CMake find_program(ltlfsynt)); "
                      "skipping the external oracle";
  }

  CliResult RunLtlfsynt(const std::vector<std::string>& args) {
    return RunSubprocess(ltlfsynt_binary_, args);
  }

 private:
  std::string ltlfsynt_binary_;
};

// ---------------------------------------------------------------------------
// The Tin fixture files (PRD "Test oracles"): three single-state files
// sharing one delta (self-loop on [t]) and one lambda formula each, plus one
// 2-state delay file. Partition shared by Tables A-D:
// input_free=a, input_known=k, output_free=o.
// ---------------------------------------------------------------------------

constexpr char kPartFileAKO[] =
    "input_free:   a\n"
    "input_known:  k\n"
    "output_free:  o\n"
    "output_known:\n";

// Tin: const-true (state 0: k). psi_in = G(k).
constexpr char kTinConstTrue[] =
    "HOA: v1\n"
    "States: 1\n"
    "Start: 0\n"
    "AP: 2 \"a\" \"k\"\n"
    "acc-name: all\n"
    "Acceptance: 0 t\n"
    "--BODY--\n"
    "State: 0\n"
    "  [t] 0\n"
    "--END--\n"
    "%%LAMBDA\n"
    "state 0: k\n";

// Tin: const-false (state 0: !k). psi_in = G(!k).
constexpr char kTinConstFalse[] =
    "HOA: v1\n"
    "States: 1\n"
    "Start: 0\n"
    "AP: 2 \"a\" \"k\"\n"
    "acc-name: all\n"
    "Acceptance: 0 t\n"
    "--BODY--\n"
    "State: 0\n"
    "  [t] 0\n"
    "--END--\n"
    "%%LAMBDA\n"
    "state 0: !k\n";

// Tin: copy, k = a (state 0: a <-> k). psi_in = G(k <-> a).
constexpr char kTinCopy[] =
    "HOA: v1\n"
    "States: 1\n"
    "Start: 0\n"
    "AP: 2 \"a\" \"k\"\n"
    "acc-name: all\n"
    "Acceptance: 0 t\n"
    "--BODY--\n"
    "State: 0\n"
    "  [t] 0\n"
    "--END--\n"
    "%%LAMBDA\n"
    "state 0: a <-> k\n";

// Tin: one-step delay, k_t = a_{t-1}, k_0 = false; 2-state.
// psi_in (docs/prd/oracle-faithfulness-guard.md "Behaviour" #1) is
// kPsiInDelayCorrected below: pure-safety, k_t=a_{t-1}/k_0=bot, weak X in the
// guarded form (a total strategy imposes safety only, must not force
// continuation).  The former string, (!k) & G(X(k <-> a)), actually encoded
// copy-from-step-1 (k_t<->a_t for t>=1) -- a different language -- which is
// why it is kept below ONLY as the known-bad pairing for the faithfulness
// guard meta-oracle, never as a corpus psi_in.
constexpr char kTinDelay[] =
    "HOA: v1\n"
    "States: 2\n"
    "Start: 0\n"
    "AP: 2 \"a\" \"k\"\n"
    "acc-name: all\n"
    "Acceptance: 0 t\n"
    "--BODY--\n"
    "State: 0\n"
    "  [!0] 0\n"
    "  [0]  1\n"
    "State: 1\n"
    "  [!0] 0\n"
    "  [0]  1\n"
    "--END--\n"
    "%%LAMBDA\n"
    "state 0: !k\n"
    "state 1: k\n";

// Corrected delay psi_in (docs/prd/oracle-faithfulness-guard.md "Behaviour"
// #1): pure safety, k_t=a_{t-1}, k_0=bot.  Weak X in the guarded form (not
// X[!]) so a total strategy imposes safety only and adds no forced-
// continuation obligation; unlike the naive (!k) & G(a <-> X k), it does not
// spuriously force `a` true at the last step (weak `X k` at the final
// position returns true, collapsing `a <-> X k` to `a`).
constexpr char kPsiInDelayCorrected[] =
    "(!k) & G(a -> X k) & G(!a -> X !k)";

// ---------------------------------------------------------------------------
// Tables A-D: known-input Tin corpus (PRD "Test oracles").
// ---------------------------------------------------------------------------

struct KnownInputRow {
  std::string name;  // gtest-safe identifier suffix
  const char* tin_file;
  std::string psi_in;
  std::string phi;
  bool realizable;
  bool load_bearing;  // if true: the bare-phi verdict must differ (a flip)
};

// Without this, GoogleTest's `--gtest_list_tests` (which CMake's
// gtest_discover_tests folds into the CTest test name) falls back to a raw
// memory dump of the struct; print the readable row name instead.
void PrintTo(const KnownInputRow& row, std::ostream* os) { *os << row.name; }

std::vector<KnownInputRow> BuildKnownInputCorpus() {
  return {
      // --- Table A: Tin const-true, psi_in = G(k) ---
      {"A_k", kTinConstTrue, "G(k)", "k", true, true},
      {"A_XBang_k", kTinConstTrue, "G(k)", "X[!] k", true, true},
      {"A_XBang_XBang_k", kTinConstTrue, "G(k)", "X[!](X[!] k)", true, true},
      {"A_G_k", kTinConstTrue, "G(k)", "G(k)", true, true},
      {"A_F_k", kTinConstTrue, "G(k)", "F(k)", true, true},
      {"A_XBang_k_and_o", kTinConstTrue, "G(k)", "X[!](k & o)", true, true},
      {"A_XBang_k_implies_o", kTinConstTrue, "G(k)", "X[!](k -> o)", true,
       false},
      {"A_G_k_implies_o", kTinConstTrue, "G(k)", "G(k -> o)", true, false},

      // --- Table B: Tin const-false, psi_in = G(!k) ---
      {"B_not_k", kTinConstFalse, "G(!k)", "!k", true, true},
      {"B_G_not_k", kTinConstFalse, "G(!k)", "G(!k)", true, true},
      {"B_XBang_not_k", kTinConstFalse, "G(!k)", "X[!] !k", true, true},
      {"B_F_not_k", kTinConstFalse, "G(!k)", "F(!k)", true, true},
      {"B_XBang_k", kTinConstFalse, "G(!k)", "X[!] k", false, false},
      {"B_G_k", kTinConstFalse, "G(!k)", "G(k)", false, false},

      // --- Table C: Tin copy (k = a), psi_in = G(k <-> a) ---
      {"C_XBang_k_iff_a", kTinCopy, "G(k <-> a)", "X[!](k <-> a)", true,
       true},
      {"C_G_a_implies_k", kTinCopy, "G(k <-> a)", "G(a -> k)", true, true},
      {"C_G_k_implies_a", kTinCopy, "G(k <-> a)", "G(k -> a)", true, true},
      {"C_G_o_iff_k", kTinCopy, "G(k <-> a)", "G(o <-> k)", true, false},
      {"C_XBang_a_and_k", kTinCopy, "G(k <-> a)", "X[!](a & k)", false,
       false},
      {"C_F_k_and_not_a", kTinCopy, "G(k <-> a)", "F(k & !a)", false, false},

      // --- Table D: Tin one-step delay, psi_in = kPsiInDelayCorrected ---
      {"D_XBang_k", kTinDelay, kPsiInDelayCorrected, "X[!] k", false, false},
      {"D_G_a_implies_XBang_k", kTinDelay, kPsiInDelayCorrected,
       "G(a -> X[!] k)", false, false},
      {"D_F_k", kTinDelay, kPsiInDelayCorrected, "F(k)", false, false},
      {"D_XBang_XBang_k", kTinDelay, kPsiInDelayCorrected, "X[!](X[!] k)",
       false, false},
      {"D_k", kTinDelay, kPsiInDelayCorrected, "k", false, false},
      {"D_G_o_iff_k", kTinDelay, kPsiInDelayCorrected, "G(o <-> k)", true,
       false},
  };
}

class KnownInputOracleTest
    : public LtlfsyntOracleTest,
      public ::testing::WithParamInterface<KnownInputRow> {};

TEST_P(KnownInputOracleTest, MatchesLtlfsyntUnderAssumptionReduction) {
  const KnownInputRow& row = GetParam();
  const ScopedTempFile part_file(kPartFileAKO);
  const ScopedTempFile transducer_file(row.tin_file);

  const CliResult ek =
      RunEkSynth({"--dfa-product", "--formula=" + row.phi, "--part-file",
                  part_file.path(), "--known-input-transducer",
                  transducer_file.path(), "--realizable"});
  const Verdict ek_verdict = ParseEkSynthVerdict(ek);
  EXPECT_EQ(IsRealizable(ek_verdict), row.realizable)
      << "ltlf-ek-synth verdict for phi=" << row.phi
      << " does not match the PRD corpus (docs/prd/ltlfsynt-oracle.md)";

  const std::string reduced = "(" + row.psi_in + ") -> (" + row.phi + ")";
  const CliResult synt =
      RunLtlfsynt({"--ins=a,k", "--outs=o", "--semantics=Mealy",
                   "--realizability", "-f", reduced});
  const Verdict synt_verdict = ParseLtlfsyntVerdict(synt);

  // The oracle itself: ltlf-ek-synth (known-input problem) and ltlfsynt
  // (psi_in -> phi reduction) must agree on realizability.
  EXPECT_EQ(IsRealizable(ek_verdict), IsRealizable(synt_verdict))
      << "phi=" << row.phi << ", psi_in=" << row.psi_in;

  if (row.load_bearing) {
    // Load-bearing guard (PRD "Behaviour" #4): the bare-phi verdict (drop
    // psi_in, keep k on --ins) must be the *opposite* of the reduction's
    // verdict, proving psi_in actually changed the outcome.
    const CliResult bare =
        RunLtlfsynt({"--ins=a,k", "--outs=o", "--semantics=Mealy",
                     "--realizability", "-f", row.phi});
    const Verdict bare_verdict = ParseLtlfsyntVerdict(bare);
    EXPECT_NE(IsRealizable(bare_verdict), IsRealizable(synt_verdict))
        << "load-bearing guard failed: psi_in did not change the verdict "
           "for phi="
        << row.phi;
  }
}

INSTANTIATE_TEST_SUITE_P(
    KnownInputCorpus, KnownInputOracleTest,
    ::testing::ValuesIn(BuildKnownInputCorpus()),
    [](const ::testing::TestParamInfo<KnownInputRow>& info) {
      return info.param.name;
    });

// ---------------------------------------------------------------------------
// Table E: empty knowledge (V = empty), no transducer -- the Mealy-only
// payoff rows (`o <-> i`, `G(o <-> i)`) the Moore baseline
// (EmptyKnowledgeMatchesMonolithicBaseline, tests/dfa_product_test.cpp:160)
// cannot cover.  Also covers the empty-trace convention alignment ("0"; PRD
// "Open theory questions").
// ---------------------------------------------------------------------------

struct EmptyKnowledgeRow {
  std::string name;
  std::string phi;
  bool realizable;
};

void PrintTo(const EmptyKnowledgeRow& row, std::ostream* os) {
  *os << row.name;
}

std::vector<EmptyKnowledgeRow> BuildEmptyKnowledgeCorpus() {
  return {
      {"o", "o", true},
      {"zero", "0", false},
      {"one", "1", true},
      {"i", "i", false},
      {"G_i_implies_o", "G(i -> o)", true},
      {"XBang_i", "X[!] i", false},
      {"XBang_o", "X[!] o", true},
      {"F_o", "F o", true},
      {"G_i", "G i", false},
      {"o_U_i", "o U i", false},
      {"i_U_o", "i U o", true},
      {"G_o_or_i", "G(o) | i", true},
      {"o_iff_i", "o <-> i", true},        // Mealy-only
      {"G_o_iff_i", "G(o <-> i)", true},   // Mealy-only
  };
}

class EmptyKnowledgeOracleTest
    : public LtlfsyntOracleTest,
      public ::testing::WithParamInterface<EmptyKnowledgeRow> {};

TEST_P(EmptyKnowledgeOracleTest, MatchesBareLtlfsyntUnderMealySemantics) {
  const EmptyKnowledgeRow& row = GetParam();
  const CliResult ek =
      RunEkSynth({"--dfa-product", "--formula=" + row.phi, "--inputs", "i",
                  "--outputs", "o", "--realizable"});
  const Verdict ek_verdict = ParseEkSynthVerdict(ek);
  EXPECT_EQ(IsRealizable(ek_verdict), row.realizable)
      << "ltlf-ek-synth verdict for phi=" << row.phi
      << " does not match the PRD corpus";

  const CliResult synt = RunLtlfsynt({"--ins=i", "--outs=o",
                                       "--semantics=Mealy", "--realizability",
                                       "-f", row.phi});
  const Verdict synt_verdict = ParseLtlfsyntVerdict(synt);
  EXPECT_EQ(IsRealizable(ek_verdict), IsRealizable(synt_verdict))
      << "empty-knowledge Mealy disagreement for phi=" << row.phi;
}

INSTANTIATE_TEST_SUITE_P(
    EmptyKnowledgeCorpus, EmptyKnowledgeOracleTest,
    ::testing::ValuesIn(BuildEmptyKnowledgeCorpus()),
    [](const ::testing::TestParamInfo<EmptyKnowledgeRow>& info) {
      return info.param.name;
    });

// ---------------------------------------------------------------------------
// Edge case: empty Ofree (PRD "Edge cases" "Empty Ofree") -- smoke-test that
// ltlfsynt accepts an empty/omitted --outs the same way ltlf-ek-synth accepts
// an empty --outputs (tests/ltlf_ek_synth_test.cpp
// EmptyOutputsShorthand{TriviallyTruePhiIsRealizable,
// FreeInputGoalIsUnrealizable}). No fixture table above exercises this
// partition shape, so it is not folded into the parameterised corpus.
// ---------------------------------------------------------------------------

TEST_F(LtlfsyntOracleTest, EmptyOutputsAcceptedByBothToolsTriviallyTrue) {
  const CliResult ek = RunEkSynth(
      {"--dfa-product", "--formula=1", "--inputs", "i", "--realizable"});
  EXPECT_TRUE(IsRealizable(ParseEkSynthVerdict(ek)));
  const CliResult synt = RunLtlfsynt(
      {"--ins=i", "--semantics=Mealy", "--realizability", "-f", "1"});
  EXPECT_TRUE(IsRealizable(ParseLtlfsyntVerdict(synt)));
}

TEST_F(LtlfsyntOracleTest, EmptyOutputsAcceptedByBothToolsFreeInputGoal) {
  const CliResult ek = RunEkSynth(
      {"--dfa-product", "--formula=i", "--inputs", "i", "--realizable"});
  EXPECT_FALSE(IsRealizable(ParseEkSynthVerdict(ek)));
  const CliResult synt = RunLtlfsynt(
      {"--ins=i", "--semantics=Mealy", "--realizability", "-f", "i"});
  EXPECT_FALSE(IsRealizable(ParseLtlfsyntVerdict(synt)));
}

// ---------------------------------------------------------------------------
// AP naming (PRD "Edge cases" "AP naming"): a typo'd AP silently becomes a
// fresh free input to ltlfsynt and can flip the verdict. Assert every
// fixture's phi/psi_in stays inside its declared partition -- pure formula
// parsing, does not invoke either subprocess, so it always runs.
// ---------------------------------------------------------------------------

TEST(LtlfsyntOracleApNaming, KnownInputCorpusApsMatchPartFile) {
  const std::set<std::string> allowed = {"a", "k", "o"};
  for (const auto& row : BuildKnownInputCorpus()) {
    SCOPED_TRACE(row.name);
    for (const std::string& ap : collect_aps(spot::parse_formula(row.phi)))
      EXPECT_TRUE(allowed.count(ap)) << "phi AP outside partition: " << ap;
    for (const std::string& ap : collect_aps(spot::parse_formula(row.psi_in)))
      EXPECT_TRUE(allowed.count(ap)) << "psi_in AP outside partition: " << ap;
  }
}

TEST(LtlfsyntOracleApNaming, EmptyKnowledgeCorpusApsMatchPartition) {
  const std::set<std::string> allowed = {"i", "o"};
  for (const auto& row : BuildEmptyKnowledgeCorpus()) {
    SCOPED_TRACE(row.name);
    for (const std::string& ap : collect_aps(spot::parse_formula(row.phi)))
      EXPECT_TRUE(allowed.count(ap)) << "phi AP outside partition: " << ap;
  }
}

// ---------------------------------------------------------------------------
// Corrected-witness load-bearing flip (docs/prd/oracle-faithfulness-guard.md
// "Behaviour" #1, formerly the DISABLED_ "excluded class" divergence
// witness): with the corrected delay psi_in this pairing is a normal
// PASSING agreement (both REALIZABLE), and it is load-bearing (bare phi is
// UNREALIZABLE).  What looked like a soundness boundary of the assumption
// reduction was a psi_in <-> transducer mis-encoding (copy-from-step-1
// instead of delay) -- not a genuine reduction divergence.
// ---------------------------------------------------------------------------

TEST_F(LtlfsyntOracleTest, DelayCorrectedPsiInAgreesLoadBearingWithBarePhi) {
  const std::string phi = "X[!](a -> X[!] k)";
  const ScopedTempFile part_file(kPartFileAKO);
  const ScopedTempFile transducer_file(kTinDelay);

  const CliResult ek =
      RunEkSynth({"--dfa-product", "--formula=" + phi, "--part-file",
                  part_file.path(), "--known-input-transducer",
                  transducer_file.path(), "--realizable"});
  EXPECT_EQ(ParseEkSynthVerdict(ek), Verdict::kRealizable);

  const std::string reduced =
      "(" + std::string(kPsiInDelayCorrected) + ") -> (" + phi + ")";
  const CliResult synt =
      RunLtlfsynt({"--ins=a,k", "--outs=o", "--semantics=Mealy",
                   "--realizability", "-f", reduced});
  EXPECT_EQ(ParseLtlfsyntVerdict(synt), Verdict::kRealizable);

  // Load-bearing guard (docs/prd/oracle-faithfulness-guard.md "Behaviour" #1,
  // the "load-bearing flip"): the bare-phi verdict (drop psi_in) must be the
  // opposite, proving psi_in actually changed the outcome.
  const CliResult bare = RunLtlfsynt({"--ins=a,k", "--outs=o",
                                       "--semantics=Mealy", "--realizability",
                                       "-f", phi});
  EXPECT_EQ(ParseLtlfsyntVerdict(bare), Verdict::kUnrealizable);
}

// ---------------------------------------------------------------------------
// Faithfulness guard (docs/prd/oracle-faithfulness-guard.md "Behaviour" #2):
// a mechanical, author-blind-spot-independent cross-check that a Tin
// fixture's psi_in and its transducer FILE denote the same produced-trace
// language.  Drives the *same* two artifacts the author already wrote
// against each other -- parse_transducer's run engine and ltlf_to_dfa's
// finite-LTLf membership -- never a third, hand-labeled trace (which would
// inherit the author's own blind spot and pass vacuously).  Library-only: no
// subprocess, so it runs even where `ltlfsynt` is absent (PRD "Edge cases").
// ---------------------------------------------------------------------------

constexpr unsigned kGuardMaxSeqLen = 5;          // N (PRD "Behaviour" #2).
constexpr std::size_t kGuardEnumCap = 4096;      // exhaustive iff alphabet^len <= cap.
constexpr std::size_t kGuardSampleCount = 4096;  // K, else fixed-seed sampling.
constexpr unsigned kGuardSampleSeed = 20260705;   // fixed seed (deterministic).

// run_transducer (PRD "Interfaces"): drive delta/lambda from q0 over one
// Ifree-sequence.  Each letter is `ifree_slice & lambda(q, ifree_slice)`
// (Sigma0=Ifree, Sigma1=Iknown for Role::t_in) -- well-defined because Tin is
// deterministic and total in the committed Case-A regime (main.tex
// \cref{def:enabled}, glossary "Partial transducers -- resolved").  No Ofree
// slice is materialised: none of the four Tin fixtures' transducer files or
// psi_in strings mention an Ofree AP, so there is nothing to fix to a
// canonical value for this corpus (PRD "Edge cases" "Ofree don't-cares").  A
// step whose delta/lambda is undefined (partial transducer, main.tex §107)
// yields no trace for that sequence -- signalled by nullopt, so the caller
// can skip it (PRD "Edge cases" "Partial transducer").
std::optional<std::vector<bdd>> run_transducer(
    const OutputLabeledTransducer& tau, const std::vector<bdd>& ifree_seq) {
  std::vector<bdd> trace;
  trace.reserve(ifree_seq.size());
  unsigned q = tau.initial_state();
  for (const bdd& ifree : ifree_seq) {
    const std::optional<bdd> iknown = tau.lambda(q, ifree);
    if (!iknown) return std::nullopt;
    const bdd letter = ifree & *iknown;
    const std::optional<unsigned> next = tau.delta(q, letter);
    if (!next) return std::nullopt;
    trace.push_back(letter);
    q = *next;
  }
  return trace;
}

// accepts_ltlf (PRD "Interfaces"): walk a concrete finite trace through
// ltlf_to_dfa(psi_in)'s automaton and test the finite-acceptance mark
// (accepting = F_D, see ltlf_to_dfa.hpp) at the reached state.  Same
// delta-navigation idiom as dfa_product.cpp's dfa_delta /
// OutputLabeledTransducer::delta: a letter satisfies guard e.cond exactly
// when (v & e.cond) != bddfalse.  ltlf_to_dfa completes the DFA
// (spot::complete_here), so delta is total here.
bool accepts_ltlf(const spot::twa_graph_ptr& a_psi,
                   const std::vector<bdd>& trace) {
  unsigned s = a_psi->get_init_state_number();
  for (const bdd& v : trace) {
    std::optional<unsigned> next;
    for (const auto& e : a_psi->out(s)) {
      if ((v & e.cond) != bddfalse) {
        next = e.dst;
        break;
      }
    }
    if (!next)
      throw std::runtime_error(
          "accepts_ltlf: ltlf_to_dfa's automaton is expected complete");
    s = *next;
  }
  return a_psi->state_is_accepting(s);
}

// Every full letter over `vars`: the exponential enumeration base, same
// idiom as dfa_product.cpp's all_letters.
std::vector<bdd> all_letters_over(const std::vector<int>& vars) {
  const std::size_t n = vars.size();
  std::vector<bdd> letters;
  letters.reserve(std::size_t{1} << n);
  for (std::size_t k = 0; k < (std::size_t{1} << n); ++k) {
    bdd v = bddtrue;
    for (std::size_t i = 0; i < n; ++i)
      v &= (k >> i & 1) ? bdd_ithvar(vars[i]) : bdd_nithvar(vars[i]);
    letters.push_back(v);
  }
  return letters;
}

// base^exp, saturating at kGuardEnumCap + 1 on overflow so a huge exponent
// always compares as "exceeds the cap" rather than wrapping around.
std::size_t pow_saturating(std::size_t base, unsigned exp) {
  std::size_t r = 1;
  for (unsigned i = 0; i < exp; ++i) {
    if (r > kGuardEnumCap / (base == 0 ? 1 : base)) return kGuardEnumCap + 1;
    r *= base;
    if (r > kGuardEnumCap) return kGuardEnumCap + 1;
  }
  return r;
}

// Every Ifree-sequence of exactly `len` (PRD "Behaviour" #2): exhaustive iff
// alphabet^len <= kGuardEnumCap, else kGuardSampleCount fixed-seed random
// sequences.
std::vector<std::vector<bdd>> ifree_sequences_of_length(
    const std::vector<bdd>& letters, unsigned len) {
  const std::size_t alphabet = letters.size();
  std::vector<std::vector<bdd>> sequences;
  if (alphabet == 0) {
    sequences.push_back(std::vector<bdd>(len, bddtrue));
    return sequences;
  }

  if (pow_saturating(alphabet, len) <= kGuardEnumCap) {
    const std::size_t total = pow_saturating(alphabet, len);
    for (std::size_t idx = 0; idx < total; ++idx) {
      std::vector<bdd> seq(len);
      std::size_t rem = idx;
      for (unsigned t = 0; t < len; ++t) {
        seq[t] = letters[rem % alphabet];
        rem /= alphabet;
      }
      sequences.push_back(std::move(seq));
    }
  } else {
    std::mt19937 rng(kGuardSampleSeed);
    std::uniform_int_distribution<std::size_t> pick(0, alphabet - 1);
    for (std::size_t i = 0; i < kGuardSampleCount; ++i) {
      std::vector<bdd> seq(len);
      for (unsigned t = 0; t < len; ++t) seq[t] = letters[pick(rng)];
      sequences.push_back(std::move(seq));
    }
  }
  return sequences;
}

// Ifree_sequences "up to N=5" (PRD "Behaviour" #2 pseudocode): every length
// from 1 through max_len, so the guard also exercises the shorter traces
// where a last-position weak-X trap can hide.  Length 0 (the empty trace) is
// deliberately excluded: LTLf traces are non-empty (main.tex §85, models
// range over (2^{I∪O})^+ -- glossary/memory "ltlf-weak-x-and-termination-
// semantics"), so ltlf_to_dfa's initial state is non-accepting for EVERY
// psi_in, even a trivially-true one (tests/ltlf_to_dfa_test.cpp
// TriviallyTrueRejectsEmptyWordButAcceptsAfterOneStep) -- testing it here
// would spuriously trip assertion (a) on every fixture, not just a
// genuinely-drifted one.
std::vector<std::vector<bdd>> ifree_sequences(const std::vector<int>& ifree_vars,
                                              unsigned max_len) {
  const std::vector<bdd> letters = all_letters_over(ifree_vars);
  std::vector<std::vector<bdd>> all;
  for (unsigned len = 1; len <= max_len; ++len) {
    std::vector<std::vector<bdd>> for_len =
        ifree_sequences_of_length(letters, len);
    all.insert(all.end(), for_len.begin(), for_len.end());
  }
  return all;
}

// single_bit_Iknown_mutations (PRD "Interfaces" / "Mutation soundness"):
// every trace obtained by flipping exactly one Iknown bit at exactly one
// position, holding everything else (the Ifree history and every other
// Iknown bit) fixed at tau's own committed value.  Relies on Tin
// determinism + totality: the required Iknown at that position is a
// function of the fixed Ifree-history alone, so the flip is genuinely
// out of psi_in's language whenever psi_in is neither too strong nor too
// weak.
std::vector<std::vector<bdd>> single_bit_iknown_mutations(
    const std::vector<bdd>& trace, const std::vector<int>& iknown_vars) {
  std::vector<std::vector<bdd>> mutations;
  for (std::size_t pos = 0; pos < trace.size(); ++pos) {
    for (int x : iknown_vars) {
      std::vector<bdd> m = trace;
      const bool was_true = (trace[pos] & bdd_ithvar(x)) != bddfalse;
      const bdd rest = bdd_exist(trace[pos], bdd_ithvar(x));
      m[pos] = rest & (was_true ? bdd_nithvar(x) : bdd_ithvar(x));
      mutations.push_back(std::move(m));
    }
  }
  return mutations;
}

// run_faithfulness_guard result: `ok` is false the first time either
// assertion (a) psi_in-too-STRONG or (b) psi_in-too-WEAK trips; `detail`
// explains which.  A plain bool-returning helper (not gtest assertions
// itself) so the guard meta-oracle can assert it FAILS without turning the
// whole suite red (PRD "Behaviour" #3).
struct GuardResult {
  bool ok = true;
  std::string detail;
};

// run_faithfulness_guard (PRD "Interfaces"): ties run_transducer,
// accepts_ltlf, Ifree_sequences and single_bit_Iknown_mutations together for
// one (Tin, psi_in) pair, on a FRESH, private bdd_dict (never the CLI
// harness's or another guard call's, so no cross-fixture variable-numbering
// leak can mask a mismatch).
GuardResult run_faithfulness_guard(const std::string& transducer_src,
                                   const std::string& psi_in,
                                   const VariablePartition& partition) {
  const spot::bdd_dict_ptr dict = spot::make_bdd_dict();
  std::istringstream transducer_in(transducer_src);
  const OutputLabeledTransducer tau =
      parse_transducer(transducer_in, partition, Role::t_in, dict);
  const spot::twa_graph_ptr a_psi =
      ltlf_to_dfa(spot::parse_formula(psi_in), dict);

  std::vector<int> ifree_vars, iknown_vars;
  for (const std::string& n : partition.input_free)
    ifree_vars.push_back(a_psi->register_ap(n));
  for (const std::string& n : partition.input_known)
    iknown_vars.push_back(a_psi->register_ap(n));

  for (const std::vector<bdd>& seq :
       ifree_sequences(ifree_vars, kGuardMaxSeqLen)) {
    const std::optional<std::vector<bdd>> trace = run_transducer(tau, seq);
    if (!trace) continue;  // partial delta/lambda: skip (PRD "Edge cases").

    if (!accepts_ltlf(a_psi, *trace))
      return {false,
              "psi_in too STRONG: rejects a trace tau itself produced"};

    for (const std::vector<bdd>& mutated :
         single_bit_iknown_mutations(*trace, iknown_vars)) {
      if (accepts_ltlf(a_psi, mutated))
        return {false,
                "psi_in too WEAK: accepts a single-bit Iknown mutation of a "
                "trace tau produced"};
    }
  }
  return {true, ""};
}

// Shared partition for the guard's own fixtures (mirrors kPartFileAKO).
const VariablePartition kPartitionAKO{
    /*input_free=*/{"a"}, /*input_known=*/{"k"}, /*output_free=*/{"o"},
    /*output_known=*/{}};

struct FaithfulnessFixture {
  std::string name;
  std::string tin_file;
  std::string psi_in;
};

void PrintTo(const FaithfulnessFixture& f, std::ostream* os) {
  *os << f.name;
}

std::vector<FaithfulnessFixture> BuildFaithfulnessFixtures() {
  return {
      {"ConstTrue", kTinConstTrue, "G(k)"},
      {"ConstFalse", kTinConstFalse, "G(!k)"},
      {"Copy", kTinCopy, "G(k <-> a)"},
      {"DelayCorrected", kTinDelay, kPsiInDelayCorrected},
  };
}

// Guard over the whole Tin corpus (PRD "Test oracles"): all four fixtures
// pass -- only delay's string changed, this confirms the other three were
// already faithful.  Not gated on ltlfsynt (library-only): a plain
// TestWithParam, not LtlfsyntOracleTest.
class FaithfulnessGuardTest
    : public ::testing::TestWithParam<FaithfulnessFixture> {};

TEST_P(FaithfulnessGuardTest, TinFixtureIsFaithfulToItsPsiIn) {
  const FaithfulnessFixture& f = GetParam();
  const GuardResult result =
      run_faithfulness_guard(f.tin_file, f.psi_in, kPartitionAKO);
  EXPECT_TRUE(result.ok) << result.detail;
}

INSTANTIATE_TEST_SUITE_P(
    TinCorpus, FaithfulnessGuardTest,
    ::testing::ValuesIn(BuildFaithfulnessFixtures()),
    [](const ::testing::TestParamInfo<FaithfulnessFixture>& info) {
      return info.param.name;
    });

// Guard meta-oracle (PRD "Behaviour" #3): a guard that never fires is
// worthless.  Run it on the known-bad pairing -- the delay transducer with
// the OLD, buggy "(!k) & G(X(k <-> a))" string (copy-from-step-1, not delay;
// see the comment above kTinDelay) -- and assert it FAILS.  An explicit
// "expected the guard trips" assertion, never a silently-skipped case.
TEST(FaithfulnessGuardMetaOracle, FiresOnOldCopyFromStepOnePsiInDelayPairing) {
  const GuardResult result =
      run_faithfulness_guard(kTinDelay, "(!k) & G(X(k <-> a))", kPartitionAKO);
  EXPECT_FALSE(result.ok)
      << "faithfulness guard failed to fire on the known-bad delay / "
         "G(X(k<->a)) (copy-from-step-1) pairing -- the guard is a no-op";
}

}  // namespace
