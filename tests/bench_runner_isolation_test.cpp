#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include <csignal>
#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>

#include <gtest/gtest.h>

#include "ltlf_ek/detail/util.hpp"

#ifndef LTLF_EK_BENCH_BINARY
#error "LTLF_EK_BENCH_BINARY must be defined by CMake (see CMakeLists.txt)"
#endif
#ifndef LTLF_EK_REPO_ROOT
#error "LTLF_EK_REPO_ROOT must be defined by CMake (see CMakeLists.txt)"
#endif

// CLI-level tests for docs/prd/benchmark-suite.md's "Phase 2 (cont. II) ---
// per-case process isolation in `ltlf-ek-bench`" (2026-08-23, uncommitted).
// `RunCaseWithTimeout` (src/ltlf_ek_bench.cpp) forks a child per case and
// SIGKILLs+reaps it on a deadline miss, replacing the old detached-thread
// strategy whose abandoned Spot/MONA state crashed the *parent* (SIGSEGV,
// exit 139) with no JSON written at all. That rewrite lives in the binary's
// own anonymous namespace, so it is not linkable from `unit_tests` --- the
// only honest coverage is subprocess-level, driving the built `ltlf-ek-bench`
// binary exactly as tests/slippery_world_test.cpp's ltlfsynt-race test does
// (RunCli/ScopedTempFile/ShellQuote are duplicated from there rather than
// shared, this project's one-file-per-suite norm --- see
// tests/ltlfsynt_oracle_test.cpp's ShellQuote/CliResult comment).
//
// The one cell known (PRD "Developer comments", 2026-08-23 entry; also
// docs/prd/engineered-domain-families.md Phase 1 finding #2) to run far
// longer than any short --timeout is `nfa-product` on `slippery-onehot` at
// n=3 (MONA-backed construction, >90s measured, up to ~7 min/goal) --- this
// is the cheapest reliable trip: at --timeout=3 the whole sweep (that cell
// plus n=2/n=3 dfa-product, which all complete in well under a second) exits
// in a few seconds, so the suite stays cheap. No test here asserts a timing
// ratio or an absolute duration (PRD B1); the only wall-clock fact checked is
// that the CLI subprocess does not hang past its own generous outer bound.
namespace {

// ---------------------------------------------------------------------------
// Minimal generic JSON parser, duplicated from tests/bench_test.cpp's
// JsonValue/JsonParser (schema-agnostic; supports objects, arrays, strings,
// numbers, booleans, null --- exactly what src/ltlf_ek_bench.cpp's
// hand-written `json << ...` emitter produces).
// ---------------------------------------------------------------------------

struct JsonValue {
  enum class Kind { kObject, kArray, kString, kNumber, kBool, kNull };
  Kind kind = Kind::kNull;
  std::map<std::string, JsonValue> object_fields;
  std::vector<JsonValue> array_items;
  std::string string_value;
  bool bool_value = false;
  double number_value = 0.0;
};

class JsonParser {
 public:
  explicit JsonParser(std::string text) : text_(std::move(text)) {}

  JsonValue Parse() {
    SkipWhitespace();
    JsonValue v = ParseValue();
    SkipWhitespace();
    if (pos_ != text_.size())
      throw std::runtime_error(
          "trailing content after the JSON value at offset " +
          std::to_string(pos_));
    return v;
  }

 private:
  std::string text_;
  std::size_t pos_ = 0;

  char Peek() const {
    if (pos_ >= text_.size())
      throw std::runtime_error("unexpected end of JSON input");
    return text_[pos_];
  }
  void Expect(char c) {
    if (Peek() != c)
      throw std::runtime_error(std::string("expected '") + c + "' at offset " +
                               std::to_string(pos_));
    ++pos_;
  }
  bool Consume(const std::string& literal) {
    if (text_.compare(pos_, literal.size(), literal) == 0) {
      pos_ += literal.size();
      return true;
    }
    return false;
  }
  void SkipWhitespace() {
    while (pos_ < text_.size() &&
          std::isspace(static_cast<unsigned char>(text_[pos_])))
      ++pos_;
  }

  JsonValue ParseValue() {
    SkipWhitespace();
    const char c = Peek();
    if (c == '{') return ParseObject();
    if (c == '[') return ParseArray();
    if (c == '"') return ParseString();
    if (c == 't' || c == 'f') return ParseBool();
    if (c == 'n') return ParseNull();
    return ParseNumber();
  }

  JsonValue ParseObject() {
    Expect('{');
    JsonValue v;
    v.kind = JsonValue::Kind::kObject;
    SkipWhitespace();
    if (Peek() == '}') { ++pos_; return v; }
    while (true) {
      SkipWhitespace();
      const JsonValue key = ParseString();
      SkipWhitespace();
      Expect(':');
      JsonValue val = ParseValue();
      v.object_fields.emplace(key.string_value, std::move(val));
      SkipWhitespace();
      if (Peek() == ',') { ++pos_; continue; }
      Expect('}');
      break;
    }
    return v;
  }

