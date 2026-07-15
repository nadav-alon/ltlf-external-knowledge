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

// Pre-2.13 op::strong_X opt-in (docs/prd/generated-corpus-oracle.md "Formula
// generation"): must precede the *first* transitive inclusion of
// <spot/tl/formula.hh> below (e.g. via <spot/tl/parse.hh>), hence this file's
// very first lines.  Since Spot 2.13 strong_X is unconditionally part of the
// public op enum and this define is a no-op there, but it is kept for
// portability to Spot [2.9, 2.13).
#define SPOT_USES_STRONG_X 1

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <cstdint>
#include <limits>
#include <map>
#include <memory>
#include <new>
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
#include <spot/misc/optionmap.hh>
#include <spot/misc/random.hh>
#include <spot/tl/defaultenv.hh>
#include <spot/tl/parse.hh>
#include <spot/tl/randomltl.hh>
#include <spot/twa/bdddict.hh>
#include <spot/twa/formula2bdd.hh>
#include <spot/twaalgos/complete.hh>
#include <spot/twaalgos/isdet.hh>

#include "ltlf_ek/cli.hpp"
#include "ltlf_ek/dfa_product.hpp"
#include "ltlf_ek/ltlf_to_dfa.hpp"
#include "ltlf_ek/mtdfa_product.hpp"
#include "ltlf_ek/output_labeled_transducer.hpp"
#include "ltlf_ek/product.hpp"
#include "ltlf_ek/transducer_io.hpp"
#include "ltlf_ek/turn_order.hpp"
#include "ltlf_ek/variables.hpp"
#include "ltlf_ek/verify_controller.hpp"

#ifndef LTLF_EK_SYNTH_BINARY
#error "LTLF_EK_SYNTH_BINARY must be defined by CMake (see CMakeLists.txt)"
#endif

