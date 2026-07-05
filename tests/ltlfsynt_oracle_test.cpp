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
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include <sys/wait.h>
#include <unistd.h>

#include <gtest/gtest.h>
#include <spot/tl/parse.hh>

#include "ltlf_ek/variables.hpp"

#ifndef LTLF_EK_SYNTH_BINARY
#error "LTLF_EK_SYNTH_BINARY must be defined by CMake (see CMakeLists.txt)"
#endif

namespace {

using ltlf_ek::collect_aps;

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
// psi_in = (!k) & G(X(k <-> a)) -- weak X (a total strategy imposes safety
// only, must not force continuation; PRD "Behaviour" #4).
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

      // --- Table D: Tin one-step delay, psi_in = (!k) & G(X(k <-> a)) ---
      {"D_XBang_k", kTinDelay, "(!k) & G(X(k <-> a))", "X[!] k", false,
       false},
      {"D_G_a_implies_XBang_k", kTinDelay, "(!k) & G(X(k <-> a))",
       "G(a -> X[!] k)", false, false},
      {"D_F_k", kTinDelay, "(!k) & G(X(k <-> a))", "F(k)", false, false},
      {"D_XBang_XBang_k", kTinDelay, "(!k) & G(X(k <-> a))", "X[!](X[!] k)",
       false, false},
      {"D_k", kTinDelay, "(!k) & G(X(k <-> a))", "k", false, false},
      {"D_G_o_iff_k", kTinDelay, "(!k) & G(X(k <-> a))", "G(o <-> k)", true,
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
// Excluded class (PRD "Excluded class" -- do NOT encode these as a passing
// agreement): a strong-X continuation obligation on the known input, nested
// under an implication, breaks the assumption reduction. Diagnosis: the EK
// problem is REALIZABLE because the system controls termination and the
// (delay) transducer is total, so it continues and supplies k; but as an
// implication antecedent, psi_in is dischargeable exactly at that
// continuation boundary, flipping the reduction's verdict. This is a
// documented soundness boundary of the reduction (PRD "Open theory
// questions"), not a DfaProduct bug. DISABLED_ so it never runs in CI or
// counts toward "ctest green"; enable manually
// (--gtest_also_run_disabled_tests) only to re-confirm the documented
// divergence, never to assert agreement.
// ---------------------------------------------------------------------------

TEST_F(LtlfsyntOracleTest, DISABLED_ExcludedClassStrongXOnKnownInputDiverges) {
  const std::string phi = "X[!](a -> X[!] k)";
  const std::string psi_in = "(!k) & G(X(k <-> a))";
  const ScopedTempFile part_file(kPartFileAKO);
  const ScopedTempFile transducer_file(kTinDelay);

  const CliResult ek =
      RunEkSynth({"--dfa-product", "--formula=" + phi, "--part-file",
                  part_file.path(), "--known-input-transducer",
                  transducer_file.path(), "--realizable"});
  EXPECT_EQ(ParseEkSynthVerdict(ek), Verdict::kRealizable);

  const std::string reduced = "(" + psi_in + ") -> (" + phi + ")";
  const CliResult synt =
      RunLtlfsynt({"--ins=a,k", "--outs=o", "--semantics=Mealy",
                   "--realizability", "-f", reduced});
  EXPECT_EQ(ParseLtlfsyntVerdict(synt), Verdict::kUnrealizable);
}

}  // namespace
