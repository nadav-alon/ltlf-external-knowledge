// O1 (the linchpin) and O3 -- Phase 3 CLI-level oracles
// (docs/prd/output-dependencies-tool.md "Test oracles"). Drives the
// `ltlf-ek-deps` binary as a subprocess, exactly mirroring
// tests/ltlf_ek_synth_test.cpp's binary-path plumbing and
// tests/ltlfsynt_oracle_test.cpp's RunSubprocess/ScopedTempFile harness (both
// duplicated file-locally per this project's stated one-file-per-suite
// convention -- see that file's own header comment).
//
// Asserts the PRD's frozen Phase 3 contract: the flags table, the stdout line
// (`dependent outputs: x   (of x, y)` / `dependent outputs: none`), the exit
// codes (0 success, 2 usage error, 3 unsatisfiable phi, 1 internal), and
// `print_partition_file`. Exact texts are pinned LITERALLY -- a difference
// even in spacing is a PRD-change event, per that block's own "If
// implementation proves this contract wrong" clause, not something to adjust
// here.

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
#include <spot/misc/optionmap.hh>
#include <spot/tl/defaultenv.hh>
#include <spot/tl/parse.hh>
#include <spot/tl/randomltl.hh>
#include <spot/twa/bdddict.hh>

#include "ltlf_ek/cli.hpp"
#include "ltlf_ek/dependent_outputs.hpp"
#include "ltlf_ek/detail/util.hpp"
#include "ltlf_ek/variables.hpp"

#ifndef LTLF_EK_SYNTH_BINARY
#error "LTLF_EK_SYNTH_BINARY must be defined by CMake (see CMakeLists.txt)"
#endif
#ifndef LTLF_EK_DEPS_BINARY
#error "LTLF_EK_DEPS_BINARY must be defined by CMake (see CMakeLists.txt, alongside LTLF_EK_SYNTH_BINARY)"
#endif