namespace {

using ltlf_ek::build_product;
using ltlf_ek::build_product_symbolic;
using ltlf_ek::collect_aps;
using ltlf_ek::Controller;
using ltlf_ek::DfaProduct;
using ltlf_ek::LetterAlphabet;
using ltlf_ek::ltlf_to_dfa;
using ltlf_ek::MtdfaProduct;
using ltlf_ek::OutputLabeledTransducer;
using ltlf_ek::parse_transducer;
using ltlf_ek::ProductGuards;
using ltlf_ek::ProductState;
using ltlf_ek::register_turn_order_aps;
using ltlf_ek::Role;
using ltlf_ek::to_guard_map;
using ltlf_ek::Transducer;
using ltlf_ek::trivial_transducer;
using ltlf_ek::VariablePartition;
using ltlf_ek::verify_controller;

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
// Optional-timeout extension (docs/prd/generated-corpus-oracle.md "Subprocess
// timeout"): when `timeout_secs` is set, the command is prefixed with
// coreutils `timeout <N>s ` and a wall-clock kill maps to `*timed_out` (exit
// 124 from `timeout`, or a signalled child) -- otherwise `*timed_out` (if
// non-null) is left false.  Both are defaulted (nullopt / nullptr), so every
// existing call site is untouched by this extension.
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

CliResult RunEkSynth(const std::vector<std::string>& args,
                     std::optional<unsigned> timeout_secs = std::nullopt,
                     bool* timed_out = nullptr) {
  return RunSubprocess(LTLF_EK_SYNTH_BINARY, args, timeout_secs, timed_out);
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

  CliResult RunLtlfsynt(const std::vector<std::string>& args,
                        std::optional<unsigned> timeout_secs = std::nullopt,
                        bool* timed_out = nullptr) {
    return RunSubprocess(ltlfsynt_binary_, args, timeout_secs, timed_out);
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
// \cref{def:consistency}, glossary "Partial transducers -- resolved").  No Ofree
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

// ---------------------------------------------------------------------------
// Generated corpus (docs/prd/generated-corpus-oracle.md): a fixed-seed
// formula / partition generator, graded by the suite's self-labeling
// oracles instead of a hand-authored expected value -- the oracle *is* the
// label.  Landed in phases (PRD "Implementation phases"); Phase 1 landed the
// corpus scaffold plus the ltlf_to_dfa structural free-rider (determinism +
// completeness); Phase 2 grew GeneratedCase to also carry the built random
// Tin and added the synthesize->verify_controller metamorphic round-trip.
// This is **Phase 3**: adds the ltlfsynt differential (gated, V=empty,
// width<=3 subset) plus the RunSubprocess optional-timeout plumbing it needs.
// All test-local, no production C++.
// ---------------------------------------------------------------------------

constexpr unsigned kCorpusSeed = 20260706;     // fixed seed (deterministic).
constexpr std::size_t kCorpusCaseCount = 256;  // corpus size (tunable).

// Wall-clock timeout for each differential subprocess (PRD "Subprocess
// timeout"): with the ≤3 differential width cap timeouts should be rare, but
// a slow ltlfsynt is a skip, never a test failure.
constexpr unsigned kCorpusSubprocessTimeoutSecs = 10;

// Tree-size cap before mandatory trivial simplifications (PRD "Formula
// generation"): kept small (<=~8-10 nodes) so ltlf_to_dfa / ltlfsynt stay
// tractable, while nesting depth >=3 is still reachable.
constexpr int kCorpusTreeSizeMin = 1;
constexpr int kCorpusTreeSizeMax = 10;

// X[!] injection probability (PRD "Formula generation"): the fraction of
// weak-X (op::X) nodes rewritten to strong-X (op::strong_X) after
// generation -- randltl only emits weak X by default, and X[!] is the
// operator that stresses the system-controlled-termination /
// weak-X-at-final-position region (memory
// "ltlf-weak-x-and-termination-semantics").
constexpr double kCorpusStrongXProbability = 0.30;

// Probability that a given input AP is marked Iknown when building a random
// partition (PRD "Partition generation": "a random subset of the inputs is
// marked Iknown, may be empty"); a plain per-element coin flip -- the PRD
// does not pin this exact probability, only that the subset may be empty.
constexpr double kCorpusIknownProbability = 0.5;

// |I| / |O| upper bounds for random_partition (docs/prd/generated-corpus-
// soak-mode.md "Configuration struct"), extracted from the formerly-inline
// input_count(1,5)/output_count(0,5) literals so CorpusConfig has a default
// to fall back to.
constexpr int kCorpusInputMax = 5;
constexpr int kCorpusOutputMax = 5;

// random_tin's state-count upper bound (soak-mode PRD "Configuration
// struct"), extracted from the formerly-inline state_count(1,3) literal.
constexpr int kCorpusTinStatesMax = 3;

// GeneratedCorpusDifferential's |I union O| cap (soak-mode PRD
// "Configuration struct"), extracted from the formerly-inline
// `if (width > 3) continue;`.
constexpr std::size_t kCorpusDiffWidthCap = 3;

// Escalation-ladder ceilings (soak-mode PRD "Escalation ladder", "Width
// ceilings guard against OOM, not just time"): random_tin enumerates
// 2^|Ifree| cubes and build_product materialises 2^|I union O| letters, so
// an unbounded width climb crashes on memory before the soak timer fires.
// kCorpusWidthCeiling bounds |I| (hence |Ifree|); kCorpusUnionCeiling bounds
// |I union O| (the joint clamp in random_partition, below);
// kCorpusDiffWidthSoakCap is the ladder's ceiling on diff_width_cap, distinct
// from kCorpusDiffWidthCap (the level-0/default cap).  Values (10/12/5) and
// their lowering from the PRD-pinned 12/16 are recorded in
// docs/prd/generated-corpus-soak-mode.md "Developer comments / PRD
// disagreements" (2026-07-12); they back-stop, and do not replace, the soak
// loop's per-case bad_alloc catch below.
constexpr int kCorpusWidthCeiling = 10;
constexpr std::size_t kCorpusUnionCeiling = 12;
constexpr std::size_t kCorpusDiffWidthSoakCap = 5;

// random_partition draws |O| from [0, kCorpusUnionCeiling - |I|]; keeping the
// |I| ceiling strictly below the union ceiling guarantees that upper bound
// stays >= 1, so std::uniform_int_distribution never sees a negative range
// (which would be UB).  A future edit raising kCorpusWidthCeiling must not
// cross kCorpusUnionCeiling.
static_assert(kCorpusWidthCeiling < static_cast<int>(kCorpusUnionCeiling),
              "width ceiling must stay < union ceiling so random_partition's "
              "|O| range is non-negative");

// CorpusConfig (docs/prd/generated-corpus-soak-mode.md "Configuration
// struct"): every generated-corpus tunable, defaulting to the constants
// above. Phase 1 only: the per-knob env-override / replay layer -- no soak
// switch, no ladder (that is Phase 2).
struct CorpusConfig {
  unsigned seed = kCorpusSeed;
  std::size_t case_count = kCorpusCaseCount;
  int tree_size_min = kCorpusTreeSizeMin;
  int tree_size_max = kCorpusTreeSizeMax;
  int input_max = kCorpusInputMax;
  int output_max = kCorpusOutputMax;
  int tin_states_max = kCorpusTinStatesMax;
  std::size_t diff_width_cap = kCorpusDiffWidthCap;
  unsigned subprocess_timeout_secs = kCorpusSubprocessTimeoutSecs;
};

// EnvInt (soak-mode PRD "Env readers"): reads env var `name` as an integer
// (std::stoll); returns std::nullopt when the var is unset (so the caller
// keeps its default), the parsed value when present and in
// [min_value, max_value], or throws std::runtime_error naming `name` on a
// non-integer, trailing garbage, or an out-of-range value (loud-on-malformed,
// never a silent fallback -- soak-mode PRD "Parse failure is loud, never
// silent").
std::optional<long long> EnvInt(const char* name, long long min_value,
                                long long max_value) {
  const char* raw = std::getenv(name);
  if (raw == nullptr) return std::nullopt;
  const std::string text(raw);
  std::size_t consumed = 0;
  long long parsed = 0;
  try {
    parsed = std::stoll(text, &consumed);
  } catch (const std::exception&) {
    throw std::runtime_error(std::string("malformed env var ") + name +
                             "=\"" + text + "\": not an integer");
  }
  if (consumed != text.size())
    throw std::runtime_error(std::string("malformed env var ") + name +
                             "=\"" + text + "\": not an integer");
  if (parsed < min_value || parsed > max_value)
    throw std::runtime_error(std::string("env var ") + name + "=\"" + text +
                             "\" out of range");
  return parsed;
}

// corpus_config_from_env (soak-mode PRD "Configuration struct"): reads the
// LTLF_EK_CORPUS_* env vars over the CorpusConfig defaults; each unset var
// keeps its default, each malformed/negative/zero (or, for tree_size_min >
// tree_size_max, inconsistent) value throws std::runtime_error naming the
// var -- never a silent fallback.
CorpusConfig corpus_config_from_env() {
  CorpusConfig cfg;
  constexpr long long kIntMax = std::numeric_limits<int>::max();

  if (auto v = EnvInt("LTLF_EK_CORPUS_SEED", 0,
                      std::numeric_limits<unsigned>::max()))
    cfg.seed = static_cast<unsigned>(*v);
  if (auto v = EnvInt("LTLF_EK_CORPUS_CASES", 1, kIntMax))
    cfg.case_count = static_cast<std::size_t>(*v);
  if (auto v = EnvInt("LTLF_EK_CORPUS_TREE_MIN", 1, kIntMax))
    cfg.tree_size_min = static_cast<int>(*v);
  if (auto v = EnvInt("LTLF_EK_CORPUS_TREE_MAX", 1, kIntMax))
    cfg.tree_size_max = static_cast<int>(*v);

  if (cfg.tree_size_min > cfg.tree_size_max)
    throw std::runtime_error(
        "LTLF_EK_CORPUS_TREE_MIN must be <= LTLF_EK_CORPUS_TREE_MAX");

  if (auto v = EnvInt("LTLF_EK_CORPUS_INPUT_MAX", 1, kIntMax))
    cfg.input_max = static_cast<int>(*v);
  if (auto v = EnvInt("LTLF_EK_CORPUS_OUTPUT_MAX", 1, kIntMax))
    cfg.output_max = static_cast<int>(*v);
  if (auto v = EnvInt("LTLF_EK_CORPUS_STATES_MAX", 1, kIntMax))
    cfg.tin_states_max = static_cast<int>(*v);
  if (auto v = EnvInt("LTLF_EK_CORPUS_DIFF_WIDTH", 1, kIntMax))
    cfg.diff_width_cap = static_cast<std::size_t>(*v);
  if (auto v = EnvInt("LTLF_EK_CORPUS_TIMEOUT", 1, kIntMax))
    cfg.subprocess_timeout_secs = static_cast<unsigned>(*v);

  return cfg;
}

// env_soak_secs (soak-mode PRD "Soak switch and budget"): LTLF_EK_SOAK is
// both the soak switch and the wall-clock budget in seconds; unset or `0`
// means soak off (a single fast BuildGeneratedCorpus pass, run_corpus
// below).  Reuses EnvInt so a malformed value (non-integer, negative)
// throws std::runtime_error loudly rather than silently running the default
// corpus and masquerading as a pass.
unsigned env_soak_secs() {
  if (auto v = EnvInt("LTLF_EK_SOAK", 0, std::numeric_limits<int>::max()))
    return static_cast<unsigned>(*v);
  return 0;
}

// ladder (soak-mode PRD "Escalation ladder"): derives level L's CorpusConfig
// from the env-base config B by the pinned monotone schedule.  tree_size_max
// grows unbounded (+3 per level, no ceiling -- deeper formulas are the
// budget's main lever once width saturates); input_max/output_max/
// diff_width_cap saturate at their ceilings so a long soak never OOMs (the
// output_max/|I|-drawn interaction is realized by random_partition's joint
// width clamp, not here -- |I| is a per-case draw, not known at this
// per-level config-construction point).  tree_size_min, subprocess_timeout,
// case_count, and seed are left at B's values; run_corpus overwrites seed
// with the level's fresh mt19937 draw.
CorpusConfig ladder(const CorpusConfig& base, unsigned level) {
  // All additions are done in long long before the min-clamp so a pathological
  // env-supplied base max (corpus_config_from_env allows up to INT_MAX) cannot
  // signed-overflow before it is clamped -- the ceiling'd fields clamp to their
  // ceiling, the unbounded fields (tree_size_max, tin_states_max) saturate at
  // INT_MAX.
  const long long l = level;
  constexpr long long kIntMax = std::numeric_limits<int>::max();
  CorpusConfig cfg = base;
  cfg.tree_size_max =
      static_cast<int>(std::min(base.tree_size_max + 3 * l, kIntMax));
  cfg.input_max =
      static_cast<int>(std::min(base.input_max + l,
                                static_cast<long long>(kCorpusWidthCeiling)));
  cfg.output_max =
      static_cast<int>(std::min(base.output_max + l,
                                static_cast<long long>(kCorpusUnionCeiling)));
  cfg.tin_states_max =
      static_cast<int>(std::min(base.tin_states_max + l, kIntMax));
  cfg.diff_width_cap = static_cast<std::size_t>(
      std::min(static_cast<long long>(base.diff_width_cap) + l,
               static_cast<long long>(kCorpusDiffWidthSoakCap)));
  return cfg;
}

// GeneratedCase (PRD "Interfaces & types", Phase 2 shape): now also carries
// the built random Tin, on its own private bdd_dict (one dict per case, like
// the structural test's per-case dict) so the metamorphic body can replay
// synthesize/verify_controller on the identical (phi, partition, t_in)
// without rebuilding anything.
struct GeneratedCase {
  spot::formula phi;
  VariablePartition partition;
  OutputLabeledTransducer t_in;
};

// strengthen_next (PRD "Formula generation"): recursively rewrite every
// op::X node in `f` to op::strong_X (X[!]) with probability
// kCorpusStrongXProbability, drawn from `rng` so the rewrite is seeded and
// reproducible.  Both ltlf-ek-synth and ltlfsynt accept weak X and X[!], so
// this only shifts the generated distribution, never an encoding hazard.
// Uses spot::formula::map (bottom-up: children are rewritten first, so a
// nested X[X a] can independently promote either/both occurrences).
spot::formula strengthen_next(spot::formula f, std::mt19937& rng) {
  spot::formula mapped = f.map(
      [&](spot::formula child) { return strengthen_next(child, rng); });
  std::bernoulli_distribution flip(kCorpusStrongXProbability);
  if (mapped.is(spot::op::X) && flip(rng))
    return spot::formula::unop(spot::op::strong_X, mapped[0]);
  return mapped;
}

// random_partition (PRD "Partition generation"): draws |I| in
// [1, config.input_max], |O| in [0, config.output_max] (0 hits the
// empty-Ofree edge case), fresh AP names p0, p1, ... up to |I \cup O|, and
// marks a random subset of the inputs Iknown (may be empty =
// degenerate empty-knowledge).  Oknown = empty always in v1 (Tout stays
// trivial, PRD "Partition generation").  Phase 1 (soak-mode PRD): the
// [1,5]/[0,5] literals now come from CorpusConfig::input_max/output_max
// (defaulting to kCorpusInputMax/kCorpusOutputMax).  Phase 2 (soak-mode PRD
// "Joint width clamp (pinned draw order)"): |I| is drawn first, capped at
// kCorpusWidthCeiling regardless of config.input_max, then |O| is drawn
// second, capped at kCorpusUnionCeiling minus the just-drawn |I|, so
// |I union O| <= kCorpusUnionCeiling and |Ifree| <= kCorpusWidthCeiling
// always hold under soak.  At the default level-0 ranges (5/5) both clamps
// are inert (min(5,12)=5, min(5,16-|I|)=5 since |I|<=5) so the mt19937 draw
// order and the non-soak corpus are unchanged from Phase 1.
VariablePartition random_partition(std::mt19937& rng,
                                   const CorpusConfig& config) {
  const int input_upper = std::min(config.input_max, kCorpusWidthCeiling);
  std::uniform_int_distribution<int> input_count(1, input_upper);
  const int n_inputs = input_count(rng);
  const int output_upper = std::max(
      0, std::min(config.output_max,
                  static_cast<int>(kCorpusUnionCeiling) - n_inputs));
  std::uniform_int_distribution<int> output_count(0, output_upper);
  const int n_outputs = output_count(rng);

  std::set<std::string> inputs, outputs;
  int next_id = 0;
  for (int i = 0; i < n_inputs; ++i)
    inputs.insert("p" + std::to_string(next_id++));
  for (int i = 0; i < n_outputs; ++i)
    outputs.insert("p" + std::to_string(next_id++));

  std::bernoulli_distribution is_known(kCorpusIknownProbability);
  std::set<std::string> governed;
  for (const std::string& name : inputs)
    if (is_known(rng)) governed.insert(name);

  return VariablePartition::split(inputs, outputs, governed);
}

// ---------------------------------------------------------------------------
// Ladder monotonicity + clamp (soak-mode PRD "Test oracles" #3): a cheap,
// pure-function unit test -- no formula generation (so none of
// generate_random_formula's Spot process-global-RNG/apid-recycling hazards
// documented above), no synthesis. Runs on the default ctest path.
// ---------------------------------------------------------------------------

// ladder(base, L) is non-decreasing in L for every ramped field, holds every
// unramped field fixed at base's value (PRD "Escalation ladder": "tree_size_
// min, subprocess_timeout, case_count = B.* (unchanged per level)"), and
// never exceeds the pinned ceilings (input_max <= kCorpusWidthCeiling,
// output_max <= kCorpusUnionCeiling, diff_width_cap <=
// kCorpusDiffWidthSoakCap) -- soak-mode PRD "Width ceilings guard against
// OOM, not just time".
TEST(Ladder, MonotoneAndClampedAcrossLevels) {
  const CorpusConfig base;  // CorpusConfig{} defaults (kCorpusSeed etc.).

  int prev_tree_max = base.tree_size_max;
  int prev_input_max = base.input_max;
  int prev_output_max = base.output_max;
  int prev_states_max = base.tin_states_max;
  std::size_t prev_diff_width = base.diff_width_cap;

  // 0..40 comfortably runs input_max/output_max/diff_width_cap past their
  // ceilings (reached by level ~7-13 from the defaults) while tree_size_max
  // and tin_states_max keep climbing unbounded, per the ladder schedule.
  for (unsigned level = 0; level <= 40; ++level) {
    SCOPED_TRACE("level=" + std::to_string(level));
    const CorpusConfig cfg = ladder(base, level);

    // Non-decreasing in L for every ramped field.
    EXPECT_GE(cfg.tree_size_max, prev_tree_max);
    EXPECT_GE(cfg.input_max, prev_input_max);
    EXPECT_GE(cfg.output_max, prev_output_max);
    EXPECT_GE(cfg.tin_states_max, prev_states_max);
    EXPECT_GE(cfg.diff_width_cap, prev_diff_width);

    // Ceilings never exceeded.
    EXPECT_LE(cfg.input_max, kCorpusWidthCeiling);
    EXPECT_LE(cfg.output_max, static_cast<int>(kCorpusUnionCeiling));
    EXPECT_LE(cfg.diff_width_cap, kCorpusDiffWidthSoakCap);

    // Unramped fields left at base's value (ladder itself; run_corpus is the
    // one that overwrites seed with the level's fresh mt19937 draw).
    EXPECT_EQ(cfg.seed, base.seed);
    EXPECT_EQ(cfg.tree_size_min, base.tree_size_min);
    EXPECT_EQ(cfg.case_count, base.case_count);
    EXPECT_EQ(cfg.subprocess_timeout_secs, base.subprocess_timeout_secs);

    prev_tree_max = cfg.tree_size_max;
    prev_input_max = cfg.input_max;
    prev_output_max = cfg.output_max;
    prev_states_max = cfg.tin_states_max;
    prev_diff_width = cfg.diff_width_cap;
  }

  // The ceilings are actually reached (not just never-exceeded vacuously) by
  // level 40 from the default base -- otherwise the LE checks above would be
  // trivially true without ever exercising the min()-clamp branch.
  const CorpusConfig high = ladder(base, 40);
  EXPECT_EQ(high.input_max, kCorpusWidthCeiling);
  EXPECT_EQ(high.output_max, static_cast<int>(kCorpusUnionCeiling));
  EXPECT_EQ(high.diff_width_cap, kCorpusDiffWidthSoakCap);
}

// The joint width clamp (soak-mode PRD "Joint width clamp (pinned draw
// order)"): a partition drawn by random_partition under a high-level,
// ceiling-saturated ladder config still respects |I| <=
// kCorpusWidthCeiling and |I union O| <= kCorpusUnionCeiling -- the property
// the clamp exists to guarantee, checked against the actual draw rather than
// just the config's declared maxes. random_partition alone (no formula
// generation) is deterministic pure-function machinery -- fixed-seed
// std::mt19937, no Spot RNG involved -- so this is safe on the default path.
TEST(Ladder, DrawnPartitionRespectsWidthCeilingsAtHighLevels) {
  const CorpusConfig base;
  std::mt19937 rng(kCorpusSeed);
  for (const unsigned level : {0u, 1u, 5u, 20u, 100u}) {
    const CorpusConfig cfg = ladder(base, level);
    for (int trial = 0; trial < 25; ++trial) {
      SCOPED_TRACE("level=" + std::to_string(level) +
                   " trial=" + std::to_string(trial));
      const VariablePartition p = random_partition(rng, cfg);
      EXPECT_LE(p.inputs().size(),
               static_cast<std::size_t>(kCorpusWidthCeiling));
      EXPECT_LE(p.universe().size(), kCorpusUnionCeiling);
    }
  }
}

// randltlgenerator wrapper (PRD "Formula generation"): thin wrapper over
// Spot's own spot::randltlgenerator (the class backing the `randltl`
// binary; not hand-rolled).  APs come from `partition`'s exact I \cup O
// set, not randltl's default p0..., so every generated phi's APs are a
// subset of I \cup O by construction (partition-first generation) -- no
// separate AP-scope guard is needed for generated cases.  Operator palette
// restricted to the LTLf-safe set (U, R, W, F, G, X, !, &, |, ->, <->):
// xor and M (strong release) are disabled via priorities; strongX stays at
// its library default of 0 -- X[!] is injected afterwards by
// strengthen_next, not by randltl itself.  Phase 1 (soak-mode PRD):
// tree_size_min/max now come from CorpusConfig (defaulting to
// kCorpusTreeSizeMin/Max).
spot::formula generate_random_formula(const VariablePartition& partition,
                                      std::mt19937& rng,
                                      const CorpusConfig& config) {
  std::set<std::string> ap_names = partition.inputs();
  for (const std::string& name : partition.outputs()) ap_names.insert(name);

  spot::atomic_prop_set aprops;
  for (const std::string& name : ap_names)
    aprops.insert(spot::default_environment::instance().require(name));

  spot::option_map opts;
  opts.set("output", spot::randltlgenerator::LTL);
  opts.set("tree_size_min", config.tree_size_min);
  opts.set("tree_size_max", config.tree_size_max);
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
        "generate_random_formula: randltlgenerator produced no formula");
  return phi;
}

// random_tin (PRD "Random Tin generation"): an in-memory OutputLabeledTransducer
// built directly on `dict`, deterministic + total by construction (the
// committed Case-A regime, \cref{def:consistency}) -- no validity check needed
// afterward.  Role t_in => Sigma0 = Ifree, Sigma1 = Iknown (glossary "Role").
// Degenerate empty-Iknown case: trivial_transducer instead of a random table
// (PRD "Edge cases" "Empty Iknown").  Phase 1 (soak-mode PRD): the
// state_count(1,3) literal now comes from config.tin_states_max (defaulting
// to kCorpusTinStatesMax).
OutputLabeledTransducer random_tin(const VariablePartition& partition,
                                   std::mt19937& rng,
                                   const spot::bdd_dict_ptr& dict,
                                   const CorpusConfig& config) {
  // Dict-setup site (docs/prd/mtdfa-product.md "Test oracles" #3:
  // "register_turn_order_aps must be called at every dict-setup site in
  // tests too, not just the CLI"): establishes Ifree (sorted) < Iknown <
  // Ofree < Oknown on `dict` BEFORE any other registration, on EITHER
  // branch below.  Identical variable numbering to the four manual
  // register_ap loops this replaces (same order: input_free, input_known,
  // output_free, output_known), so the existing byte-identical golden
  // corpus (GeneratedCorpus.DefaultCorpusIsByteIdenticalToGolden) is
  // unaffected -- register_ap is idempotent, so the loops below just look
  // up the same variable numbers register_turn_order_aps already assigned.
  register_turn_order_aps(partition, dict);

  if (partition.input_known.empty())
    return trivial_transducer(partition, Role::t_in, dict);

  auto g = spot::make_twa_graph(dict);
  // Register every I \cup O AP on this dict (PRD pseudocode), even the ones
  // delta/lambda never look at (Ofree, Oknown) -- ltlf_to_dfa/synthesize will
  // register phi's own APs on this same dict later, by name.
  std::vector<int> ifree_vars, iknown_vars;
  for (const std::string& n : partition.input_free)
    ifree_vars.push_back(g->register_ap(n));
  for (const std::string& n : partition.input_known)
    iknown_vars.push_back(g->register_ap(n));
  for (const std::string& n : partition.output_free) g->register_ap(n);
  for (const std::string& n : partition.output_known) g->register_ap(n);

  std::uniform_int_distribution<int> state_count(1, config.tin_states_max);
  const int n = state_count(rng);
  g->new_states(n);
  g->set_init_state(0);

  const std::vector<bdd> ifree_letters = all_letters_over(ifree_vars);
  std::uniform_int_distribution<int> dst_dist(0, n - 1);
  std::bernoulli_distribution bit(0.5);
  std::vector<bdd> lambda_by_state(n, bddfalse);
  for (int q = 0; q < n; ++q) {
    bdd lambda_q = bddfalse;
    for (const bdd& ifree_cube : ifree_letters) {
      // delta: one edge per Ifree-cube -- mutually exclusive, exhaustive
      // guards, so delta is deterministic + total over Ifree.
      g->new_edge(q, dst_dist(rng), ifree_cube);
      // lambda: a full Iknown assignment OR'd in per Ifree-cube, so
      // lambda(q, .) is a total function Ifree -> 2^Iknown.
      bdd iknown_cube = bddtrue;
      for (int x : iknown_vars) iknown_cube &= bit(rng) ? bdd_ithvar(x) : bdd_nithvar(x);
      lambda_q |= ifree_cube & iknown_cube;
    }
    lambda_by_state[q] = lambda_q;
  }

  bdd sigma0_cube = bddtrue;
  for (int x : ifree_vars) sigma0_cube &= bdd_ithvar(x);
  bdd sigma1_cube = bddtrue;
  for (int x : iknown_vars) sigma1_cube &= bdd_ithvar(x);
  return OutputLabeledTransducer(g, std::move(lambda_by_state), sigma0_cube,
                                 sigma1_cube);
}

// GenerateOneCase (soak-driver OOM fix, 2026-07-12:
// docs/prd/generated-corpus-soak-mode.md "Developer comments / PRD
// disagreements"): the per-case body factored out of BuildGeneratedCorpus's
// loop (random_partition -> generate_random_formula -> strengthen_next ->
// random_tin), so run_corpus's soak branch can generate-and-consume one case
// at a time -- checking the wall-clock deadline before each draw -- instead
// of building an entire per-level 256-case corpus up front (the original bug:
// one high-level BuildGeneratedCorpus call ran unbounded between deadline
// checks and could OOM before the timer ever fired). BuildGeneratedCorpus
// itself calls this once per case too, so the std::mt19937 draw order per
// (config, seed) is unchanged and the byte-identical default-corpus golden
// still holds.
GeneratedCase GenerateOneCase(std::mt19937& rng, const CorpusConfig& config) {
  VariablePartition partition = random_partition(rng, config);
  spot::formula phi = strengthen_next(
      generate_random_formula(partition, rng, config), rng);
  const spot::bdd_dict_ptr dict = spot::make_bdd_dict();
  OutputLabeledTransducer t_in = random_tin(partition, rng, dict, config);
  return {phi, std::move(partition), std::move(t_in)};
}

// BuildGeneratedCorpus (PRD "Determinism / seeding"): one seeded
// std::mt19937(config.seed), no reserved draw slots.  Emits config.case_count
// cases partition-first, so
// every phi's APs are in-partition by construction; each case's random Tin
// is built on its own private bdd_dict.  Phase 1 (soak-mode PRD
// "Configuration struct"): takes an explicit CorpusConfig (no default
// argument -- callers, including a future ladder, pass it explicitly) and
// threads it into random_partition / generate_random_formula / random_tin,
// preserving the exact std::mt19937 draw order (random_partition ->
// generate_random_formula -> strengthen_next -> random_tin, per case) so
// the default (all-env-unset) corpus stays byte-identical to pre-Phase-1.
// Unchanged by the soak-driver OOM fix: builds via GenerateOneCase (above)
// but is still called by the soak==0 fast path and the golden tests exactly
// as before, up front, with no deadline/OOM handling of its own.
std::vector<GeneratedCase> BuildGeneratedCorpus(const CorpusConfig& config) {
  std::mt19937 rng(config.seed);
  std::vector<GeneratedCase> corpus;
  corpus.reserve(config.case_count);
  for (std::size_t i = 0; i < config.case_count; ++i)
    corpus.push_back(GenerateOneCase(rng, config));
  return corpus;
}

// RunCorpusStats: bookkeeping returned by run_corpus (soak-mode PRD
// "Reproducibility of a soak run", "Skip/level accounting") so each body can
// RecordProperty("levels_reached", ...) / ("cases_run", ...).
// cases_skipped (soak-driver OOM fix, 2026-07-12: docs/prd/generated-corpus-
// soak-mode.md "Developer comments / PRD disagreements") is a PRD-unnamed
// addition, same rationale as levels_reached/cases_run: a case whose
// GenerateOneCase draw throws std::bad_alloc under soak is skipped, not
// fatal, and this counter makes that visible via RecordProperty rather than
// silently swallowed.
struct RunCorpusStats {
  unsigned levels_reached = 0;
  std::size_t cases_run = 0;
  std::size_t cases_skipped = 0;
};

// run_corpus (soak-mode PRD "The three bodies under soak"): the shared
// level/deadline driver factored out so it is not triplicated across the
// three bodies.  `per_case(const GeneratedCase&, const CorpusConfig& cfg,
// unsigned level, std::size_t index)` supplies the body's own unchanged
// per-case assertions.
//
// soak == 0 (LTLF_EK_SOAK unset/`0`): exactly Phase 1's single
// BuildGeneratedCorpus(base) pass -- byte-identical to Phase 1, including no
// spot::srand call here (that would perturb Spot's process-global RNG ahead
// of the very first randltlgenerator construction and risk shifting the
// byte-identical golden; the fast path must reproduce Phase 1 exactly, per
// the soak-mode PRD's Phase 2 green checkpoint).
//
// soak > 0: escalates level L = 0, 1, 2, ... until `deadline` (a soft
// deadline: the case in flight when it passes always finishes).  A single
// std::mt19937 seed_rng(base.seed) draws each level's seed as the L-th
// successive draw (not base.seed + L, to avoid low-bit correlation between
// adjacent levels), so the whole soak run is reproducible from base.seed.
//
// Generate-and-consume, one case at a time (soak-driver OOM fix, 2026-07-12:
// docs/prd/generated-corpus-soak-mode.md "Developer comments / PRD
// disagreements"): the level loop does NOT call BuildGeneratedCorpus(cfg) to
// materialise all cfg.case_count cases up front -- that ran the entire
// per-level corpus build unbounded between deadline checks, so one
// high-level build (large |Ifree|, growing Tin state count) could run past
// the wall-clock budget and exhaust memory before any check fired, even
// though the ceilings bound a single case's width. Instead each case is
// drawn on demand via GenerateOneCase, with the deadline checked
// immediately before every draw (not just between levels), and a per-case
// std::bad_alloc is caught and counted as a skip rather than aborting the
// body -- an over-large draw degrades gracefully instead of crashing the
// process.
//
// Spot global-RNG reset (soak-mode PRD "Fresh seed per level" warning):
// spot::randltlgenerator's "seed" option seeds Spot's process-*global* RNG
// (spot::srand), which a same-seed re-construction in the same process does
// not cleanly reset (empirically confirmed -- see the kGoldenSeed1Corpus*
// comment above).  run_corpus builds many corpora per process, so calling
// spot::srand(cfg.seed) immediately before each per-level BuildGeneratedCorpus
// resets that one mechanism.  IMPORTANT: this reset is necessary but *not*
// sufficient for full in-process, level-to-level reproducibility -- a deeper
// Spot mechanism (reference-counted/apid-recycling hash-consing of atomic
// props, spot/tl/formula.cc fnode::ap) can still make two same-seed
// in-process builds diverge (empirically confirmed by the Phase 2 developer;
// see docs/prd/generated-corpus-soak-mode.md "Developer comments / PRD
// disagreements", 2026-07-12, for the full repro and root cause). This does
// NOT affect the documented per-case replay recipe (fresh process, single
// BuildGeneratedCorpus call, LTLF_EK_SOAK unset) -- only repeated in-process
// rebuilds with the same seed are at risk. Do not write a "same cfg.seed
// twice in one process" unit test against run_corpus; it would be flaky.
template <class PerCase>
RunCorpusStats run_corpus(const CorpusConfig& base, PerCase&& per_case) {
  RunCorpusStats stats;
  const unsigned soak = env_soak_secs();
  if (soak == 0) {
    const std::vector<GeneratedCase> corpus = BuildGeneratedCorpus(base);
    for (std::size_t i = 0; i < corpus.size(); ++i) {
      per_case(corpus[i], base, /*level=*/0u, i);
      ++stats.cases_run;
    }
    stats.levels_reached = 1;
    return stats;
  }

  const auto deadline =
      std::chrono::steady_clock::now() + std::chrono::seconds(soak);
  std::mt19937 seed_rng(base.seed);
  for (unsigned level = 0; std::chrono::steady_clock::now() < deadline;
       ++level) {
    CorpusConfig cfg = ladder(base, level);
    cfg.seed = seed_rng();
    spot::srand(cfg.seed);  // Load-bearing: see comment above.
    std::mt19937 rng(cfg.seed);
    stats.levels_reached = level + 1;
    for (std::size_t i = 0; i < cfg.case_count; ++i) {
      if (std::chrono::steady_clock::now() >= deadline) break;
      // Deadline is checked before generating (not just before consuming) --
      // this is the fix: a single case's generation is the unbounded step
      // that could OOM, so it must never run outside a deadline check.
      try {
        GeneratedCase generated = GenerateOneCase(rng, cfg);
        per_case(generated, cfg, level, i);
        ++stats.cases_run;
      } catch (const std::bad_alloc&) {
        // An over-large draw degrades to a skip, not a crash (soak-driver
        // OOM fix, above). The mt19937 stream is left wherever the failed
        // draw advanced it to; per-run reproducibility is already accepted
        // as impossible under soak (PRD "Edge cases" "Per-run
        // non-reproducibility"), so this does not add a new hazard.
        ++stats.cases_skipped;
      }
    }
  }
  return stats;
}

// Renders a VariablePartition's four sets for SCOPED_TRACE (PRD "Behaviour"
// #1: "SCOPED_TRACE printing the offending phi + partition + case index").
std::string DescribeGeneratedPartition(const VariablePartition& p) {
  auto describe_set = [](const std::set<std::string>& s) {
    std::string out;
    for (const std::string& name : s) out += name + " ";
    return out;
  };
  std::ostringstream os;
  os << "input_free={" << describe_set(p.input_free) << "} "
     << "input_known={" << describe_set(p.input_known) << "} "
     << "output_free={" << describe_set(p.output_free) << "} "
     << "output_known={" << describe_set(p.output_known) << "}";
  return os.str();
}

// Renders a CorpusConfig's seed + range knobs for SCOPED_TRACE (soak-mode
// PRD "Reproducibility of a soak run": the replay recipe needs the seed and
// per-knob maxes a failing case ran under; Phase 1 prints them even for the
// single non-soak pass so the trace format is stable across both phases).
std::string DescribeCorpusConfig(const CorpusConfig& config) {
  std::ostringstream os;
  os << "seed=" << config.seed << " cases=" << config.case_count
     << " tree_min=" << config.tree_size_min
     << " tree_max=" << config.tree_size_max
     << " input_max=" << config.input_max
     << " output_max=" << config.output_max
     << " states_max=" << config.tin_states_max
     << " diff_width=" << config.diff_width_cap
     << " timeout=" << config.subprocess_timeout_secs;
  return os.str();
}

// ---------------------------------------------------------------------------
// Phase 1 harness tests (docs/prd/generated-corpus-soak-mode.md "Test oracles
// (for /test-writer)"): these exercise the *driver* (CorpusConfig,
// corpus_config_from_env, the config-parameterized BuildGeneratedCorpus)
// added by Phase 1, not new synthesis behaviour. Two properties, per the
// PRD: (1) the byte-identical default -- the critical guard that proves the
// constexpr->struct refactor did not shift the mt19937 draw order; (2) the
// per-knob env-override plumbing, including loud-on-malformed. Phase 2's
// ladder monotonicity/clamp test and the LTLF_EK_SOAK soak smoke test are
// out of scope here -- that code does not exist yet.
// ---------------------------------------------------------------------------

// ScopedEnvVar: sets (or, with value == nullptr, unsets) an environment
// variable for the lifetime of the guard and restores its prior value (or
// absence) on destruction, so LTLF_EK_CORPUS_* overrides in one test never
// leak into the next (PRD "Env override plumbing": "Set/unset env within
// the test via setenv/unsetenv and restore afterward").
class ScopedEnvVar {
 public:
  ScopedEnvVar(const char* name, const char* value) : name_(name) {
    const char* prev = std::getenv(name);
    had_prev_ = prev != nullptr;
    if (had_prev_) prev_value_ = prev;
    if (value == nullptr)
      unsetenv(name);
    else
      setenv(name, value, /*overwrite=*/1);
  }
  ScopedEnvVar(const ScopedEnvVar&) = delete;
  ScopedEnvVar& operator=(const ScopedEnvVar&) = delete;
  ~ScopedEnvVar() {
    if (had_prev_)
      setenv(name_.c_str(), prev_value_.c_str(), /*overwrite=*/1);
    else
      unsetenv(name_.c_str());
  }

 private:
  std::string name_;
  bool had_prev_;
  std::string prev_value_;
};

// Every LTLF_EK_CORPUS_* knob name (soak-mode PRD "Configuration struct",
// "Env var names"), used to force a clean (fully-unset) environment before
// asserting the byte-identical default -- ambient env in a soaker's shell
// must not leak into the fast-gate guard.
constexpr const char* kAllCorpusEnvVars[] = {
    "LTLF_EK_CORPUS_SEED",  "LTLF_EK_CORPUS_CASES",
    "LTLF_EK_CORPUS_TREE_MIN", "LTLF_EK_CORPUS_TREE_MAX",
    "LTLF_EK_CORPUS_INPUT_MAX", "LTLF_EK_CORPUS_OUTPUT_MAX",
    "LTLF_EK_CORPUS_STATES_MAX", "LTLF_EK_CORPUS_DIFF_WIDTH",
    "LTLF_EK_CORPUS_TIMEOUT"};

// Builds an RAII bundle (one guard per knob, heap-held since ScopedEnvVar is
// non-movable) that unsets every corpus knob for the caller's scope; returned
// by value so the guards live as long as the caller needs them.
std::vector<std::unique_ptr<ScopedEnvVar>> UnsetAllCorpusEnvVars() {
  std::vector<std::unique_ptr<ScopedEnvVar>> guards;
  for (const char* name : kAllCorpusEnvVars)
    guards.push_back(std::make_unique<ScopedEnvVar>(name, nullptr));
  return guards;
}

// A portable, order-sensitive 64-bit FNV-1a fold over a generated corpus's
// (phi, partition) stream -- a stable stand-in for "the whole 256-case
// stream" so the byte-identical guard does not string-compare 256 cases by
// hand (PRD "Test oracles": "a stable checksum over the streamed cases").
// Deliberately NOT std::hash, whose output is implementation-defined -- a
// golden captured under one stdlib would false-fail under another and invite
// a spurious re-capture that masks real draw-order drift; FNV-1a is fixed
// across compilers/stdlibs so the golden stays a genuine drift detector.
std::uint64_t ChecksumGeneratedCorpus(const std::vector<GeneratedCase>& corpus) {
  std::uint64_t hash = 14695981039346656037ull;  // FNV-1a 64-bit offset basis
  for (const GeneratedCase& c : corpus) {
    std::ostringstream phi_os;
    phi_os << c.phi;
    const std::string s = phi_os.str() + DescribeGeneratedPartition(c.partition);
    for (unsigned char byte : s) {
      hash ^= byte;
      hash *= 1099511628211ull;  // FNV-1a 64-bit prime
    }
  }
  return hash;
}

// Golden values captured from the actual just-landed default corpus (PRD
// "Golden values": "the PRD leaves the exact golden open" -- captured here
// by running BuildGeneratedCorpus(corpus_config_from_env()) with no env set
// on the landed Phase 1 tree). A later mismatch means the mt19937 draw order
// drifted -- "investigate, don't adjust" (PRD "Edge cases" / repo-wide rule),
// never silently re-capture to make the test pass.
const std::string kGoldenDefaultCorpusPhi0 = "1";
const std::string kGoldenDefaultCorpusPartition0 =
    "input_free={p0 p1 p2 p3 } input_known={} output_free={p4 } "
    "output_known={}";
constexpr std::uint64_t kGoldenDefaultCorpusChecksum = 6327196066608144121ull;

// Byte-identical default (soak-mode PRD "Test oracles" #1, "the critical
// guard"): with no LTLF_EK_CORPUS_* env set, corpus_config_from_env() must
// return the plain CorpusConfig{} defaults, and BuildGeneratedCorpus on those
// defaults must reproduce the exact 256-case golden corpus captured above --
// proving the constexpr->struct refactor preserved the std::mt19937 draw
// order (random_partition -> generate_random_formula -> strengthen_next ->
// random_tin, per case).
TEST(GeneratedCorpus, DefaultCorpusIsByteIdenticalToGolden) {
  const std::vector<std::unique_ptr<ScopedEnvVar>> clean_env =
      UnsetAllCorpusEnvVars();

  const CorpusConfig config = corpus_config_from_env();
  EXPECT_EQ(config.seed, kCorpusSeed);
  EXPECT_EQ(config.case_count, kCorpusCaseCount);
  EXPECT_EQ(config.tree_size_min, kCorpusTreeSizeMin);
  EXPECT_EQ(config.tree_size_max, kCorpusTreeSizeMax);
  EXPECT_EQ(config.input_max, kCorpusInputMax);
  EXPECT_EQ(config.output_max, kCorpusOutputMax);
  EXPECT_EQ(config.tin_states_max, kCorpusTinStatesMax);
  EXPECT_EQ(config.diff_width_cap, kCorpusDiffWidthCap);
  EXPECT_EQ(config.subprocess_timeout_secs, kCorpusSubprocessTimeoutSecs);

  const std::vector<GeneratedCase> corpus = BuildGeneratedCorpus(config);
  ASSERT_EQ(corpus.size(), kCorpusCaseCount);

  std::ostringstream phi0_os;
  phi0_os << corpus.front().phi;
  EXPECT_EQ(phi0_os.str(), kGoldenDefaultCorpusPhi0)
      << "default corpus's case-0 phi drifted from the golden -- the "
         "CorpusConfig refactor may have shifted the mt19937 draw order";
  EXPECT_EQ(DescribeGeneratedPartition(corpus.front().partition),
            kGoldenDefaultCorpusPartition0)
      << "default corpus's case-0 partition drifted from the golden";
  EXPECT_EQ(ChecksumGeneratedCorpus(corpus), kGoldenDefaultCorpusChecksum)
      << "default corpus's full 256-case (phi, partition) stream drifted "
         "from the golden checksum -- some case beyond #0 shifted";
}

// Env override plumbing (PRD "Test oracles" #2): LTLF_EK_CORPUS_CASES=8
// shrinks both the reported config and the built corpus to exactly 8 cases.
TEST(CorpusConfigFromEnv, CasesOverrideChangesCorpusSize) {
  const ScopedEnvVar cases_guard("LTLF_EK_CORPUS_CASES", "8");

  const CorpusConfig config = corpus_config_from_env();
  EXPECT_EQ(config.case_count, 8u);

  const std::vector<GeneratedCase> corpus = BuildGeneratedCorpus(config);
  EXPECT_EQ(corpus.size(), 8u);
}

// Golden values for LTLF_EK_CORPUS_SEED=1, captured the same way as the
// default golden above (a single BuildGeneratedCorpus call in a fresh
// process).  NOTE for future readers: spot::randltlgenerator draws from a
// process-*global* RNG that a same-seed re-construction does not cleanly
// reset (confirmed experimentally: constructing it twice with an identical
// explicit spot::srand()+"seed" option in the *same* process yields two
// different formulas), so "internally deterministic" is verified the same
// way as the default corpus -- one call per (fresh, ctest-per-test) process
// against a captured golden -- rather than by rebuilding twice in-process
// and comparing, which would spuriously fail on this Spot quirk and not on
// any bug in CorpusConfig/BuildGeneratedCorpus. This is a mechanical Spot-API
// issue (fix: spot::srand(seed) before each in-process build), captured as a
// Phase-2 must-resolve in docs/prd/generated-corpus-soak-mode.md ("Fresh seed
// per level"), since run_corpus's level loop builds multiple corpora per
// process -- not a /theory-review item.
const std::string kGoldenSeed1CorpusPhi0 = "(!p0 & p6) | (p0 & !p6)";
const std::string kGoldenSeed1CorpusPartition0 =
    "input_free={p0 p2 } input_known={p1 } output_free={p3 p4 p5 p6 p7 } "
    "output_known={}";
constexpr std::uint64_t kGoldenSeed1CorpusChecksum = 12756743135039034952ull;

// Env override plumbing (PRD "Test oracles" #2): LTLF_EK_CORPUS_SEED=1
// yields a *different* corpus than the default seed (from the golden
// checksum above), and that corpus is itself reproducible (a captured
// golden, verified the same way the default corpus's is).
TEST(CorpusConfigFromEnv, SeedOverrideChangesCorpusDeterministically) {
  const ScopedEnvVar seed_guard("LTLF_EK_CORPUS_SEED", "1");

  const CorpusConfig config = corpus_config_from_env();
  EXPECT_EQ(config.seed, 1u);

  const std::vector<GeneratedCase> corpus = BuildGeneratedCorpus(config);
  ASSERT_EQ(corpus.size(), kCorpusCaseCount);

  std::ostringstream phi0_os;
  phi0_os << corpus.front().phi;
  EXPECT_EQ(phi0_os.str(), kGoldenSeed1CorpusPhi0);
  EXPECT_EQ(DescribeGeneratedPartition(corpus.front().partition),
            kGoldenSeed1CorpusPartition0);
  const std::uint64_t checksum = ChecksumGeneratedCorpus(corpus);
  EXPECT_EQ(checksum, kGoldenSeed1CorpusChecksum)
      << "seed=1 corpus drifted from its captured golden";
  EXPECT_NE(checksum, kGoldenDefaultCorpusChecksum)
      << "seed=1 corpus is identical to the default-seed golden corpus -- "
         "the seed override is not reaching BuildGeneratedCorpus";
}

// Env override plumbing (PRD "Test oracles" #2, "Parse failure is loud,
// never silent"): a non-integer LTLF_EK_CORPUS_SEED value throws
// std::runtime_error rather than silently falling back to the default seed.
TEST(CorpusConfigFromEnv, MalformedSeedThrows) {
  const ScopedEnvVar seed_guard("LTLF_EK_CORPUS_SEED", "notanumber");
  EXPECT_THROW(corpus_config_from_env(), std::runtime_error);
}

// Env override plumbing (PRD "Test oracles" #2, "for counts/maxes -- zero"
// must throw): LTLF_EK_CORPUS_CASES=0 throws rather than silently building
// an empty (and therefore vacuously "passing") corpus.
TEST(CorpusConfigFromEnv, ZeroCasesThrows) {
  const ScopedEnvVar cases_guard("LTLF_EK_CORPUS_CASES", "0");
  EXPECT_THROW(corpus_config_from_env(), std::runtime_error);
}

// Env override plumbing (PRD "Edge cases", "tree_size_min > tree_size_max"):
// an internally-inconsistent tree-size range throws std::runtime_error
// instead of silently building with a swapped or clamped range.
TEST(CorpusConfigFromEnv, TreeMinGreaterThanMaxThrows) {
  const ScopedEnvVar min_guard("LTLF_EK_CORPUS_TREE_MIN", "20");
  const ScopedEnvVar max_guard("LTLF_EK_CORPUS_TREE_MAX", "5");
  EXPECT_THROW(corpus_config_from_env(), std::runtime_error);
}

// Structural free-rider (PRD "ltlf_to_dfa structural check", Phase 1's
// green checkpoint): for every generated phi, ltlf_to_dfa(phi) must be
// deterministic and complete (ltlf_to_dfa calls spot::complete_here, so
// completeness must hold) -- a pure library property, no external tool, no
// hand-labeled expected value.  This kills the "ltlf_to_dfa asserted on one
// formula" blind spot (PRD "Goal") at zero oracle cost.  Never gated on
// ltlfsynt: a plain TEST, not under LtlfsyntOracleTest, so it runs even
// where ltlfsynt is absent.  Phase 2 (soak-mode PRD "The three bodies under
// soak"): the single BuildGeneratedCorpus pass is now run_corpus's soak==0
// fast path (byte-identical to Phase 1); LTLF_EK_SOAK>0 escalates this same
// per-case assertion across levels until the deadline.
TEST(GeneratedCorpus, LtlfToDfaStructural) {
  const CorpusConfig base = corpus_config_from_env();
  const RunCorpusStats stats = run_corpus(
      base, [](const GeneratedCase& c, const CorpusConfig& cfg,
               unsigned level, std::size_t i) {
        std::ostringstream phi_os;
        phi_os << c.phi;
        SCOPED_TRACE("case " + std::to_string(i) + ": phi=" + phi_os.str() +
                     ", partition=" + DescribeGeneratedPartition(c.partition) +
                     ", level=" + std::to_string(level) + " " +
                     DescribeCorpusConfig(cfg));
        const spot::bdd_dict_ptr dict = spot::make_bdd_dict();
        const spot::twa_graph_ptr dfa = ltlf_to_dfa(c.phi, dict);
        EXPECT_TRUE(spot::is_deterministic(dfa))
            << "ltlf_to_dfa returned a non-deterministic automaton";
        EXPECT_TRUE(spot::is_complete(dfa))
            << "ltlf_to_dfa returned an incomplete automaton";
      });
  RecordProperty("levels_reached", static_cast<int>(stats.levels_reached));
  RecordProperty("cases_run", static_cast<int>(stats.cases_run));
  RecordProperty("cases_skipped", static_cast<int>(stats.cases_skipped));
}

// ---------------------------------------------------------------------------
// Soak smoke (soak-mode PRD "Test oracles" #4): with a tiny LTLF_EK_SOAK
// budget, run_corpus's escalating branch (not the soak==0 fast path, which
// trivially reports levels_reached=1 without escalating at all) reaches at
// least one full level and terminates within a small margin of the budget,
// with no OOM. DISABLED_ so the default `ctest` gate -- which runs the whole
// discovered suite, unfiltered -- never pays a real soak budget: GoogleTest
// skips any DISABLED_-prefixed test unless invoked with BOTH
// --gtest_filter=*SoakSmoke* AND --gtest_also_run_disabled_tests (a soaker
// opts in explicitly; see also the PRD's "per body" budget note -- this
// smokes exactly one body, the cheapest, library-only one, mirroring
// LtlfToDfaStructural's assertion so no ltlfsynt subprocess dependency is
// introduced). The LTLF_EK_SOAK=1 override is scoped to this test body via
// ScopedEnvVar (restored on exit), so even an explicitly-enabled run is
// never at the mercy of whatever LTLF_EK_SOAK a soaker's shell happens to
// have exported for an unrelated, real soak invocation.
// ---------------------------------------------------------------------------
TEST(GeneratedCorpusSoak, DISABLED_LtlfToDfaStructuralReachesAtLeastOneLevel) {
  constexpr unsigned kSmokeBudgetSecs = 1;
  const ScopedEnvVar soak_guard("LTLF_EK_SOAK", "1");

  const CorpusConfig base = corpus_config_from_env();
  ASSERT_EQ(env_soak_secs(), kSmokeBudgetSecs)
      << "LTLF_EK_SOAK override did not reach env_soak_secs()";

  const auto start = std::chrono::steady_clock::now();
  const RunCorpusStats stats = run_corpus(
      base, [](const GeneratedCase& c, const CorpusConfig& cfg,
               unsigned level, std::size_t i) {
        std::ostringstream phi_os;
        phi_os << c.phi;
        SCOPED_TRACE("case " + std::to_string(i) + ": phi=" + phi_os.str() +
                     ", partition=" + DescribeGeneratedPartition(c.partition) +
                     ", level=" + std::to_string(level) + " " +
                     DescribeCorpusConfig(cfg));
        const spot::bdd_dict_ptr dict = spot::make_bdd_dict();
        const spot::twa_graph_ptr dfa = ltlf_to_dfa(c.phi, dict);
        EXPECT_TRUE(spot::is_deterministic(dfa));
        EXPECT_TRUE(spot::is_complete(dfa));
      });
  const auto elapsed = std::chrono::steady_clock::now() - start;
  const auto elapsed_ms =
      std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();

  EXPECT_GE(stats.levels_reached, 1u)
      << "run_corpus(LTLF_EK_SOAK=1) did not reach a single escalation level";
  EXPECT_GT(stats.cases_run, 0u);
  // Soft deadline (PRD "Deadline mechanics"): the case in flight when the
  // deadline passes always finishes, so allow generous slack over the 1s
  // budget for a wide in-flight case plus process/CI scheduling jitter --
  // this is a runaway-escalation smoke check, not a tight timing assertion.
  EXPECT_LT(elapsed, std::chrono::seconds(30))
      << "run_corpus(LTLF_EK_SOAK=" << kSmokeBudgetSecs << ") took "
      << elapsed_ms
      << "ms -- far beyond budget + soft-deadline slack; possible runaway "
         "escalation or OOM-adjacent slowdown";
  RecordProperty("levels_reached", static_cast<int>(stats.levels_reached));
  RecordProperty("cases_run", static_cast<int>(stats.cases_run));
  RecordProperty("cases_skipped", static_cast<int>(stats.cases_skipped));
  RecordProperty("elapsed_ms", static_cast<int>(elapsed_ms));
}

// DumpTinForReplay (soak-mode PRD "Test oracles" deliverable 3, "closing the
// metamorphic t_in replay gap"): renders the reachable (state, Ifree-letter)
// -> (lambda-over-Iknown, delta dst) table for a generated case's random
// Tin, so a MetamorphicRoundTrip failure is fully replayable even though the
// generated corpus itself has no reliable in-process regeneration recipe
// (run_corpus's Spot apid-recycling comment above) -- SCOPED_TRACE already
// prints phi + partition + the seed/config a failing case ran under, but
// until now nothing captured the random Tin those were paired with, so a
// failure's *transducer* half of the reproducer was lost. Enumerates only
// Sigma0=Ifree letters: random_tin's delta guards are exactly Ifree cubes
// and lambda reads only its Sigma0 slice (Role t_in: Sigma0=Ifree,
// Sigma1=Iknown), so Ifree alone determines every (delta, lambda) pair --
// same partial-cube idiom as run_transducer/single_bit_iknown_mutations
// above. A throwaway registrar twa_graph on t_in's own dict resolves I∪O
// names to the exact variable numbers t_in's private graph already
// registered (register_ap is idempotent per dict), so no accessor needs to
// be added to OutputLabeledTransducer's public API for this. Intentionally
// failure-only: call this only from a failing EXPECT_*'s `<<` stream (which
// GoogleTest evaluates lazily -- only when the assertion actually fails),
// never unconditionally, so passing cases pay nothing extra.
std::string DumpTinForReplay(const OutputLabeledTransducer& t_in,
                             const VariablePartition& partition) {
  const spot::bdd_dict_ptr dict = t_in.dict();
  const spot::twa_graph_ptr registrar = spot::make_twa_graph(dict);
  std::vector<int> ifree_vars;
  for (const std::string& n : partition.input_free)
    ifree_vars.push_back(registrar->register_ap(n));
  const std::vector<bdd> ifree_letters = all_letters_over(ifree_vars);

  std::ostringstream os;
  os << "t_in replay dump: initial_state=" << t_in.initial_state()
     << ", " << ifree_letters.size() << " Ifree letter(s) per state\n";
  std::vector<unsigned> queue{t_in.initial_state()};
  std::set<unsigned> visited{t_in.initial_state()};
  for (std::size_t qi = 0; qi < queue.size(); ++qi) {
    const unsigned q = queue[qi];
    for (const bdd& ifree : ifree_letters) {
      const std::optional<bdd> iknown = t_in.lambda(q, ifree);
      os << "  state " << q
         << " ifree=(" << spot::bdd_to_formula(ifree, dict) << ")";
      if (!iknown) {
        os << " lambda=undefined delta=undefined\n";
        continue;
      }
      os << " lambda=(" << spot::bdd_to_formula(*iknown, dict) << ")";
      const std::optional<unsigned> dst = t_in.delta(q, ifree & *iknown);
      if (!dst) {
        os << " delta=undefined\n";
        continue;
      }
      os << " delta=" << *dst << "\n";
      if (visited.insert(*dst).second) queue.push_back(*dst);
    }
  }
  return os.str();
}

// Metamorphic round-trip (PRD "Test oracles" #1, Phase 2's green checkpoint;
// extended by docs/prd/mtdfa-product.md "Test oracles" #1 to a SECOND
// method): for every generated case, DfaProduct::synthesize (a) and
// MtdfaProduct::synthesize (b) must AGREE on realizability (cross-method
// metamorphic -- route (a) leans on Spot semantics Phase 0 probes but does
// not prove, so this "roughly doubles corpus runtime; earns it"), and
// whichever of a/b returns a Controller must pass verify_controller on that
// same (phi, Tin, trivial Tout, T_C) -- the standing "every ...Product
// controller verifies" invariant (docs/prd/controller-verifier.md), now
// exercised on generated phi AND generated Tin, for BOTH methods
// independently.  Unrealizable cases assert nothing further for that method
// -- one-directional by design (PRD "Edge cases" "Unrealizable generated
// case").  Never gated on ltlfsynt: a plain TEST, not under
// LtlfsyntOracleTest, so it runs even where ltlfsynt is absent (the
// differential half of oracle #1, "EXPECT_EQ(b.has_value(),
// ltlfsynt_verdict(...))", lives in GeneratedCorpusDifferential below,
// which IS gated on ltlfsynt since it drives it as a subprocess).
// Phase 2 (soak-mode PRD "The three bodies under soak"): driven by
// run_corpus, unchanged assertion; the per-case `continue` becomes an early
// lambda `return`.
TEST(GeneratedCorpus, MetamorphicRoundTrip) {
  const CorpusConfig base = corpus_config_from_env();
  const RunCorpusStats stats = run_corpus(
      base, [](const GeneratedCase& c, const CorpusConfig& cfg,
               unsigned level, std::size_t i) {
        std::ostringstream phi_os;
        phi_os << c.phi;
        SCOPED_TRACE("case " + std::to_string(i) + ": phi=" + phi_os.str() +
                     ", partition=" + DescribeGeneratedPartition(c.partition) +
                     ", level=" + std::to_string(level) + " " +
                     DescribeCorpusConfig(cfg));

        const OutputLabeledTransducer t_out =
            trivial_transducer(c.partition, Role::t_out, c.t_in.dict());

        DfaProduct dfa_method;
        MtdfaProduct mtdfa_method;
        const std::optional<Controller> a =
            dfa_method.synthesize(c.phi, c.partition, c.t_in, t_out);
        const std::optional<Controller> b =
            mtdfa_method.synthesize(c.phi, c.partition, c.t_in, t_out);

        EXPECT_EQ(a.has_value(), b.has_value())
            << "cross-method metamorphic failure: DfaProduct and "
               "MtdfaProduct disagree on realizability -- t_in for replay "
               "(see DumpTinForReplay):\n"
            << DumpTinForReplay(c.t_in, c.partition);

        if (a.has_value())
          EXPECT_TRUE(
              verify_controller(c.phi, c.partition, c.t_in, t_out, *a).ok)
              << "metamorphic round-trip failed: DfaProduct returned a "
                 "controller that verify_controller rejects -- t_in for "
                 "replay (see DumpTinForReplay):\n"
              << DumpTinForReplay(c.t_in, c.partition);
        if (b.has_value())
          EXPECT_TRUE(
              verify_controller(c.phi, c.partition, c.t_in, t_out, *b).ok)
              << "metamorphic round-trip failed: MtdfaProduct returned a "
                 "controller that verify_controller rejects -- t_in for "
                 "replay (see DumpTinForReplay):\n"
              << DumpTinForReplay(c.t_in, c.partition);
      });
  RecordProperty("levels_reached", static_cast<int>(stats.levels_reached));
  RecordProperty("cases_run", static_cast<int>(stats.cases_run));
  RecordProperty("cases_skipped", static_cast<int>(stats.cases_skipped));
}

// ---------------------------------------------------------------------------
// Build-equivalence metamorphic oracle, generated-corpus half
// (docs/prd/symbolic-dfa-product.md "Test oracles" #2): for every generated
// (phi, partition, Tin), build_product_symbolic(...) must equal
// to_guard_map(build_product(...), alphabet) on the SAME (goal, taus, init)
// DfaProduct::synthesize itself would build (src/dfa_product.cpp) --
// identical reachable ProductState set, identical acc flag, BDD-equal
// per-<src,dst> guards.  This is the "same game" check, distinct from
// MetamorphicRoundTrip above (which checks "same verdict" via
// synthesize->verify_controller) and from GeneratedCorpusDifferential below
// (realizability against an external oracle) -- it never calls synthesize or
// solve_dfa, so it needs no controller and runs unconditionally realizable or
// not.  Library-only, so -- like MetamorphicRoundTrip -- a plain TEST, not
// gated on ltlfsynt, and driven by run_corpus for soak escalation.
//
// DescribeProductState / ExpectProductGuardsEqual duplicate
// tests/product_build_equivalence_test.cpp's dedicated-fixture helpers
// (this project's one-file-per-suite duplication norm, matching CliResult /
// ShellQuote above) rather than sharing them across translation units.
// ---------------------------------------------------------------------------

std::string DescribeProductState(const ProductState& s) {
  std::ostringstream os;
  os << "<goal=" << s.goal << ", taus=[";
  for (std::size_t i = 0; i < s.taus.size(); ++i) {
    if (i) os << ",";
    os << s.taus[i];
  }
  os << "]>";
  return os.str();
}

void ExpectProductGuardsEqual(const ProductGuards& symbolic,
                              const ProductGuards& reference) {
  ASSERT_EQ(symbolic.nodes.size(), reference.nodes.size())
      << "different number of reachable ProductStates between the symbolic "
         "and per-letter (reference) builds";
  for (const auto& [state, ref_entry] : reference.nodes) {
    SCOPED_TRACE("state " + DescribeProductState(state));
    ASSERT_TRUE(symbolic.nodes.count(state))
        << "symbolic build is missing a ProductState the reference build "
           "reached";
    const auto& [ref_acc, ref_guards] = ref_entry;
    const auto& [sym_acc, sym_guards] = symbolic.nodes.at(state);
    EXPECT_EQ(sym_acc, ref_acc) << "acc flag differs";
    ASSERT_EQ(sym_guards.size(), ref_guards.size())
        << "different number of outgoing destinations from this state";
    for (const auto& [dst, ref_guard] : ref_guards) {
      SCOPED_TRACE("dst " + DescribeProductState(dst));
      ASSERT_TRUE(sym_guards.count(dst))
          << "symbolic build is missing an edge to this destination";
      EXPECT_EQ(sym_guards.at(dst), ref_guard)
          << "guard BDD differs (BuDDy canonicalises, so == is semantic "
             "equality)";
    }
  }
}

TEST(GeneratedCorpus, BuildEquivalence) {
  const CorpusConfig base = corpus_config_from_env();
  const RunCorpusStats stats = run_corpus(
      base, [](const GeneratedCase& c, const CorpusConfig& cfg,
               unsigned level, std::size_t i) {
        std::ostringstream phi_os;
        phi_os << c.phi;
        SCOPED_TRACE("case " + std::to_string(i) + ": phi=" + phi_os.str() +
                     ", partition=" + DescribeGeneratedPartition(c.partition) +
                     ", level=" + std::to_string(level) + " " +
                     DescribeCorpusConfig(cfg));

        const OutputLabeledTransducer t_out =
            trivial_transducer(c.partition, Role::t_out, c.t_in.dict());
        const std::vector<const Transducer*> taus{&c.t_in, &t_out};

        // Same (goal, taus, init) construction as DfaProduct::synthesize
        // (src/dfa_product.cpp): ltlf_to_dfa always returns a complete DFA,
        // so invariant 3 holds for every generated phi without a
        // hand-crafted goal.
        const spot::twa_graph_ptr goal = ltlf_to_dfa(c.phi, c.t_in.dict());
        const ProductState init{goal->get_init_state_number(),
                                {c.t_in.initial_state(), t_out.initial_state()}};

        const LetterAlphabet alphabet(c.partition, goal);
        const ProductGuards symbolic = build_product_symbolic(goal, taus, init);
        const std::map<ProductState, ltlf_ek::ProductNode> graph = build_product(
            goal, taus, init, alphabet, /*goal_must_be_complete=*/true);
        const ProductGuards reference = to_guard_map(graph, alphabet);

        ExpectProductGuardsEqual(symbolic, reference);
      });
  RecordProperty("levels_reached", static_cast<int>(stats.levels_reached));
  RecordProperty("cases_run", static_cast<int>(stats.cases_run));
  RecordProperty("cases_skipped", static_cast<int>(stats.cases_skipped));
}

// Comma-joins an AP-name set for --inputs/--outputs (ek-synth) or the
// --ins=/--outs= form (ltlfsynt) -- both parse a plain comma-separated list
// (src/ltlf_ek_synth.cpp SplitCsv).
std::string JoinCsv(const std::set<std::string>& names) {
  std::string out;
  for (const std::string& name : names) {
    if (!out.empty()) out += ",";
    out += name;
  }
  return out;
}

// Differential (PRD "Test oracles" #2, Phase 3's green checkpoint): over the
// V = empty (all-free partition, psi_in = top) subset of the generated corpus
// whose width |I union O| <= 3 (PRD "Partition generation" "Differential
// width cap"), ltlf-ek-synth and `ltlfsynt --semantics=Mealy` must agree on
// the bare-phi REALIZABLE/UNREALIZABLE verdict -- no assumption reduction, so
// no load-bearing guard (that concept only applies when psi_in != top).
// Random Tin is never fed here: this body only reads phi/partition off the
// generated case, matching the PRD's "differential body needs only phi +
// partition". Gated on ltlfsynt via LtlfsyntOracleTest (GTEST_SKIPs cleanly
// when it is absent, per "Edge cases").  Phase 2 (soak-mode PRD "The three
// bodies under soak"): driven by run_corpus; the width cap escalates too
// (config.diff_width_cap, clamped by the ladder to kCorpusDiffWidthSoakCap =
// 5), and the RunSubprocess timeout stays per-subprocess so a slow ltlfsynt
// is always a skip, never a failure, under soak.
//
// Extended by docs/prd/mtdfa-product.md "Test oracles" #1 (the differential
// line of the oracle's pseudocode, "EXPECT_EQ(b.has_value(),
// ltlfsynt_verdict(...))"): the SAME phi/partition are also driven through
// `--mtdfa-product`, and MtdfaProduct's verdict must agree with ltlfsynt too
// -- "the differential drives the binary as a subprocess, so it needs
// --mtdfa-product -- a Phase 1 dependency, not Phase 2."
TEST_F(LtlfsyntOracleTest, GeneratedCorpusDifferential) {
  const CorpusConfig base = corpus_config_from_env();
  std::size_t skipped = 0;
  const RunCorpusStats stats = run_corpus(
      base, [this, &skipped](const GeneratedCase& c, const CorpusConfig& cfg,
                             unsigned level, std::size_t i) {
        // V = empty subset only: an all-free partition (PRD "Behaviour",
        // differential item).
        if (!c.partition.input_known.empty() ||
            !c.partition.output_known.empty())
          return;
        // Differential width cap (soak-mode PRD "Configuration struct" /
        // "Escalation ladder"): was the literal `3`, now cfg.diff_width_cap
        // (defaulting to kCorpusDiffWidthCap = 3, ladder-clamped to
        // kCorpusDiffWidthSoakCap = 5 under soak).
        const std::size_t width =
            c.partition.input_free.size() + c.partition.output_free.size();
        if (width > cfg.diff_width_cap) return;

        std::ostringstream phi_os;
        phi_os << c.phi;
        const std::string phi_str = phi_os.str();
        SCOPED_TRACE("case " + std::to_string(i) + ": phi=" + phi_str +
                     ", partition=" + DescribeGeneratedPartition(c.partition) +
                     ", level=" + std::to_string(level) + " " +
                     DescribeCorpusConfig(cfg));

        std::vector<std::string> ek_args = {
            "--dfa-product", "--formula=" + phi_str, "--inputs",
            JoinCsv(c.partition.input_free)};
        if (!c.partition.output_free.empty()) {
          ek_args.push_back("--outputs");
          ek_args.push_back(JoinCsv(c.partition.output_free));
        }
        ek_args.push_back("--realizable");

        std::vector<std::string> synt_args = {
            "--ins=" + JoinCsv(c.partition.input_free)};
        if (!c.partition.output_free.empty())
          synt_args.push_back("--outs=" + JoinCsv(c.partition.output_free));
        synt_args.push_back("--semantics=Mealy");
        synt_args.push_back("--realizability");
        synt_args.push_back("-f");
        synt_args.push_back(phi_str);

        bool ek_timed_out = false;
        const CliResult ek =
            RunEkSynth(ek_args, cfg.subprocess_timeout_secs, &ek_timed_out);
        if (ek_timed_out) {
          ++skipped;
          return;  // a slow subprocess is a skip, never a test failure.
        }
        bool synt_timed_out = false;
        const CliResult synt = RunLtlfsynt(
            synt_args, cfg.subprocess_timeout_secs, &synt_timed_out);
        if (synt_timed_out) {
          ++skipped;
          return;
        }

        // ParseEkSynthVerdict/ParseLtlfsyntVerdict ADD_FAILURE loudly (with
        // captured stderr) on any exit code/output outside the known verdict
        // contract, so a parse/usage error can never masquerade as a
        // verdict.
        const Verdict ek_verdict = ParseEkSynthVerdict(ek);
        const Verdict synt_verdict = ParseLtlfsyntVerdict(synt);
        EXPECT_EQ(IsRealizable(ek_verdict), IsRealizable(synt_verdict))
            << "generated-corpus differential disagreement for phi="
            << phi_str
            << ", partition=" << DescribeGeneratedPartition(c.partition);

        // MtdfaProduct half (docs/prd/mtdfa-product.md "Test oracles" #1):
        // same phi/partition, --mtdfa-product instead of --dfa-product,
        // compared against the SAME ltlfsynt verdict already computed above.
        std::vector<std::string> mtdfa_args = ek_args;
        mtdfa_args[0] = "--mtdfa-product";
        bool mtdfa_timed_out = false;
        const CliResult mtdfa_ek = RunEkSynth(
            mtdfa_args, cfg.subprocess_timeout_secs, &mtdfa_timed_out);
        if (mtdfa_timed_out) {
          ++skipped;
          return;
        }
        const Verdict mtdfa_verdict = ParseEkSynthVerdict(mtdfa_ek);
        EXPECT_EQ(IsRealizable(mtdfa_verdict), IsRealizable(synt_verdict))
            << "MtdfaProduct generated-corpus differential disagreement for "
               "phi="
            << phi_str
            << ", partition=" << DescribeGeneratedPartition(c.partition);
      });
  RecordProperty("levels_reached", static_cast<int>(stats.levels_reached));
  RecordProperty("cases_run", static_cast<int>(stats.cases_run));
  RecordProperty("cases_skipped", static_cast<int>(stats.cases_skipped));
  RecordProperty("differential_skipped", static_cast<int>(skipped));
}

}  // namespace