  JsonValue ParseArray() {
    Expect('[');
    JsonValue v;
    v.kind = JsonValue::Kind::kArray;
    SkipWhitespace();
    if (Peek() == ']') { ++pos_; return v; }
    while (true) {
      v.array_items.push_back(ParseValue());
      SkipWhitespace();
      if (Peek() == ',') { ++pos_; continue; }
      Expect(']');
      break;
    }
    return v;
  }

  JsonValue ParseString() {
    Expect('"');
    JsonValue v;
    v.kind = JsonValue::Kind::kString;
    std::string out;
    while (true) {
      if (pos_ >= text_.size())
        throw std::runtime_error("unterminated JSON string");
      const char c = text_[pos_++];
      if (c == '"') break;
      if (c == '\\') {
        if (pos_ >= text_.size())
          throw std::runtime_error("unterminated JSON string escape");
        const char esc = text_[pos_++];
        switch (esc) {
          case '"': out += '"'; break;
          case '\\': out += '\\'; break;
          case '/': out += '/'; break;
          case 'n': out += '\n'; break;
          case 't': out += '\t'; break;
          case 'r': out += '\r'; break;
          case 'b': out += '\b'; break;
          case 'f': out += '\f'; break;
          case 'u': pos_ += 4; out += '?'; break;  // schema has no \u escapes.
          default:
            throw std::runtime_error("unsupported JSON string escape");
        }
      } else {
        out += c;
      }
    }
    v.string_value = out;
    return v;
  }

  JsonValue ParseBool() {
    JsonValue v;
    v.kind = JsonValue::Kind::kBool;
    if (Consume("true")) { v.bool_value = true; return v; }
    if (Consume("false")) { v.bool_value = false; return v; }
    throw std::runtime_error("expected true/false at offset " +
                             std::to_string(pos_));
  }

  JsonValue ParseNull() {
    if (!Consume("null"))
      throw std::runtime_error("expected null at offset " +
                               std::to_string(pos_));
    JsonValue v;
    v.kind = JsonValue::Kind::kNull;
    return v;
  }

  JsonValue ParseNumber() {
    const std::size_t start = pos_;
    if (pos_ < text_.size() && text_[pos_] == '-') ++pos_;
    while (pos_ < text_.size() &&
          std::isdigit(static_cast<unsigned char>(text_[pos_])))
      ++pos_;
    if (pos_ < text_.size() && text_[pos_] == '.') {
      ++pos_;
      while (pos_ < text_.size() &&
            std::isdigit(static_cast<unsigned char>(text_[pos_])))
        ++pos_;
    }
    if (pos_ < text_.size() && (text_[pos_] == 'e' || text_[pos_] == 'E')) {
      ++pos_;
      if (pos_ < text_.size() && (text_[pos_] == '+' || text_[pos_] == '-'))
        ++pos_;
      while (pos_ < text_.size() &&
            std::isdigit(static_cast<unsigned char>(text_[pos_])))
        ++pos_;
    }
    if (pos_ == start)
      throw std::runtime_error("expected a JSON value at offset " +
                               std::to_string(pos_));
    JsonValue v;
    v.kind = JsonValue::Kind::kNumber;
    v.number_value = std::stod(text_.substr(start, pos_ - start));
    return v;
  }
};

// ---------------------------------------------------------------------------
// Subprocess plumbing, duplicated from tests/slippery_world_test.cpp (which
// duplicates it from tests/ltlf_ek_synth_test.cpp in turn --- the same
// one-file-per-suite norm).
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
  explicit ScopedTempFile() {
    path_ = ltlf_ek::detail::temp_template("bench_runner_isolation_test");
    const int fd = mkstemp(path_.data());
    EXPECT_GE(fd, 0) << "mkstemp failed for " << path_;
    if (fd >= 0) close(fd);
  }
  ~ScopedTempFile() { std::remove(path_.c_str()); }
  const std::string& path() const { return path_; }

 private:
  std::string path_;
};