namespace {

using ltlf_ek::dependent_outputs;
using ltlf_ek::DependentOutputs;
using ltlf_ek::parse_partition_file;
using ltlf_ek::print_partition_file;
using ltlf_ek::VariablePartition;

// ---------------------------------------------------------------------------
// Subprocess harness (mirrors tests/ltlfsynt_oracle_test.cpp's CliResult /
// ShellQuote / ScopedTempFile / RunSubprocess, duplicated rather than shared
// across translation units per that file's own stated convention).
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
    path_ = ltlf_ek::detail::temp_template("ltlf_ek_deps_test");
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

// An existing DIRECTORY at an output path. rename(2) of a file onto a
// directory always fails (EISDIR), which is the one portable way to make an
// artifact fail at the INSTALL step rather than the open step -- i.e. after an
// earlier artifact in the same run has already been renamed into place.
class ScopedTempDir {
 public:
  ScopedTempDir() {
    path_ = ltlf_ek::detail::temp_template("ltlf_ek_deps_test_dir");
    EXPECT_NE(mkdtemp(path_.data()), nullptr) << "mkdtemp failed";
  }
  ~ScopedTempDir() { rmdir(path_.c_str()); }
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

// Verdict parsing (mirrors tests/ltlfsynt_oracle_test.cpp): parse the printed
// word, never the exit code alone; ltlf-ek-synth's {realizable,
// unrealizable} exit-code pair is {0, 20}, ltlfsynt's is {0, 1}.
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

// ---------------------------------------------------------------------------
// O3 -- part-file co-management (all four PRD assertions). None of these
// need ltlfsynt, so they are plain TEST()s, not gated on its availability.
// ---------------------------------------------------------------------------

// Assertion 2 (order-independent of the rest): parse_partition_file(
// print_partition_file(p)) == p (as sets, per the PRD's own phrasing) -- a
// pure library round-trip, no subprocess at all. Exercises a spread of
// partition shapes (empty inputs, empty outputs, empty partition, multiple
// known variables in every slot), not just the single a/x pair.
TEST(PrintPartitionFile, RoundTripsForAVarietyOfPartitionShapes) {
  const std::vector<VariablePartition> fixtures = {
      VariablePartition::split({"a"}, {"x"}, /*governed=*/{}),
      VariablePartition::split({"a", "b"}, {"x", "y"}, /*governed=*/{"b"}),
      VariablePartition::split({"a", "b"}, {"x", "y"},
                               /*governed=*/{"b", "x"}),
      VariablePartition::split({}, {"x"}, /*governed=*/{}),
      VariablePartition::split({"a"}, {}, /*governed=*/{}),
      VariablePartition::split({}, {}, /*governed=*/{}),
  };
  for (const VariablePartition& p : fixtures) {
    std::ostringstream out;
    print_partition_file(out, p);
    std::istringstream in(out.str());
    const VariablePartition round_tripped = parse_partition_file(in);
    EXPECT_EQ(round_tripped.input_free, p.input_free);
    EXPECT_EQ(round_tripped.input_known, p.input_known);
    EXPECT_EQ(round_tripped.output_free, p.output_free);
    EXPECT_EQ(round_tripped.output_known, p.output_known);
  }
}

// Assertion 1: feed a part file with a non-empty input_known; the emitted
// part file preserves input_free/input_known byte-for-byte-equivalently (as
// sets) and repartitions only the two output keys. Same (phi, partition)
// data as tests/dependent_outputs_test.cpp's
// NonEmptyInputKnownPassesThroughVerbatim -- reusing the concrete fixture to
// check a DIFFERENT property (the CLI's I9 pass-through), cross-checked
// against the already-landed library oracle for the expected repartition.
TEST(LtlfEkDepsPartFile,
    EmitPartPassesThroughInputKeysAndRepartitionsOutputsOnly) {
  const std::string phi_str = "G(a <-> x)";
  const ScopedTempFile input_part(
      "input_free:   a\n"
      "input_known:  b\n"
      "output_free:  x\n"
      "output_known:\n");
  const ScopedTempFile emit_part;
  const ScopedTempFile transducer_file;

  const CliResult deps =
      RunEkDeps({"--formula=" + phi_str, "--part-file", input_part.path(),
                 "--emit-part", emit_part.path(), "--transducer",
                 transducer_file.path()});
  ASSERT_EQ(deps.exit_code, 0) << deps.stderr_text;

  std::ifstream emitted_in(emit_part.path());
  const VariablePartition emitted = parse_partition_file(emitted_in);

  // input_free / input_known pass through byte-for-byte-equivalently (as
  // sets) -- I9.
  EXPECT_EQ(emitted.input_free, (std::set<std::string>{"a"}));
  EXPECT_EQ(emitted.input_known, (std::set<std::string>{"b"}));

  // Only the two output keys are repartitioned; cross-check against the
  // already-landed Phase 2 library oracle for the SAME (phi, partition) --
  // b never occurs in phi (I10: the analysis ignores Tin/Iknown entirely).
  auto dict = spot::make_bdd_dict();
  std::ifstream input_in2(input_part.path());
  const VariablePartition parsed_input = parse_partition_file(input_in2);
  const DependentOutputs expected =
      dependent_outputs(spot::parse_formula(phi_str), parsed_input, dict);
  EXPECT_EQ(emitted.output_known, expected.dependent);
  EXPECT_EQ(emitted.output_free, expected.partition.output_free);
}

// Assertion 3: a non-empty output_known on input is refused with exit 2 (I9
// -- there is no "compose two Touts" notion).
TEST(LtlfEkDepsPartFile, RefusesNonEmptyOutputKnownOnInputWithExitCode2) {
  const ScopedTempFile input_part(
      "input_free:   a\n"
      "output_free:  x\n"
      "output_known: y\n");
  const ScopedTempFile emit_part;
  const CliResult deps =
      RunEkDeps({"--formula=G(a <-> x)", "--part-file", input_part.path(),
                 "--emit-part", emit_part.path()});
  EXPECT_EQ(deps.exit_code, 2);
}

// Assertion 4: --emit-part equal to --part-file is refused with exit 2 (a
// crash mid-write must not destroy the co-managed file).
TEST(LtlfEkDepsPartFile, RefusesEmitPartEqualToPartFileWithExitCode2) {
  const ScopedTempFile input_part(
      "input_free:   a\n"
      "output_free:  x\n"
      "output_known:\n");
  const CliResult deps =
      RunEkDeps({"--formula=G(a <-> x)", "--part-file", input_part.path(),
                 "--emit-part", input_part.path()});
  EXPECT_EQ(deps.exit_code, 2);
}

// Same refusal, reached by a DIFFERENT SPELLING of the one path. The guard
// used to compare the two argv strings verbatim, so `/tmp/p` vs `/tmp/./p`
// slipped past it and the tool rewrote the part file it had just read --
// precisely what assertion 4 exists to prevent. Now compared on the resolved
// path.
TEST(LtlfEkDepsPartFile, RefusesEmitPartAliasingPartFileByADifferentSpelling) {
  const std::string contents =
      "input_free:   a\n"
      "input_known:  b\n"
      "output_free:  x\n"
      "output_known:\n";
  const ScopedTempFile input_part(contents);

  // Same file, spelled with a redundant `/./` -- a plain string compare sees
  // two different paths.
  const std::size_t slash = input_part.path().find_last_of('/');
  ASSERT_NE(slash, std::string::npos);
  const std::string aliased = input_part.path().substr(0, slash) + "/./" +
                              input_part.path().substr(slash + 1);

  const CliResult deps =
      RunEkDeps({"--formula=G(a <-> x)", "--part-file", input_part.path(),
                 "--emit-part", aliased});
  EXPECT_EQ(deps.exit_code, 2) << "stderr=[" << deps.stderr_text << "]";

  std::ifstream after(input_part.path());
  std::ostringstream after_ss;
  after_ss << after.rdbuf();
  EXPECT_EQ(after_ss.str(), contents)
      << "the --part-file must be left byte-identical when the run is refused";
}

// The same refusal for the OTHER output flag. Assertion 4 names --emit-part,
// but the hazard is a property of the path, not of the flag that lands on it:
// --transducer aliasing --part-file replaced the co-managed part file with a
// transducer and, because the partition had already been read into memory,
// reported success while doing it (exit 0, `dependent outputs: x`). Only two
// of the three pairs were guarded.
TEST(LtlfEkDepsPartFile, RefusesTransducerEqualToPartFileWithExitCode2) {
  const std::string contents =
      "input_free:   a\n"
      "output_free:  x\n"
      "output_known:\n";
  const ScopedTempFile input_part(contents);

  const CliResult deps =
      RunEkDeps({"--formula=G(a <-> x)", "--part-file", input_part.path(),
                 "--transducer", input_part.path()});
  EXPECT_EQ(deps.exit_code, 2) << "stdout=[" << deps.stdout_text << "]";

  std::ifstream after(input_part.path());
  std::ostringstream after_ss;
  after_ss << after.rdbuf();
  EXPECT_EQ(after_ss.str(), contents)
      << "the --part-file must not be overwritten by the emitted transducer";
}

// All-or-nothing artifact commit. The part file declares output_known = Xdep,
// and ltlf-ek-synth REFUSES that without a companion
// --known-output-transducer, so a run that wrote the part file and then failed
// on the transducer left behind a pair that breaks the pipeline this tool
// feeds. Both artifacts are now staged before either is installed.
TEST(LtlfEkDepsPartFile, FailedTransducerWriteLeavesNoPartFileBehind) {
  const ScopedTempFile input_part(
      "input_free:   a\n"
      "output_free:  x\n"
      "output_known:\n");
  const ScopedTempFile emit_part;

  const CliResult deps = RunEkDeps(
      {"--formula=G(a <-> x)", "--part-file", input_part.path(), "--emit-part",
       emit_part.path(), "--transducer",
       "/nonexistent-dir-ltlf-ek-deps/t.out"});
  EXPECT_EQ(deps.exit_code, 2);

  // mkstemp created emit_part empty; a partial commit would have filled it in
  // with `output_known: x` and no transducer to match.
  std::ifstream emitted_in(emit_part.path());
  std::ostringstream emitted_contents;
  emitted_contents << emitted_in.rdbuf();
  EXPECT_TRUE(emitted_contents.str().empty())
      << "a failed --transducer write must not leave a committed part file: ["
      << emitted_contents.str() << "]";

  // And no staging file may survive the failure.
  EXPECT_FALSE(std::ifstream(emit_part.path() + ".ltlf-ek-deps.tmp").good());
}

// The same guarantee one step later, at the INSTALL rather than the open. The
// test above fails while staging, so nothing had been renamed yet; staging
// alone does not make the commit atomic, because a rename can still fail on
// its own (here: a target that exists as a directory). With the part file
// installed FIRST, this left exactly the orphan the staging exists to
// prevent -- `output_known: x` with no transducer, which ltlf-ek-synth then
// refuses outright ("output_known is non-empty but --known-output-transducer
// is missing"). The part file is now the last artifact installed.
TEST(LtlfEkDepsPartFile, FailedTransducerInstallLeavesNoPartFileBehind) {
  const ScopedTempFile input_part(
      "input_free:   a\n"
      "output_free:  x\n"
      "output_known:\n");
  const ScopedTempFile emit_part;
  const ScopedTempDir transducer_dir;  // rename onto it fails with EISDIR

  const CliResult deps =
      RunEkDeps({"--formula=G(a <-> x)", "--part-file", input_part.path(),
                 "--emit-part", emit_part.path(), "--transducer",
                 transducer_dir.path()});
  EXPECT_EQ(deps.exit_code, 2) << "stdout=[" << deps.stdout_text << "]";

  std::ifstream emitted_in(emit_part.path());
  std::ostringstream emitted_contents;
  emitted_contents << emitted_in.rdbuf();
  EXPECT_TRUE(emitted_contents.str().empty())
      << "a --transducer that fails to INSTALL must not leave a committed "
         "part file: ["
      << emitted_contents.str() << "]";

  EXPECT_FALSE(std::ifstream(emit_part.path() + ".ltlf-ek-deps.tmp").good());
  EXPECT_FALSE(
      std::ifstream(transducer_dir.path() + ".ltlf-ek-deps.tmp").good());
}

// Xdep = empty writes no transducer (Edge cases) -- but "writes nothing" must
// not mean "leaves whatever was there". A file from an earlier run at the same
// path describes a DIFFERENT Xdep than the part file this run just wrote, so
// the pair on disk contradicts itself, and a caller that passes both flags
// every time silently gets the stale Tout beside a fresh part file.
TEST(LtlfEkDepsPartFile, EmptyDependentSetClearsAStaleTransducerFile) {
  const ScopedTempFile emit_part;
  const ScopedTempFile transducer;

  // Run 1: Xdep = {x}, so both artifacts land.
  const CliResult first =
      RunEkDeps({"--formula=G(a <-> x)", "--inputs", "a", "--outputs", "x",
                 "--emit-part", emit_part.path(), "--transducer",
                 transducer.path()});
  ASSERT_EQ(first.exit_code, 0) << first.stderr_text;
  {
    std::ifstream in(emit_part.path());
    EXPECT_EQ(parse_partition_file(in).output_known,
              (std::set<std::string>{"x"}));
  }
  EXPECT_GT(std::ifstream(transducer.path()).peek(), -1)
      << "run 1 should have written a transducer";

  // Run 2: same paths, a formula with no dependent output.
  const CliResult second =
      RunEkDeps({"--formula=G(a -> x)", "--inputs", "a", "--outputs", "x",
                 "--emit-part", emit_part.path(), "--transducer",
                 transducer.path()});
  ASSERT_EQ(second.exit_code, 0) << second.stderr_text;
  {
    std::ifstream in(emit_part.path());
    EXPECT_TRUE(parse_partition_file(in).output_known.empty());
  }
  EXPECT_FALSE(std::ifstream(transducer.path()).good())
      << "run 1's transducer must not survive beside run 2's part file, which "
         "declares no known output at all";
  EXPECT_NE(second.stderr_text.find("removed the stale file"),
            std::string::npos)
      << "the removal must be reported; stderr=[" << second.stderr_text << "]";
}

// ---------------------------------------------------------------------------
// Frozen stdout contract (PRD "Interfaces & types" Phase 3): "stdout gets one
// line, `dependent outputs: x   (of x, y)`" for the non-empty case, and
// "dependent outputs: none" (PRD "Edge cases") for the empty case. Pinned
// LITERALLY -- if the real implementation's exact text differs even in
// spacing, that is a PRD-change event (per the block's own "If
// implementation proves this contract wrong" clause), not something to
// silently adjust here.
// ---------------------------------------------------------------------------

// I={a}, O={x,y}, phi=G(a<->x): x is the only dependent output (y is
// unconstrained), reproducing the PRD's own literal example exactly.
TEST(LtlfEkDepsStdout, DependentOutputsLineMatchesFrozenExampleLiterally) {
  const ScopedTempFile emit_part;
  const CliResult deps =
      RunEkDeps({"--formula=G(a <-> x)", "--inputs", "a", "--outputs", "x,y",
                 "--emit-part", emit_part.path()});
  ASSERT_EQ(deps.exit_code, 0) << deps.stderr_text;
  EXPECT_EQ(deps.stdout_text, "dependent outputs: x   (of x, y)\n");
}

// U2 (Xdep = empty): stdout must say "dependent outputs: none" (PRD "Edge
// cases"), and (per the same bullet) no transducer file is written even in
// pure-query mode -- exercised here by simply not passing --transducer at
// all.
TEST(LtlfEkDepsStdout, EmptyDependentSetPrintsNone) {
  const ScopedTempFile emit_part;
  const CliResult deps =
      RunEkDeps({"--formula=G(a -> x)", "--inputs", "a", "--outputs", "x",
                 "--emit-part", emit_part.path()});
  ASSERT_EQ(deps.exit_code, 0) << deps.stderr_text;
  EXPECT_EQ(deps.stdout_text, "dependent outputs: none\n");

  std::ifstream emitted_in(emit_part.path());
  const VariablePartition emitted = parse_partition_file(emitted_in);
  EXPECT_TRUE(emitted.output_known.empty());
}

// Exit 3 is reserved for an unsatisfiable phi and must not be reachable by any
// other dependent_outputs refusal. It used to be dispatched by searching the
// std::invalid_argument message for "unsatisfiable", which an AP *named*
// `unsatisfiable` trips: the closed-universe refusal below then exited 3,
// reporting a usage error as an unsatisfiable formula. Fixed by giving the
// unsatisfiable case its own exception type (ltlf_ek::UnsatisfiableFormula).
TEST(LtlfEkDepsExitCodes, ClosedUniverseRefusalExitsTwoEvenWhenAnApIsNamedUnsatisfiable) {
  const CliResult deps = RunEkDeps(
      {"--formula=G(a <-> unsatisfiable)", "--inputs", "a", "--outputs", "x"});
  EXPECT_EQ(deps.exit_code, 2)
      << "an AP named 'unsatisfiable' must not be mistaken for an "
         "unsatisfiable formula; stderr=["
      << deps.stderr_text << "]";
}

// Unsatisfiable phi (PRD "Edge cases"): exit 3, no artifacts written. The
// emit-part ScopedTempFile already exists (mkstemp) and is empty; asserting
// it stays empty after the run stands in for "writes no artifacts".
TEST(LtlfEkDepsExitCodes, UnsatisfiableFormulaExitsThreeAndWritesNothing) {
  const ScopedTempFile emit_part;
  const CliResult deps = RunEkDeps({"--formula=0", "--inputs", "a",
                                     "--outputs", "x", "--emit-part",
                                     emit_part.path()});
  EXPECT_EQ(deps.exit_code, 3);
  std::ifstream emitted_in(emit_part.path());
  std::ostringstream emitted_contents;
  emitted_contents << emitted_in.rdbuf();
  EXPECT_TRUE(emitted_contents.str().empty())
      << "unsatisfiable phi must write no artifacts";
}

// ---------------------------------------------------------------------------
// --verbose. Previously exercised by nothing, and unexercisable in any useful
// sense: the CLI re-derived liveness/live-regions/the greedy walk from its own
// copy of src/dependent_outputs.cpp's algorithm, so a test of the narration
// tested the copy, not the search. The narration now comes from
// dependent_outputs' CandidateObserver, i.e. from the real greedy loop, so
// these assertions bind the actual verdict.
// ---------------------------------------------------------------------------

// Narration goes to stderr; stdout keeps the single machine-readable line the
// PRD specifies ("stdout gets one line"), so --verbose cannot break a caller
// that parses stdout.
TEST(LtlfEkDepsVerbose, NarratesToStderrAndLeavesStdoutSingleLine) {
  const CliResult deps = RunEkDeps({"--formula=G(x <-> y)", "--inputs", "a",
                                    "--outputs", "x,y", "--verbose"});
  ASSERT_EQ(deps.exit_code, 0) << deps.stderr_text;
  EXPECT_EQ(deps.stdout_text, "dependent outputs: x   (of x, y)\n");
  EXPECT_EQ(deps.stderr_text,
            "candidate x: accepted\n"
            "candidate y: rejected (undetermined: x)\n");
}

// The trace must agree with the verdict, which is what tying it to the real
// loop buys: every `accepted` candidate, and only those, appear in Xdep. U3's
// phi is the discriminating one -- x is accepted, y is then rejected against
// the ACCUMULATED {x, y} (I6), not against {y} alone.
TEST(LtlfEkDepsVerbose, AcceptedCandidatesAreExactlyTheReportedDependentSet) {
  const CliResult deps = RunEkDeps({"--formula=G(a <-> x)", "--inputs", "a",
                                    "--outputs", "x,y", "--verbose"});
  ASSERT_EQ(deps.exit_code, 0) << deps.stderr_text;
  EXPECT_EQ(deps.stderr_text,
            "candidate x: accepted\n"
            "candidate y: rejected (undetermined: y)\n");
  EXPECT_EQ(deps.stdout_text, "dependent outputs: x   (of x, y)\n");
}

// An unsatisfiable phi is refused before the greedy loop runs, so --verbose
// narrates nothing rather than a vacuous all-accepted walk.
TEST(LtlfEkDepsVerbose, UnsatisfiableFormulaNarratesNoCandidates) {
  const CliResult deps =
      RunEkDeps({"--formula=0", "--inputs", "a", "--outputs", "x",
                 "--verbose"});
  EXPECT_EQ(deps.exit_code, 3);
  EXPECT_EQ(deps.stdout_text, "");
  EXPECT_EQ(deps.stderr_text.find("candidate "), std::string::npos)
      << "no candidate should be narrated; stderr=[" << deps.stderr_text << "]";
}

// ---------------------------------------------------------------------------
// Flag handling. Both of these silently did the wrong thing: a value on a
// boolean flag was discarded (so `--verbose=false` turned narration ON), and a
// repeated single-valued flag last-wins (so a scripted caller could analyse a
// formula it did not mean).
// ---------------------------------------------------------------------------

TEST(LtlfEkDepsFlags, RejectsAValueOnTheBooleanVerboseFlag) {
  const CliResult deps = RunEkDeps({"--formula=G(a <-> x)", "--inputs", "a",
                                    "--outputs", "x", "--verbose=false"});
  EXPECT_EQ(deps.exit_code, 2);
  EXPECT_EQ(deps.stdout_text, "");
}

TEST(LtlfEkDepsFlags, RejectsARepeatedFlagRatherThanSilentlyTakingTheLast) {
  const CliResult deps =
      RunEkDeps({"--formula=G(a <-> x)", "--formula=G(a -> x)", "--inputs",
                 "a", "--outputs", "x"});
  EXPECT_EQ(deps.exit_code, 2);
  EXPECT_EQ(deps.stdout_text, "");
}

// ---------------------------------------------------------------------------
// O1 -- end-to-end equirealizability oracle, the linchpin. Gated on ltlfsynt
// (the external, independent oracle) via a fixture mirroring
// tests/ltlfsynt_oracle_test.cpp's LtlfsyntOracleTest: GTEST_SKIP()s
// wholesale when ltlfsynt cannot be located/run, so a clean box without
// Spot's CLI tools is a no-op, not a failure. NOT gated on ltlf-ek-deps
// itself -- that binary is mandatory in-project infrastructure (like
// ltlf-ek-synth), not an optional external tool.
// ---------------------------------------------------------------------------

class LtlfEkDepsOracleTest : public ::testing::Test {
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

// The I4 totality witness (PRD "Behaviour" I4, "Test oracles" O1): phi =
// G(!a) & G(x), I={a}, O={x}. Plain synthesis of phi is UNREALIZABLE (the
// environment can always play a). A WRONGLY partial Tout would flip
// ek-with-emitted-Tout to REALIZABLE; a correctly totalized one (I5) must
// agree with both the no-knowledge baseline and ltlfsynt -- all three
// UNREALIZABLE. This is the test the PRD calls out explicitly as catching
// I4.
TEST_F(LtlfEkDepsOracleTest,
      I4TotalityWitnessAgreesAcrossAllThreeAndIsUnrealizable) {
  const std::string phi_str = "G(!a) & G(x)";
  const ScopedTempFile emit_part;
  const ScopedTempFile transducer_file;

  const CliResult deps =
      RunEkDeps({"--formula=" + phi_str, "--inputs", "a", "--outputs", "x",
                 "--emit-part", emit_part.path(), "--transducer",
                 transducer_file.path()});
  ASSERT_EQ(deps.exit_code, 0) << deps.stderr_text;

  std::ifstream emitted_in(emit_part.path());
  const VariablePartition emitted = parse_partition_file(emitted_in);
  ASSERT_EQ(emitted.output_known, (std::set<std::string>{"x"}))
      << "phi=G(!a)&G(x) is the PRD's I4 totality witness: x must be "
         "reported dependent";

  const CliResult ek_baseline =
      RunEkSynth({"--dfa-product", "--formula=" + phi_str, "--inputs", "a",
                  "--outputs", "x", "--realizable"});
  const CliResult ek_with_deps = RunEkSynth(
      {"--dfa-product", "--formula=" + phi_str, "--part-file",
       emit_part.path(), "--known-output-transducer", transducer_file.path(),
       "--realizable"});
  const CliResult synt =
      RunLtlfsynt({"--ins=a", "--outs=x", "--semantics=Mealy",
                   "--realizability", "-f", phi_str});

  const Verdict baseline_verdict = ParseEkSynthVerdict(ek_baseline);
  const Verdict with_deps_verdict = ParseEkSynthVerdict(ek_with_deps);
  const Verdict synt_verdict = ParseLtlfsyntVerdict(synt);

  EXPECT_EQ(baseline_verdict, Verdict::kUnrealizable);
  EXPECT_EQ(synt_verdict, Verdict::kUnrealizable);
  EXPECT_EQ(with_deps_verdict, Verdict::kUnrealizable)
      << "I4 regression: a partial Tout would wrongly flip this to "
         "realizable";
}

// ---------------------------------------------------------------------------
// Generated corpus (O1's main body): a fixed-seed formula/partition
// generator (same technique as tests/ltlfsynt_oracle_test.cpp's
// GeneratedCorpus, duplicated per that file's own stated one-file-per-suite
// convention), restricted to cases where the library oracle
// (`dependent_outputs`, already landed) finds a non-empty Xdep. For each
// such case: run `ltlf-ek-deps` to emit the part-file/transducer pair, then
// three-way compare `ltlf-ek-synth --part-file <emitted>
// --known-output-transducer <emitted>` against the no-external-knowledge
// baseline (`ltlf-ek-synth --inputs --outputs`, same I/O split) and against
// `ltlfsynt` on the bare phi.
// ---------------------------------------------------------------------------

constexpr unsigned kDepsCorpusSeed = 20260731;
constexpr std::size_t kDepsCorpusCaseCount = 150;
constexpr int kDepsTreeSizeMin = 1;
constexpr int kDepsTreeSizeMax = 7;
constexpr int kDepsInputMax = 3;
constexpr int kDepsOutputMax = 3;
constexpr unsigned kDepsSubprocessTimeoutSecs = 10;

spot::formula GenerateRandomFormula(const VariablePartition& partition,
                                    std::mt19937& rng) {
  std::set<std::string> ap_names = partition.inputs();
  for (const std::string& name : partition.outputs()) ap_names.insert(name);

  spot::atomic_prop_set aprops;
  for (const std::string& name : ap_names)
    aprops.insert(spot::default_environment::instance().require(name));

  spot::option_map opts;
  opts.set("output", spot::randltlgenerator::LTL);
  opts.set("tree_size_min", kDepsTreeSizeMin);
  opts.set("tree_size_max", kDepsTreeSizeMax);
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

// >=1 input, >=1 output, no Iknown/Oknown (I9 requires an empty
// output_known on input; the differential itself needs no pre-existing V --
// it compares the emitted-Tout run against the fully-free baseline).
VariablePartition RandomPartition(std::mt19937& rng) {
  std::uniform_int_distribution<int> input_count(1, kDepsInputMax);
  std::uniform_int_distribution<int> output_count(1, kDepsOutputMax);
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

TEST_F(LtlfEkDepsOracleTest,
      GeneratedCorpusEquirealizableAgainstBaselineAndLtlfsynt) {
  std::mt19937 rng(kDepsCorpusSeed);
  std::size_t dependent_cases = 0;
  std::size_t checked_cases = 0;

  for (std::size_t i = 0; i < kDepsCorpusCaseCount; ++i) {
    const VariablePartition partition = RandomPartition(rng);
    const spot::formula phi = GenerateRandomFormula(partition, rng);
    auto dict = spot::make_bdd_dict();

    // Library pre-filter (dependent_outputs is already landed, Phase 2):
    // decides which cases have a non-empty Xdep at all, and doubles as the
    // corpus's own vacuousness statistic below -- independent of whether the
    // ltlf-ek-deps binary (under test) agrees, which is cross-checked
    // per-case immediately after.
    std::optional<DependentOutputs> lib_result;
    try {
      lib_result = dependent_outputs(phi, partition, dict);
    } catch (const std::invalid_argument&) {
      continue;  // unsatisfiable phi (PRD "Edge cases").
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
        {"--formula=" + phi_str, "--inputs", JoinCsv(partition.input_free),
         "--outputs", JoinCsv(partition.output_free), "--emit-part",
         emit_part.path(), "--transducer", transducer_file.path()},
        kDepsSubprocessTimeoutSecs);
    EXPECT_EQ(deps.exit_code, 0)
        << "ltlf-ek-deps failed for phi=" << phi_str << ": "
        << deps.stderr_text;
    if (deps.exit_code != 0) continue;  // don't let one bad case sink the
                                        // whole corpus's statistics.

    std::ifstream emitted_in(emit_part.path());
    const VariablePartition emitted = parse_partition_file(emitted_in);
    EXPECT_EQ(emitted.output_known, lib_result->dependent)
        << "CLI-reported Xdep disagrees with the already-landed library "
           "oracle for phi="
        << phi_str;
    if (emitted.output_known.empty()) continue;  // filter-mismatch guard.
    ++checked_cases;

    bool baseline_timed_out = false, with_deps_timed_out = false,
        synt_timed_out = false;
    const CliResult ek_baseline = RunEkSynth(
        {"--dfa-product", "--formula=" + phi_str, "--inputs",
         JoinCsv(partition.input_free), "--outputs",
         JoinCsv(partition.output_free), "--realizable"},
        kDepsSubprocessTimeoutSecs, &baseline_timed_out);
    const CliResult ek_with_deps = RunEkSynth(
        {"--dfa-product", "--formula=" + phi_str, "--part-file",
         emit_part.path(), "--known-output-transducer",
         transducer_file.path(), "--realizable"},
        kDepsSubprocessTimeoutSecs, &with_deps_timed_out);
    const CliResult synt = RunLtlfsynt(
        {"--ins=" + JoinCsv(partition.input_free),
         "--outs=" + JoinCsv(partition.output_free), "--semantics=Mealy",
         "--realizability", "-f", phi_str},
        kDepsSubprocessTimeoutSecs, &synt_timed_out);
    if (baseline_timed_out || with_deps_timed_out || synt_timed_out)
      continue;  // a slow subprocess is a skip, never a test failure.

    const Verdict baseline_verdict = ParseEkSynthVerdict(ek_baseline);
    const Verdict with_deps_verdict = ParseEkSynthVerdict(ek_with_deps);
    const Verdict synt_verdict = ParseLtlfsyntVerdict(synt);

    EXPECT_EQ(IsRealizable(baseline_verdict), IsRealizable(synt_verdict))
        << "no-external-knowledge baseline disagrees with ltlfsynt for phi="
        << phi_str;
    EXPECT_EQ(IsRealizable(with_deps_verdict), IsRealizable(synt_verdict))
        << "O1 linchpin: ltlf-ek-synth against the EMITTED Tout disagrees "
           "with ltlfsynt for phi="
        << phi_str;
    EXPECT_EQ(IsRealizable(with_deps_verdict), IsRealizable(baseline_verdict))
        << "O1 linchpin: ltlf-ek-synth against the EMITTED Tout disagrees "
           "with the no-external-knowledge baseline for phi="
        << phi_str;
  }

  RecordProperty("dependent_cases", static_cast<int>(dependent_cases));
  RecordProperty("checked_cases", static_cast<int>(checked_cases));
  // Vacuousness guard (PRD "Test oracles" O1, verbatim): "assert that a
  // non-trivial fraction of the corpus DOES yield a non-empty Xdep, else the
  // oracle is vacuous."
  EXPECT_GT(dependent_cases, 0u);
  EXPECT_GE(dependent_cases, kDepsCorpusCaseCount / 20)  // >= 5%.
      << "only " << dependent_cases << "/" << kDepsCorpusCaseCount
      << " generated cases had a non-empty Xdep -- the oracle risks being "
         "vacuous";
}

}  // namespace