// Runs `binary` with `args` via a POSIX shell, bounded by a generous
// wall-clock `timeout_secs` (coreutils `timeout`) so a genuine hang shows up
// as a killed subprocess (exit_code == -1) rather than blocking `ctest`
// forever.
CliResult RunCli(const std::string& binary, const std::vector<std::string>& args,
                 unsigned timeout_secs) {
  ScopedTempFile out_capture, err_capture;
  std::ostringstream cmd;
  cmd << "timeout " << timeout_secs << "s " << ShellQuote(binary);
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

// ---------------------------------------------------------------------------
// The one shared sweep every test in this file reads from --- a single
// `ltlf-ek-bench` invocation, run at most once per process (function-local
// static, initialized on first use) so the ~7s MONA-backed cost of tripping
// the timeout is paid once, not once per TEST. `slippery-onehot`'s
// `nfa-product` cell at n=3 is the known-slow trip (see file header);
// `dfa-product` (both n) and `nfa-product` at n=2 are the "everything else
// finished for real" control cells.
// ---------------------------------------------------------------------------

struct SweepRun {
  CliResult cli;
  std::string out_path;
  std::string report_text;
  bool json_parsed = false;
  JsonValue report;
};

const SweepRun& RunFamilySweepWithShortTimeout() {
  static const SweepRun run = [] {
    SweepRun r;
    r.out_path = std::string(LTLF_EK_REPO_ROOT) +
                "/build/benchout/runner_isolation_timeout.json";
    r.cli = RunCli(
        LTLF_EK_BENCH_BINARY,
        {"--families=slippery-onehot", "--subjects=nfa-product,dfa-product",
         "--n-min=2", "--n-max=3", "--repeat=1", "--timeout=3",
         "--out=" + r.out_path},
        /*timeout_secs=*/60);
    if (r.cli.exit_code == 0) {
      std::ifstream in(r.out_path);
      std::ostringstream ss;
      ss << in.rdbuf();
      r.report_text = ss.str();
      try {
        r.report = JsonParser(r.report_text).Parse();
        r.json_parsed = true;
      } catch (const std::exception&) {
        r.json_parsed = false;
      }
    }
    return r;
  }();
  return run;
}

// Rows of the `timings` (or `structural`) array matching every key/value
// pair given in `filter` --- a tiny hand-rolled predicate join, not a general
// query engine, since the schema here is a flat array of flat objects.
std::vector<const JsonValue*> MatchingRows(
    const JsonValue& array,
    const std::vector<std::pair<std::string, std::string>>& string_filter,
    const std::vector<std::pair<std::string, long long>>& int_filter) {
  std::vector<const JsonValue*> out;
  for (const JsonValue& row : array.array_items) {
    bool ok = true;
    for (const auto& [key, expected] : string_filter) {
      const auto it = row.object_fields.find(key);
      if (it == row.object_fields.end() ||
          it->second.string_value != expected) {
        ok = false;
        break;
      }
    }
    if (ok) {
      for (const auto& [key, expected] : int_filter) {
        const auto it = row.object_fields.find(key);
        if (it == row.object_fields.end() ||
            static_cast<long long>(it->second.number_value) != expected) {
          ok = false;
          break;
        }
      }
    }
    if (ok) out.push_back(&row);
  }
  return out;
}

// ---------------------------------------------------------------------------
// Fault-injection sweep runner, for the kFailed path (LTLF_EK_BENCH_FAULT_INJECT,
// see MaybeInjectFault's own comment in src/ltlf_ek_bench.cpp). Not cached
// (each fault-injection test wants its own env var + args), unlike
// RunFamilySweepWithShortTimeout above --- but cheap regardless, since these
// tests use cons-prunes/dfa-product (no MONA, no realistic construction cost)
// rather than the MONA-backed slow cell.
// ---------------------------------------------------------------------------

std::string UniqueOutPath(const std::string& label) {
  return std::string(LTLF_EK_REPO_ROOT) + "/build/benchout/runner_isolation_" +
        label + "_" + std::to_string(::getpid()) + ".json";
}

SweepRun RunFaultInjectedSweep(const std::string& fault_spec,
                               const std::vector<std::string>& extra_args,
                               const std::string& label) {
  SweepRun r;
  r.out_path = UniqueOutPath(label);
  ::setenv("LTLF_EK_BENCH_FAULT_INJECT", fault_spec.c_str(), /*overwrite=*/1);
  std::vector<std::string> args = extra_args;
  args.push_back("--out=" + r.out_path);
  r.cli = RunCli(LTLF_EK_BENCH_BINARY, args, /*timeout_secs=*/30);
  ::unsetenv("LTLF_EK_BENCH_FAULT_INJECT");
  if (r.cli.exit_code == 0) {
    std::ifstream in(r.out_path);
    std::ostringstream ss;
    ss << in.rdbuf();
    r.report_text = ss.str();
    try {
      r.report = JsonParser(r.report_text).Parse();
      r.json_parsed = true;
    } catch (const std::exception&) {
      r.json_parsed = false;
    }
  }
  return r;
}

// ---------------------------------------------------------------------------
// Process-table sampling, for Property 4 (no orphaned running descendant
// after a deadline-miss kill). `comm=`/`pid=`/... (trailing '=', no column
// name) is the POSIX way to suppress ps's header, so this is a plain
// whitespace-split per line, no header line to skip.
// ---------------------------------------------------------------------------

struct PsRow {
  long long pid = 0;
  long long ppid = 0;
  std::string stat;
  std::string comm;
};

// NOTE: pgid is deliberately not sampled --- inside this project's bwrap
// sandbox, `ps`'s PGID column reads 0 for every process (a PID-namespace
// reporting artifact, not a real group id), which makes process-group
// correlation silently useless here even though it works on an unsandboxed
// host. PPID is reported correctly, so Property 4 below tracks descendants
// by walking the PPID chain instead.
std::vector<PsRow> SampleProcessTable() {
  std::vector<PsRow> rows;
  std::FILE* ps = popen("ps -eo pid=,ppid=,stat=,comm=", "r");
  if (!ps) return rows;
  std::string text;
  std::array<char, 8192> buf{};
  std::size_t n = 0;
  while ((n = std::fread(buf.data(), 1, buf.size(), ps)) > 0)
    text.append(buf.data(), n);
  pclose(ps);
  std::istringstream lines(text);
  std::string line;
  while (std::getline(lines, line)) {
    std::istringstream ls(line);
    PsRow row;
    if (ls >> row.pid >> row.ppid >> row.stat >> row.comm)
      rows.push_back(row);
  }
  return rows;
}

// Forks+execv's `ltlf-ek-bench` directly (no shell, no `timeout` wrapper),
// so the caller gets the real child pid to poll/watch --- RunCli's
// system()-through-a-shell plumbing above hides that pid, which Property 4
// needs to correlate observed process-group ids against.
pid_t SpawnBenchDirect(const std::vector<std::string>& args,
                       const std::string& out_capture_path,
                       const std::string& err_capture_path) {
  std::vector<std::string> arg_storage = args;
  std::vector<char*> argv;
  argv.push_back(const_cast<char*>(LTLF_EK_BENCH_BINARY));
  for (auto& a : arg_storage) argv.push_back(a.data());
  argv.push_back(nullptr);

  const pid_t pid = fork();
  if (pid == 0) {
    const int out_fd =
        open(out_capture_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600);
    const int err_fd =
        open(err_capture_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (out_fd >= 0) dup2(out_fd, STDOUT_FILENO);
    if (err_fd >= 0) dup2(err_fd, STDERR_FILENO);
    execv(LTLF_EK_BENCH_BINARY, argv.data());
    _exit(127);  // execv itself failed
  }
  return pid;
}

}  // namespace

// ---------------------------------------------------------------------------
// Property 1 --- the regression this phase exists to fix: a sweep containing
// a cell that cannot finish inside a short --timeout still exits 0 and
// writes a complete, schema-valid JSON. Pre-fix this was exit 139 (SIGSEGV
// from the abandoned detached thread) with no --out file at all.
// ---------------------------------------------------------------------------

TEST(BenchRunnerIsolation, ATimingOutCellStillExitsZeroAndWritesACompleteSchemaValidJson) {
#ifndef MONA_FOUND
  GTEST_SKIP() << "mona not found (CMake find_program(mona)); slippery-onehot's "
                  "nfa-product subject needs mona to actually attempt (and "
                  "time out on) n=3, the cell this suite relies on to trip "
                  "the short --timeout";
#endif
  const SweepRun& run = RunFamilySweepWithShortTimeout();
  ASSERT_EQ(run.cli.exit_code, 0)
      << "a timing-out cell must not kill the run; stdout=[" << run.cli.stdout_text
      << "] stderr=[" << run.cli.stderr_text << "]";
  ASSERT_TRUE(run.json_parsed)
      << "--out must contain a complete, parsable JSON document; raw text=["
      << run.report_text << "]";

  ASSERT_EQ(run.report.kind, JsonValue::Kind::kObject);
  for (const std::string& key :
       {"provenance", "timings", "structural", "ltlfsynt", "summary"}) {
    ASSERT_TRUE(run.report.object_fields.count(key))
        << "top-level report is missing the '" << key << "' section";
  }
  EXPECT_EQ(run.report.object_fields.at("provenance").kind,
           JsonValue::Kind::kObject);
  EXPECT_EQ(run.report.object_fields.at("timings").kind, JsonValue::Kind::kArray);
  EXPECT_EQ(run.report.object_fields.at("structural").kind,
           JsonValue::Kind::kArray);
  EXPECT_FALSE(run.report.object_fields.at("timings").array_items.empty())
      << "the timings section must not be empty --- a crash-before-writing "
        "run would leave it so";
  EXPECT_FALSE(run.report.object_fields.at("structural").array_items.empty())
      << "the structural section must not be empty either";
}

// ---------------------------------------------------------------------------
// Property 2 --- the timed-out cell is marked as such, and every other cell
// in the same sweep carries a full real stage/metric row set (B2 rule 1:
// absent metrics, never zeros --- the timed-out cell's structural rows must
// be ABSENT, not present-and-zero).
// ---------------------------------------------------------------------------

TEST(BenchRunnerIsolation, TheTimedOutCellIsMarkedAndEveryOtherCellCarriesRealMeasurements) {
#ifndef MONA_FOUND
  GTEST_SKIP() << "mona not found; see the other test in this file for why "
                  "this suite needs it";
#endif
  const SweepRun& run = RunFamilySweepWithShortTimeout();
  ASSERT_EQ(run.cli.exit_code, 0) << "stderr=[" << run.cli.stderr_text << "]";
  ASSERT_TRUE(run.json_parsed);
  const JsonValue& timings = run.report.object_fields.at("timings");
  const JsonValue& structural = run.report.object_fields.at("structural");

  // The known-slow cell: both realizability polarities of slippery-onehot
  // n=3 nfa-product must be marked TIMEOUT in `timings` and must be ABSENT
  // (not zero) from `structural`.
  for (const bool realizable : {true, false}) {
    SCOPED_TRACE(std::string("realizable=") + (realizable ? "true" : "false"));
    const std::vector<const JsonValue*> rows = MatchingRows(
        timings,
        {{"family", "slippery-onehot"}, {"subject", "nfa-product"}},
        {{"n", 3}});
    // realizable is a bool field, not filtered by MatchingRows' int_filter;
    // narrow it by hand.
    bool found = false;
    for (const JsonValue* row : rows) {
      const auto rit = row->object_fields.find("realizable");
      ASSERT_NE(rit, row->object_fields.end());
      if (rit->second.bool_value != realizable) continue;
      found = true;
      const auto stage_it = row->object_fields.find("stage");
      ASSERT_NE(stage_it, row->object_fields.end());
      EXPECT_EQ(stage_it->second.string_value, "TIMEOUT")
          << "n=3 nfa-product on slippery-onehot must be marked TIMEOUT";
      const auto timed_out_it = row->object_fields.find("timed_out");
      ASSERT_NE(timed_out_it, row->object_fields.end());
      EXPECT_TRUE(timed_out_it->second.bool_value);
    }
    EXPECT_TRUE(found) << "expected a TIMEOUT timing row for slippery-onehot "
                          "n=3 nfa-product realizable=" << realizable;

    const std::vector<const JsonValue*> struct_rows = MatchingRows(
        structural,
        {{"family", "slippery-onehot"}, {"subject", "nfa-product"}},
        {{"n", 3}});
    int matching_polarity = 0;
    for (const JsonValue* row : struct_rows) {
      const auto rit = row->object_fields.find("realizable");
      if (rit != row->object_fields.end() &&
          rit->second.bool_value == realizable)
        ++matching_polarity;
    }
    EXPECT_EQ(matching_polarity, 0)
        << "a timed-out cell must emit NO structural rows (absent, never "
          "zero) --- got " << matching_polarity;
  }

  // The control cells: every subject/n combination that is NOT the known-slow
  // one must carry real stage rows with a real (nonzero) elapsed time and
  // must not be marked as timed out.
  struct Cell { std::string subject; long long n; };
  const std::vector<Cell> control_cells = {
      {"dfa-product", 2}, {"dfa-product", 3}, {"nfa-product", 2}};
  for (const Cell& cell : control_cells) {
    SCOPED_TRACE("subject=" + cell.subject + " n=" + std::to_string(cell.n));
    const std::vector<const JsonValue*> rows = MatchingRows(
        timings, {{"family", "slippery-onehot"}, {"subject", cell.subject}},
        {{"n", cell.n}});
    ASSERT_FALSE(rows.empty()) << "expected timing rows for this control cell";
    for (const JsonValue* row : rows) {
      const auto timed_out_it = row->object_fields.find("timed_out");
      ASSERT_NE(timed_out_it, row->object_fields.end());
      EXPECT_FALSE(timed_out_it->second.bool_value)
          << "a control cell must not be marked timed out";
      const auto stage_it = row->object_fields.find("stage");
      ASSERT_NE(stage_it, row->object_fields.end());
      EXPECT_NE(stage_it->second.string_value, "TIMEOUT");
      const auto ns_it = row->object_fields.find("ns");
      ASSERT_NE(ns_it, row->object_fields.end());
      EXPECT_GT(ns_it->second.number_value, 0.0)
          << "a completed cell's stage row must carry a real, nonzero "
            "elapsed time (B2 rule 1: absent metrics, never zeros)";
    }
    const std::vector<const JsonValue*> struct_rows = MatchingRows(
        structural, {{"family", "slippery-onehot"}, {"subject", cell.subject}},
        {{"n", cell.n}});
    EXPECT_FALSE(struct_rows.empty())
        << "a completed cell must carry real structural (size) rows";
  }
}

// ---------------------------------------------------------------------------
// Property 3 --- the kFailed path itself (PRD "Phase 2 (cont. II)"'s own
// motivating case: surviving a child that dies by signal or exits nonzero,
// not just a slow child). Forced via LTLF_EK_BENCH_FAULT_INJECT, test-only
// (see MaybeInjectFault's comment in src/ltlf_ek_bench.cpp), scoped with
// ":<k>" to exactly the 2nd of 4 RunCaseWithTimeout calls in the sweep
// below (--families=cons-prunes --subjects=dfa-product --n-min=2 --n-max=3
// --repeat=1: n ascending outer, realizable false-then-true inner, so call
// order is (n=2,F), (n=2,T), (n=3,F), (n=3,T) --- call #2 is (n=2,T)). The
// scoping is what lets this test also check the sweep's *other* three cells
// stayed real, not just that the crashed one is marked --- cons-prunes/
// dfa-product needs no MONA and no realistic construction cost, so these run
// unconditionally (no MONA_FOUND guard).
// ---------------------------------------------------------------------------

void ExpectCompleteCrashMarksFailedAndOthersStayReal(
    const std::string& fault_spec, const std::string& expected_stage,
    const std::string& label) {
  const SweepRun run = RunFaultInjectedSweep(
      fault_spec,
      {"--families=cons-prunes", "--subjects=dfa-product", "--n-min=2",
       "--n-max=3", "--repeat=1"},
      label);
  ASSERT_EQ(run.cli.exit_code, 0)
      << "a crashed child must not kill the sweep; stdout=["
      << run.cli.stdout_text << "] stderr=[" << run.cli.stderr_text << "]";
  ASSERT_TRUE(run.json_parsed)
      << "--out must still contain a complete, parsable JSON document; raw "
        "text=[" << run.report_text << "]";
  const JsonValue& timings = run.report.object_fields.at("timings");
  const JsonValue& structural = run.report.object_fields.at("structural");

  const std::vector<const JsonValue*> crashed_rows = MatchingRows(
      timings, {{"family", "cons-prunes"}, {"subject", "dfa-product"}},
      {{"n", 2}});
  bool found = false;
  for (const JsonValue* row : crashed_rows) {
    const auto rit = row->object_fields.find("realizable");
    ASSERT_NE(rit, row->object_fields.end());
    if (!rit->second.bool_value) continue;  // (n=2,F) is call #1, unaffected
    found = true;
    const auto stage_it = row->object_fields.find("stage");
    ASSERT_NE(stage_it, row->object_fields.end());
    EXPECT_EQ(stage_it->second.string_value, expected_stage)
        << "the faulted child must be marked by its failure detail";
    const auto timed_out_it = row->object_fields.find("timed_out");
    ASSERT_NE(timed_out_it, row->object_fields.end());
    EXPECT_TRUE(timed_out_it->second.bool_value);
    const auto ns_it = row->object_fields.find("ns");
    ASSERT_NE(ns_it, row->object_fields.end());
    EXPECT_EQ(ns_it->second.number_value, 0.0)
        << "a FAILED_ marker row itself carries ns=0 by construction (same "
          "shape as the TIMEOUT marker) --- this is the marker's own "
          "encoding, not the metrics-absence property checked below";
  }
  EXPECT_TRUE(found) << "expected a " << expected_stage
                     << " row for the faulted cell (n=2, realizable=true)";

  const std::vector<const JsonValue*> crashed_struct = MatchingRows(
      structural, {{"family", "cons-prunes"}, {"subject", "dfa-product"}},
      {{"n", 2}});
  int crashed_polarity = 0;
  for (const JsonValue* row : crashed_struct) {
    const auto rit = row->object_fields.find("realizable");
    if (rit != row->object_fields.end() && rit->second.bool_value)
      ++crashed_polarity;
  }
  EXPECT_EQ(crashed_polarity, 0)
      << "PRD B2 rule 1: a failed cell's metrics must be ABSENT, never "
        "zero --- got " << crashed_polarity << " structural rows";

  // Every other cell in the same sweep ((n=2,F), (n=3,F), (n=3,T)) must
  // still carry real, nonzero measurements: the crash must not bleed into
  // the rest of the sweep.
  struct Cell { long long n; bool realizable; };
  const std::vector<Cell> other_cells = {{2, false}, {3, false}, {3, true}};
  for (const Cell& cell : other_cells) {
    SCOPED_TRACE("n=" + std::to_string(cell.n) +
                " realizable=" + (cell.realizable ? "true" : "false"));
    const std::vector<const JsonValue*> rows = MatchingRows(
        timings, {{"family", "cons-prunes"}, {"subject", "dfa-product"}},
        {{"n", cell.n}});
    bool row_found = false;
    for (const JsonValue* row : rows) {
      const auto rit = row->object_fields.find("realizable");
      ASSERT_NE(rit, row->object_fields.end());
      if (rit->second.bool_value != cell.realizable) continue;
      row_found = true;
      const auto stage_it = row->object_fields.find("stage");
      ASSERT_NE(stage_it, row->object_fields.end());
      EXPECT_NE(stage_it->second.string_value.rfind("FAILED_", 0), 0u)
          << "an unfaulted cell must not carry a FAILED_ marker";
      const auto ns_it = row->object_fields.find("ns");
      ASSERT_NE(ns_it, row->object_fields.end());
      EXPECT_GT(ns_it->second.number_value, 0.0)
          << "an unfaulted cell's stage/summary row must carry a real, "
            "nonzero elapsed time (B2 rule 1)";
    }
    EXPECT_TRUE(row_found) << "expected timing rows for this control cell";

    const std::vector<const JsonValue*> struct_rows = MatchingRows(
        structural, {{"family", "cons-prunes"}, {"subject", "dfa-product"}},
        {{"n", cell.n}});
    int matching = 0;
    for (const JsonValue* row : struct_rows) {
      const auto rit = row->object_fields.find("realizable");
      if (rit != row->object_fields.end() &&
          rit->second.bool_value == cell.realizable)
        ++matching;
    }
    EXPECT_GT(matching, 0)
        << "a completed cell must carry real structural (size) rows";
  }
}

TEST(BenchRunnerIsolation, ASignalKilledChildIsMarkedFailedSignal11AndOthersStayReal) {
  ExpectCompleteCrashMarksFailedAndOthersStayReal("segv:2", "FAILED_signal_11",
                                                  "fault_segv");
}

TEST(BenchRunnerIsolation, ANonzeroExitChildIsMarkedFailedExit1AndOthersStayReal) {
  ExpectCompleteCrashMarksFailedAndOthersStayReal("exit1:2", "FAILED_exit_1",
                                                  "fault_exit1");
}

// ---------------------------------------------------------------------------
// Property 3b --- a partial failure (some repeats of one cell crash, others
// succeed) is marked PARTIAL_FAILED_<k>_of_<K> while keeping the successful
// repeats' real measurements (PRD "Edge cases": "Structural rows already
// gathered for that case stay valid" --- the same rows-still-valid guarantee
// PARTIAL_TIMEOUT already has). --repeat=3 on a single cell, fault scoped to
// call #2 (the 2nd of that cell's 3 repeats; realizable=false runs first, so
// calls 1-3 are its repeats and calls 4-6 belong to realizable=true, entirely
// unaffected).
// ---------------------------------------------------------------------------

TEST(BenchRunnerIsolation, APartialFailureIsMarkedPartialFailedAndKeepsTheSuccessfulRepeatsMeasurements) {
  const SweepRun run = RunFaultInjectedSweep(
      "exit1:2",
      {"--families=cons-prunes", "--subjects=dfa-product", "--n-min=2",
       "--n-max=2", "--repeat=3"},
      "fault_partial");
  ASSERT_EQ(run.cli.exit_code, 0)
      << "stdout=[" << run.cli.stdout_text << "] stderr=["
      << run.cli.stderr_text << "]";
  ASSERT_TRUE(run.json_parsed) << "raw text=[" << run.report_text << "]";
  const JsonValue& timings = run.report.object_fields.at("timings");
  const JsonValue& structural = run.report.object_fields.at("structural");

  const std::vector<const JsonValue*> rows = MatchingRows(
      timings, {{"family", "cons-prunes"}, {"subject", "dfa-product"}},
      {{"n", 2}});
  bool marker_found = false;
  bool real_row_found = false;
  int unaffected_partial_markers = 0;
  for (const JsonValue* row : rows) {
    const auto rit = row->object_fields.find("realizable");
    ASSERT_NE(rit, row->object_fields.end());
    const auto stage_it = row->object_fields.find("stage");
    ASSERT_NE(stage_it, row->object_fields.end());
    if (!rit->second.bool_value) {
      // realizable=false: the faulted cell (calls #1-3).
      if (stage_it->second.string_value == "PARTIAL_FAILED_1_of_3") {
        marker_found = true;
        const auto timed_out_it = row->object_fields.find("timed_out");
        ASSERT_NE(timed_out_it, row->object_fields.end());
        EXPECT_TRUE(timed_out_it->second.bool_value);
      } else {
        real_row_found = true;
        const auto ns_it = row->object_fields.find("ns");
        ASSERT_NE(ns_it, row->object_fields.end());
        EXPECT_GT(ns_it->second.number_value, 0.0)
            << "the two surviving repeats must still yield a real "
              "min-of-K measurement";
      }
    } else if (stage_it->second.string_value.rfind("PARTIAL_FAILED", 0) == 0) {
      // realizable=true (calls #4-6): the fault is scoped to call #2 only.
      ++unaffected_partial_markers;
    }
  }
  EXPECT_TRUE(marker_found) << "expected a PARTIAL_FAILED_1_of_3 marker row";
  EXPECT_TRUE(real_row_found)
      << "the partially-failed cell must still carry real stage rows from "
        "its 2 surviving repeats";
  EXPECT_EQ(unaffected_partial_markers, 0)
      << "the fault is scoped to call #2 only; realizable=true's cell must "
        "not see any PARTIAL_FAILED marker";

  const std::vector<const JsonValue*> struct_rows = MatchingRows(
      structural, {{"family", "cons-prunes"}, {"subject", "dfa-product"}},
      {{"n", 2}});
  int partial_struct = 0;
  for (const JsonValue* row : struct_rows) {
    const auto rit = row->object_fields.find("realizable");
    if (rit != row->object_fields.end() && !rit->second.bool_value)
      ++partial_struct;
  }
  EXPECT_GT(partial_struct, 0)
      << "the rows-still-valid guarantee extends to structural rows too "
        "--- a partial failure must not blank out the successful repeats' "
        "structural measurements";
}

// ---------------------------------------------------------------------------
// Property 4 --- no leaked children. The old version of this test (see git
// history) grepped `ps` for zombies only *after* `RunCli`'s subprocess had
// already exited --- by which point any unreaped child had already been
// reparented to init and reaped by it, so the check could not fail even if
// `waitpid` were deleted from RunCaseWithTimeout entirely. This version polls
// the process table *while* the spawned `ltlf-ek-bench` is still running,
// tracking the full descendant set by walking the PPID chain each poll (not
// by comm name or Z state, and not by process-group id --- SampleProcessTable
// above explains why pgid isn't usable in this sandbox): once a pid is seen
// as a descendant it stays tracked by that pid even after a later reparent
// (e.g. a MONA grandchild outliving the direct child that spawned it), which
// is exactly what makes this check able to catch a regression the old one
// structurally could not: after the spawned process has exited and been
// reaped by *this* test, no previously-seen descendant pid may still be
// running (R/S).
//
// Manually traced (see this phase's Developer comments): for this specific
// cell/--timeout, the ~400s the PRD measures for slippery-onehot n=3
// nfa-product is dominated by in-process NFA determinization (Spot, not
// MONA) --- so at --timeout=3 the process actually observed and killed is
// the RunCaseWithTimeout fork itself (comm "ltlf-ek-bench"), not a `mona`
// grandchild; MONA is invoked later in that cell's real (~400s) run than
// this test's timeout ever reaches. That's still the real fork+setpgid+kill
// path under test --- a `mona` grandchild would additionally be covered by
// the same pgid-independent, PPID-chain tracking below if one happened to be
// alive at kill time; it is simply not this cell's bottleneck at --timeout=3.
// ---------------------------------------------------------------------------

TEST(BenchRunnerIsolation, NoOrphanedRunningDescendantSurvivesADeadlineMissKill) {
#ifndef MONA_FOUND
  GTEST_SKIP() << "mona not found; this test needs the long-running "
                  "nfa-product cell (see the file header) to give a "
                  "kill-and-reap regression something observable to leak";
#endif
  ScopedTempFile out_capture, err_capture;
  const std::string report_out_path = UniqueOutPath("orphan_watch");
  const std::vector<std::string> args = {
      "--families=slippery-onehot", "--subjects=nfa-product,dfa-product",
      "--n-min=2",                  "--n-max=3",
      "--repeat=1",                 "--timeout=3",
      "--out=" + report_out_path};

  const pid_t pid =
      SpawnBenchDirect(args, out_capture.path(), err_capture.path());
  ASSERT_GT(pid, 0) << "fork() failed";

  // Grows to the transitive descendant set of `pid` (the direct
  // RunCaseWithTimeout child, and any grandchild it shells out to) as the
  // polling loop observes them; a pid, once added, stays tracked even if a
  // later sample shows it reparented to init.
  std::vector<long long> descendants = {pid};
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(50);
  int status = 0;
  bool exited = false;
  while (std::chrono::steady_clock::now() < deadline) {
    const pid_t wr = waitpid(pid, &status, WNOHANG);
    if (wr == pid) {
      exited = true;
      break;
    }
    const std::vector<PsRow> snapshot = SampleProcessTable();
    // Transitive closure within this single snapshot: a grandchild's row
    // may appear before or after its parent's in `snapshot`, so keep
    // sweeping until a pass adds nothing new.
    for (bool added = true; added;) {
      added = false;
      for (const PsRow& row : snapshot) {
        const bool parent_known =
            std::find(descendants.begin(), descendants.end(), row.ppid) !=
            descendants.end();
        const bool already_known =
            std::find(descendants.begin(), descendants.end(), row.pid) !=
            descendants.end();
        if (parent_known && !already_known) {
          descendants.push_back(row.pid);
          added = true;
        }
      }
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }
  if (!exited) {
    // Hygiene, not part of the property under test: don't leave our own
    // spawned process behind just because this test is about to fail.
    kill(pid, SIGKILL);
    waitpid(pid, &status, 0);
  }
  ASSERT_TRUE(exited) << "the bench subprocess did not exit within this "
                        "test's 50s outer bound --- a hang-on-reap "
                        "regression would look exactly like this";
  ASSERT_TRUE(WIFEXITED(status)) << "abnormal exit, raw status=" << status;
  EXPECT_EQ(WEXITSTATUS(status), 0);

  ASSERT_GT(descendants.size(), 1u)
      << "never observed a RunCaseWithTimeout child of the spawned process "
        "--- this test isn't exercising the fork path at all, and so "
        "cannot claim to guard the kill-and-reap it targets";

  // The spawned process has exited and been reaped above; any previously
  // observed descendant pid still present and running (R/S) now is an
  // orphan the kill-and-reap failed to catch.
  const std::vector<PsRow> after = SampleProcessTable();
  for (long long descendant_pid : descendants) {
    if (descendant_pid == pid) continue;  // already confirmed reaped, above
    for (const PsRow& row : after) {
      if (row.pid != descendant_pid) continue;
      const char stat0 = row.stat.empty() ? '\0' : row.stat[0];
      EXPECT_TRUE(stat0 != 'R' && stat0 != 'S')
          << "orphaned running descendant (pid=" << row.pid
          << " comm=" << row.comm << " stat=" << row.stat
          << ") survived the parent's exit";
    }
  }
}
