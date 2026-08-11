// ltlf-ek-bench --- the Phase 2 timing runner (docs/prd/benchmark-suite.md
// "Phase 2 (cont.) -- the timing runner", B6, "Edge cases").  Sweeps the
// Phase 2A registry (include/ltlf_ek/bench_suite.hpp: bench_families() /
// bench_subjects() / run_bench_case), races ltlfsynt on T1 families, and
// writes a committed JSON report plus (optionally) a 5-sheet workbook via
// scripts/bench_xlsx_export.py.  Thin orchestration only, same idiom as
// src/ltlf_ek_synth.cpp: parse argv, drive the frozen Phase 2A registry,
// format the result.  No synthesis semantics live here.
//
// Exit codes: 0 success (sweep completed or hit its own budget and stopped
// cleanly -- both are success, per B6 "a partial workbook beats no
// workbook"); 2 usage error; 1 internal error (e.g. --out could not be
// opened for writing after the sweep already ran -- the accumulated JSON
// text is still printed to stderr as a last resort so the measurements are
// not silently lost).

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <deque>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include <sys/wait.h>
#include <unistd.h>

#include <spot/misc/version.hh>
#include <spot/tl/formula.hh>
#include <spot/tl/print.hh>

#include "ltlf_ek/bench.hpp"
#include "ltlf_ek/bench_suite.hpp"
#include "ltlf_ek/detail/util.hpp"
#include "ltlf_ek/variables.hpp"

#ifndef LTLF_EK_REPO_ROOT
#error "LTLF_EK_REPO_ROOT must be defined by CMake (see CMakeLists.txt)"
#endif
#ifndef LTLF_EK_BENCH_XLSX_SCRIPT
#error "LTLF_EK_BENCH_XLSX_SCRIPT must be defined by CMake (see CMakeLists.txt)"
#endif
#ifndef LTLF_EK_BUILD_TYPE
#define LTLF_EK_BUILD_TYPE "unknown"
#endif

namespace {

namespace fs = std::filesystem;

using ltlf_ek::BenchCase;
using ltlf_ek::BenchFamily;
using ltlf_ek::BenchParams;
using ltlf_ek::BenchRow;
using ltlf_ek::BenchSubject;
using ltlf_ek::ComparabilityTier;
using ltlf_ek::bench_families;
using ltlf_ek::bench_subjects;
using ltlf_ek::comparability_tier_name;
using ltlf_ek::run_bench_case;
using ltlf_ek::size_metric_name;
using ltlf_ek::SizeMetric;
using ltlf_ek::stage_name;
using ltlf_ek::Stage;

// A usage mistake --- unknown flag, unknown family/subject name, an --out
// that would resolve outside the repository.  Exit code 2, same convention
// as src/ltlf_ek_synth.cpp's UsageError.
class UsageError : public std::runtime_error {
 public:
  explicit UsageError(const std::string& msg) : std::runtime_error(msg) {}
};

// ---------------------------------------------------------------------------
// argv parsing (PRD "Phase 2 (cont.) -- the timing runner"):
//   ltlf-ek-bench --families=<csv|all> --subjects=<csv|all> --n-min=N
//                 --n-max=N --repeat=K --timeout=SECONDS --out=FILE
//                 [--ltlfsynt=PATH] [--xlsx=FILE] [--budget=SECONDS]
// --budget is this developer's naming of the "stated wall-clock ceiling,
// default 2h, overridable by a flag" B6 asks for without pinning a flag
// name; recorded in the PRD's Developer comments.
// ---------------------------------------------------------------------------

struct CliArgs {
  std::string families = "all";
  std::string subjects = "all";
  std::int64_t n_min = 2;
  std::int64_t n_max = 6;
  int repeat = 3;                 // PRD default
  unsigned timeout_seconds = 20;  // PRD default
  std::optional<std::string> out;
  std::optional<std::string> ltlfsynt;
  std::optional<std::string> xlsx;
  unsigned budget_seconds = 7200;  // 2h, PRD B6 default
};

struct Flag {
  std::string name;
  std::optional<std::string> value;
};

Flag SplitFlag(const std::string& arg) {
  if (arg.size() < 3 || arg[0] != '-' || arg[1] != '-')
    throw UsageError("unrecognised argument (expected --flag): " + arg);
  const std::string body = arg.substr(2);
  const std::size_t eq = body.find('=');
  if (eq == std::string::npos) return {body, std::nullopt};
  return {body.substr(0, eq), body.substr(eq + 1)};
}

std::int64_t ParseInt(const std::string& s, const std::string& flag) {
  try {
    std::size_t pos = 0;
    const long long v = std::stoll(s, &pos);
    if (pos != s.size()) throw std::invalid_argument("trailing garbage");
    return v;
  } catch (const std::exception&) {
    throw UsageError("--" + flag + " expects an integer, got '" + s + "'");
  }
}

CliArgs ParseArgs(int argc, char** argv) {
  CliArgs args;
  const std::vector<std::string> raw(argv + 1, argv + argc);
  for (std::size_t i = 0; i < raw.size(); ++i) {
    const Flag f = SplitFlag(raw[i]);
    auto need_value = [&]() -> std::string {
      if (f.value) return *f.value;
      if (i + 1 >= raw.size())
        throw UsageError("--" + f.name + " requires a value");
      return raw[++i];
    };
    if (f.name == "families") {
      args.families = need_value();
    } else if (f.name == "subjects") {
      args.subjects = need_value();
    } else if (f.name == "n-min") {
      args.n_min = ParseInt(need_value(), "n-min");
    } else if (f.name == "n-max") {
      args.n_max = ParseInt(need_value(), "n-max");
    } else if (f.name == "repeat") {
      args.repeat = static_cast<int>(ParseInt(need_value(), "repeat"));
    } else if (f.name == "timeout") {
      args.timeout_seconds =
          static_cast<unsigned>(ParseInt(need_value(), "timeout"));
    } else if (f.name == "out") {
      args.out = need_value();
    } else if (f.name == "ltlfsynt") {
      args.ltlfsynt = need_value();
    } else if (f.name == "xlsx") {
      args.xlsx = need_value();
    } else if (f.name == "budget") {
      args.budget_seconds =
          static_cast<unsigned>(ParseInt(need_value(), "budget"));
    } else {
      throw UsageError("unrecognised flag: --" + f.name);
    }
  }
  if (args.n_min > args.n_max)
    throw UsageError("--n-min must be <= --n-max");
  if (args.repeat < 1) throw UsageError("--repeat must be >= 1");
  return args;
}

std::set<std::string> SplitCsv(const std::string& csv) {
  std::set<std::string> out;
  std::istringstream ss(csv);
  std::string tok;
  while (std::getline(ss, tok, ',')) {
    const std::string trimmed = ltlf_ek::detail::trim(tok);
    if (!trimmed.empty()) out.insert(trimmed);
  }
  return out;
}

// ---------------------------------------------------------------------------
// --out / --xlsx path safety (PRD "Phase 2 (cont.)": "Never /tmp, never
// $TMPDIR: verified 2026-08-09, the sandbox mounts /tmp ... read-only, so a
// run that measures for two hours and then writes to /tmp loses everything
// at the last step").  Resolved against the compile-time repo root so the
// check does not depend on the binary's cwd.
// ---------------------------------------------------------------------------

bool PathStartsWith(const fs::path& full, const fs::path& prefix) {
  auto fit = full.begin();
  auto pit = prefix.begin();
  for (; pit != prefix.end(); ++fit, ++pit) {
    if (fit == full.end() || *fit != *pit) return false;
  }
  return true;
}

fs::path ResolveAndValidateOutPath(const std::string& raw,
                                   const std::string& what) {
  const fs::path repo_root = fs::weakly_canonical(fs::path(LTLF_EK_REPO_ROOT));
  const fs::path requested(raw);
  const fs::path abs =
      requested.is_absolute() ? requested : fs::current_path() / requested;
  const fs::path normalized = fs::weakly_canonical(abs);

  if (!PathStartsWith(normalized, repo_root))
    throw UsageError(what + " must resolve inside the repository (" +
                     repo_root.string() + "), got " + normalized.string());
  if (PathStartsWith(normalized, fs::path("/tmp")))
    throw UsageError(what +
                     " must not resolve under /tmp (mounted read-only under "
                     "the sandbox): " +
                     normalized.string());
  if (const char* tmpdir = std::getenv("TMPDIR"); tmpdir && *tmpdir) {
    const fs::path tmpdir_norm = fs::weakly_canonical(fs::path(tmpdir));
    if (PathStartsWith(normalized, tmpdir_norm))
      throw UsageError(what +
                       " must not resolve under $TMPDIR (mounted read-only "
                       "under the sandbox): " +
                       normalized.string());
  }
  return normalized;
}

// ---------------------------------------------------------------------------
// Registry selection.
// ---------------------------------------------------------------------------

std::vector<const BenchFamily*> SelectFamilies(const std::string& csv) {
  const auto& all = bench_families();
  std::vector<const BenchFamily*> out;
  if (csv == "all") {
    for (const auto& f : all) out.push_back(f.get());
    return out;
  }
  const std::set<std::string> want = SplitCsv(csv);
  std::set<std::string> found;
  for (const auto& f : all) {
    if (want.count(f->name())) {
      out.push_back(f.get());
      found.insert(f->name());
    }
  }
  for (const auto& name : want)
    if (!found.count(name))
      throw UsageError("unknown --families entry: '" + name + "'");
  return out;
}

std::vector<const BenchSubject*> SelectSubjects(const std::string& csv) {
  const auto& all = bench_subjects();
  std::vector<const BenchSubject*> out;
  if (csv == "all") {
    for (const auto& s : all) out.push_back(s.get());
    return out;
  }
  const std::set<std::string> want = SplitCsv(csv);
  std::set<std::string> found;
  for (const auto& s : all) {
    if (want.count(s->name())) {
      out.push_back(s.get());
      found.insert(s->name());
    }
  }
  for (const auto& name : want)
    if (!found.count(name))
      throw UsageError("unknown --subjects entry: '" + name + "'");
  return out;
}

// ---------------------------------------------------------------------------
// Per-case timeout enforcement (PRD "Edge cases" "Timeout (Phase 2)": a case
// exceeding --timeout records a timeout marker and must not abort the
// remaining sweep).  run_bench_case (bench_suite.hpp) is a single blocking
// call with no cancellation hook, so the only way to bound its wall time is
// to run it on its own thread and, on a deadline miss, detach rather than
// join -- the detached thread keeps running in the background (rare in
// practice; every method here terminates given enough time, it is not an
// infinite loop) and is abandoned when the process exits.  `case_ptr` /
// `subject_ptr` must outlive any detached worker: callers keep every
// instantiated BenchCase in a std::deque for the process's whole lifetime
// (deque never invalidates references on push_back) and BenchSubject
// pointers come from bench_subjects()'s process-lifetime static vector.
// ---------------------------------------------------------------------------

struct TimedRunResult {
  bool timed_out = false;
  std::vector<BenchRow> rows;
};

TimedRunResult RunCaseWithTimeout(const BenchCase* case_ptr,
                                  const BenchSubject* subject_ptr,
                                  std::chrono::seconds timeout) {
  auto rows = std::make_shared<std::vector<BenchRow>>();
  auto done = std::make_shared<std::atomic<bool>>(false);
  auto mtx = std::make_shared<std::mutex>();
  auto cv = std::make_shared<std::condition_variable>();

  std::thread worker([=]() {
    std::vector<BenchRow> local;
    try {
      local = run_bench_case(*case_ptr, *subject_ptr);
    } catch (const std::exception&) {
      // Swallow: a thrown exception must not std::terminate() the whole
      // sweep (an uncaught exception crossing a std::thread's top-level
      // function calls std::terminate).  Treated as "zero rows produced",
      // same as a cleanly-skipped subject (PRD "Edge cases" "MONA absent").
    }
    {
      std::lock_guard<std::mutex> lock(*mtx);
      *rows = std::move(local);
    }
    done->store(true);
    cv->notify_all();
  });

  {
    std::unique_lock<std::mutex> lock(*mtx);
    cv->wait_for(lock, timeout, [&] { return done->load(); });
  }

  TimedRunResult result;
  if (done->load()) {
    worker.join();
    result.rows = *rows;
  } else {
    worker.detach();
    result.timed_out = true;
  }
  return result;
}

// ---------------------------------------------------------------------------
// Canonical key classification (B2's charge table + Stage's four values):
// used to bucket a BenchRow into the "timings" sheet (a Stage span, ns) or
// the "structural" sheet (a SizeMetric, a count) without needing a type tag
// on BenchRow itself (frozen interface, PRD "Interfaces & types").
// ---------------------------------------------------------------------------

const std::set<std::string>& CanonicalStageNames() {
  static const std::set<std::string> names = {
      std::string(stage_name(Stage::automaton_construction)),
      std::string(stage_name(Stage::product_construction)),
      std::string(stage_name(Stage::game_solving)),
      std::string(stage_name(Stage::aggregation)),
  };
  return names;
}

const std::set<std::string>& CanonicalMetricNames() {
  static const std::set<std::string> names = {
      std::string(size_metric_name(SizeMetric::goal_dfa_states)),
      std::string(size_metric_name(SizeMetric::goal_nfa_states)),
      std::string(size_metric_name(SizeMetric::goal_mtdfa_roots)),
      std::string(size_metric_name(SizeMetric::nfa_product_states)),
      std::string(size_metric_name(SizeMetric::product_states)),
      std::string(size_metric_name(SizeMetric::product_mtdfa_roots)),
      std::string(size_metric_name(SizeMetric::product_bdd_nodes)),
      std::string(size_metric_name(SizeMetric::controller_states)),
  };
  return names;
}

// ---------------------------------------------------------------------------
// Repeated runs, minimum-of-K (PRD "Phase 2 (cont.)": "Reports the minimum
// of K repetitions").
// ---------------------------------------------------------------------------

struct RepeatedResult {
  std::map<std::string, std::uint64_t> min_values;  // key -> min over successes
  int successes = 0;
  int timeouts = 0;
};

RepeatedResult RunCaseRepeated(const BenchCase* case_ptr,
                               const BenchSubject* subject_ptr, int repeat,
                               std::chrono::seconds timeout) {
  RepeatedResult result;
  for (int i = 0; i < repeat; ++i) {
    const TimedRunResult tr = RunCaseWithTimeout(case_ptr, subject_ptr, timeout);
    if (tr.timed_out) {
      ++result.timeouts;
      continue;
    }
    ++result.successes;
    for (const BenchRow& row : tr.rows) {
      auto it = result.min_values.find(row.key);
      if (it == result.min_values.end() || row.value < it->second)
        result.min_values[row.key] = row.value;
    }
  }
  return result;
}

// ---------------------------------------------------------------------------
// Output row shapes (JSON + the 5-sheet workbook, B6).
// ---------------------------------------------------------------------------

struct TimingRow {
  std::string family;
  std::int64_t n;
  bool realizable;
  std::string subject;
  std::string stage;
  std::uint64_t ns;
  bool timed_out;
};

struct StructuralRow {
  std::string family;
  std::int64_t n;
  bool realizable;
  std::string subject;
  std::string metric;
  std::uint64_t value;
};

struct LtlfsyntRow {
  std::string family;
  std::int64_t n;
  bool realizable;
  std::string tier;
  std::optional<std::string> psi_in;
  std::string status;  // "ok" | "skipped" | "timeout" | "n/a -- by expressibility" | "error"
  std::optional<std::string> ek_verdict;
  std::optional<std::string> ltlfsynt_verdict;
  std::optional<std::uint64_t> ltlfsynt_ns;
  bool verdict_mismatch = false;
};

// Per (family, subject) running summary state, folded into a SummaryRow at
// the end of the sweep (PRD item 4: "per (family, method), best time at the
// largest n that completed").
struct SubjectProgress {
  bool completed_any = false;
  std::int64_t largest_n = 0;
  std::uint64_t best_time_ns = 0;      // wall_total at largest_n
  std::uint64_t construction_ns = 0;   // automaton_construction + product_construction, same n
  std::optional<std::uint64_t> product_states;
  std::optional<std::uint64_t> product_mtdfa_roots;
};

struct SummaryRow {
  std::string family;
  std::string subject;
  std::string tier;
  std::optional<std::int64_t> largest_n_completed;
  std::optional<std::uint64_t> best_time_ns;
  std::optional<std::uint64_t> construction_ns;
  std::optional<double> speedup_vs_mtdfa_product;
  std::optional<std::uint64_t> product_states;
  std::optional<std::uint64_t> product_mtdfa_roots;
};

// ---------------------------------------------------------------------------
// The ltlfsynt T1 race (PRD "The ltlfsynt T1 race", Stop-list item 4).
// Subprocess idiom follows tests/ltlfsynt_oracle_test.cpp's RunSubprocess /
// ShellQuote (duplicated here rather than shared across translation units,
// this project's precedent for CLI-facing subprocess helpers).
// ---------------------------------------------------------------------------

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

struct SubprocessResult {
  int exit_code = -1;
  std::string stdout_text;
  bool timed_out = false;
};

SubprocessResult RunSubprocessCaptured(const std::string& binary,
                                       const std::vector<std::string>& args,
                                       unsigned timeout_secs) {
  const std::string out_path =
      ltlf_ek::detail::temp_template("ltlf_ek_bench_ltlfsynt");
  char out_buf[4096];
  std::snprintf(out_buf, sizeof(out_buf), "%s", out_path.c_str());
  const int fd = mkstemp(out_buf);
  if (fd >= 0) close(fd);
  const std::string capture_path = out_buf;

  std::ostringstream cmd;
  cmd << "timeout " << timeout_secs << "s " << ShellQuote(binary);
  for (const auto& a : args) cmd << " " << ShellQuote(a);
  cmd << " >" << ShellQuote(capture_path) << " 2>/dev/null";
  const int rc = std::system(cmd.str().c_str());

  SubprocessResult result;
  result.exit_code = WIFEXITED(rc) ? WEXITSTATUS(rc) : -1;
  if ((WIFEXITED(rc) && WEXITSTATUS(rc) == 124) || WIFSIGNALED(rc))
    result.timed_out = true;
  std::ifstream in(capture_path);
  std::ostringstream ss;
  ss << in.rdbuf();
  result.stdout_text = ss.str();
  std::remove(capture_path.c_str());
  return result;
}

// Bare "ltlfsynt --version" runnability probe, same idiom as the test suite.
bool IsRunnable(const std::string& binary) {
  if (binary.empty()) return false;
  std::ostringstream cmd;
  cmd << ShellQuote(binary) << " --version >/dev/null 2>&1";
  const int rc = std::system(cmd.str().c_str());
  return WIFEXITED(rc) && WEXITSTATUS(rc) == 0;
}

// PRD "ltlfsynt invocation": --ltlfsynt=<absolute path> first; if absent,
// this developer's addition is to also try the CMake-resolved
// LTLFSYNT_BINARY default (same find_program(ltlfsynt HINTS SPOT_ROOT/bin)
// tests/ltlfsynt_oracle_test.cpp already relies on); only then the bare
// name, matching the PRD's literal "if absent and the bare name does not
// resolve, skip the race" -- resolution order is a sharpening, not a
// contract change (PRD "Developer comments").
std::string ResolveLtlfsyntBinary(const std::optional<std::string>& flag) {
  if (flag && IsRunnable(*flag)) return *flag;
#ifdef LTLFSYNT_BINARY
  if (IsRunnable(LTLFSYNT_BINARY)) return LTLFSYNT_BINARY;
#endif
  if (IsRunnable("ltlfsynt")) return "ltlfsynt";
  return "";
}

std::optional<bool> ParseLtlfsyntVerdict(const SubprocessResult& r) {
  if (r.exit_code == 0 && r.stdout_text == "REALIZABLE\n") return true;
  if (r.exit_code == 1 && r.stdout_text == "UNREALIZABLE\n") return false;
  return std::nullopt;
}

// ---------------------------------------------------------------------------
// Provenance (PRD "Phase 2 (cont.)": machine, ldd-resolved Spot version,
// repo commit, timeout, repetition count, date, plus the CMake build type
// this developer added since the runner's own binary is a build artifact
// whose optimisation level affects every timing in the report).
// ---------------------------------------------------------------------------

std::string RunCaptureStdout(const std::string& cmd) {
  std::array<char, 4096> buf{};
  std::string out;
  FILE* pipe = popen(cmd.c_str(), "r");
  if (!pipe) return "";
  while (fgets(buf.data(), static_cast<int>(buf.size()), pipe))
    out += buf.data();
  pclose(pipe);
  while (!out.empty() && (out.back() == '\n' || out.back() == '\r'))
    out.pop_back();
  return out;
}

// The actually-loaded libspot's path, resolved via ldd on this very binary
// -- NOT pkg-config's, which several shadowing Spot installs on this box
// (LD_LIBRARY_PATH) can silently disagree with (PRD "Phase 2 (cont.)").
std::string ResolveLoadedSpotPath() {
  char self_path[4096] = {0};
  const ssize_t n = readlink("/proc/self/exe", self_path, sizeof(self_path) - 1);
  if (n <= 0) return "";
  const std::string cmd = "ldd " + ShellQuote(std::string(self_path, n)) +
                          " 2>/dev/null | grep -i libspot";
  const std::string line = RunCaptureStdout(cmd);
  const std::size_t arrow = line.find("=>");
  if (arrow == std::string::npos) return line;  // best effort
  std::string path = line.substr(arrow + 2);
  const std::size_t paren = path.find('(');
  if (paren != std::string::npos) path = path.substr(0, paren);
  return ltlf_ek::detail::trim(path);
}

std::string IsoDateNow() {
  const std::time_t t = std::time(nullptr);
  char buf[32];
  std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", std::gmtime(&t));
  return buf;
}

std::string Hostname() {
  char buf[256] = {0};
  if (gethostname(buf, sizeof(buf) - 1) != 0) return "unknown";
  return buf;
}

// ---------------------------------------------------------------------------
// JSON writing --- hand-rolled, same escaping discipline as bench.cpp's
// WriteJsonString (duplicated: that helper is anonymous-namespace-local to
// bench.cpp, and this project's precedent is per-translation-unit
// duplication over a shared infra header for small CLI-only helpers).
// ---------------------------------------------------------------------------

void WriteJsonString(std::ostream& os, std::string_view s) {
  os << '"';
  for (const char c : s) {
    switch (c) {
      case '"': os << "\\\""; break;
      case '\\': os << "\\\\"; break;
      case '\n': os << "\\n"; break;
      case '\r': os << "\\r"; break;
      case '\t': os << "\\t"; break;
      default:
        if (static_cast<unsigned char>(c) < 0x20) {
          char buf[8];
          std::snprintf(buf, sizeof(buf), "\\u%04x", c);
          os << buf;
        } else {
          os << c;
        }
    }
  }
  os << '"';
}

void WriteJsonOptStr(std::ostream& os, const std::optional<std::string>& v) {
  if (!v) {
    os << "null";
  } else {
    WriteJsonString(os, *v);
  }
}

void WriteJsonOptU64(std::ostream& os, const std::optional<std::uint64_t>& v) {
  if (!v) os << "null"; else os << *v;
}

void WriteJsonOptI64(std::ostream& os, const std::optional<std::int64_t>& v) {
  if (!v) os << "null"; else os << *v;
}

void WriteJsonOptDouble(std::ostream& os, const std::optional<double>& v) {
  if (!v) os << "null"; else os << *v;
}

}  // namespace

int main(int argc, char** argv) {
  try {
    const CliArgs args = ParseArgs(argc, argv);

    const std::string default_out =
        std::string(LTLF_EK_REPO_ROOT) + "/build/benchout/report.json";
    const fs::path out_path =
        ResolveAndValidateOutPath(args.out.value_or(default_out), "--out");
    std::optional<fs::path> xlsx_path;
    if (args.xlsx) xlsx_path = ResolveAndValidateOutPath(*args.xlsx, "--xlsx");

    const std::vector<const BenchFamily*> families = SelectFamilies(args.families);
    const std::vector<const BenchSubject*> subjects = SelectSubjects(args.subjects);
    if (families.empty()) throw UsageError("--families selected zero families");
    if (subjects.empty()) throw UsageError("--subjects selected zero subjects");

    const std::string ltlfsynt_binary = ResolveLtlfsyntBinary(args.ltlfsynt);

    // ---- provenance ----
    const std::string spot_version = spot::version();  // a real dynamic-link
        // call: resolved at runtime through whichever libspot.so the loader
        // actually picked, so this is already the "actually loaded" version,
        // not the headers this binary was compiled against.
    const std::string spot_resolved_path = ResolveLoadedSpotPath();
    const std::string repo_commit =
        RunCaptureStdout("git -C " + ShellQuote(LTLF_EK_REPO_ROOT) +
                         " rev-parse HEAD 2>/dev/null");
    const std::string date = IsoDateNow();
    const std::string hostname = Hostname();

    // ---- the sweep ----
    const auto sweep_start = std::chrono::steady_clock::now();
    const auto deadline =
        sweep_start + std::chrono::seconds(args.budget_seconds);
    bool stopped_early = false;

    std::deque<BenchCase> case_store;  // never erased: stable references for
                                       // the whole process lifetime (see
                                       // RunCaseWithTimeout's doc comment).

    std::vector<TimingRow> timing_rows;
    std::vector<StructuralRow> structural_rows;
    std::vector<LtlfsyntRow> ltlfsynt_rows;
    std::map<std::pair<std::string, std::string>, SubjectProgress> progress;  // (family, subject)

    auto stage_sum = [](const std::map<std::string, std::uint64_t>& mv,
                        std::initializer_list<Stage> stages) -> std::uint64_t {
      std::uint64_t total = 0;
      for (Stage s : stages) {
        const auto it = mv.find(std::string(stage_name(s)));
        if (it != mv.end()) total += it->second;
      }
      return total;
    };

    // Ordered (n, realizable) x family x subject: n ascending outer, so a
    // budget-exceeded stop still leaves broad low-n coverage across every
    // family/subject rather than exhausting the budget on one family (PRD
    // B6 "Sweep budget": "a partial workbook beats no workbook").
    for (std::int64_t n = args.n_min; n <= args.n_max && !stopped_early; ++n) {
      for (int realizable_i = 0; realizable_i <= 1 && !stopped_early; ++realizable_i) {
        const bool realizable = realizable_i != 0;
        const BenchParams params{{"n", n}, {"realizable", realizable_i}};

        for (const BenchFamily* family : families) {
          if (std::chrono::steady_clock::now() >= deadline) {
            stopped_early = true;
            break;
          }

          case_store.push_back(family->instantiate(params));
          const BenchCase* c = &case_store.back();

          // --- the five-method subjects (or whichever --subjects picked) ---
          for (const BenchSubject* subject : subjects) {
            if (std::chrono::steady_clock::now() >= deadline) {
              stopped_early = true;
              break;
            }
            const RepeatedResult rr = RunCaseRepeated(
                c, subject, args.repeat, std::chrono::seconds(args.timeout_seconds));

            if (rr.successes == 0) {
              // Every repeat timed out: a timeout marker (PRD "Edge cases"
              // "Timeout (Phase 2)"), never a silent absence and never an
              // error that aborts the sweep.
              if (rr.timeouts > 0) {
                timing_rows.push_back(TimingRow{
                    family->name(), n, realizable, subject->name(), "TIMEOUT",
                    static_cast<std::uint64_t>(args.timeout_seconds) * 1'000'000'000ull,
                    true});
              }
              // rr.successes == 0 and rr.timeouts == 0 means the subject
              // cleanly skipped every repeat (MONA absent, PRD "Edge
              // cases") -- absence, not a row (B2 rule 1).
              continue;
            }

            // Bucket the min-of-K values into timing (Stage) vs structural
            // (SizeMetric) rows.
            std::uint64_t wall_total = 0;
            for (const auto& [key, value] : rr.min_values) {
              if (CanonicalMetricNames().count(key)) {
                structural_rows.push_back(
                    StructuralRow{family->name(), n, realizable, subject->name(),
                                 key, value});
              } else {
                timing_rows.push_back(TimingRow{family->name(), n, realizable,
                                                subject->name(), key, value,
                                                false});
                if (CanonicalStageNames().count(key)) wall_total += value;
              }
            }
            if (!rr.min_values.empty()) {
              timing_rows.push_back(TimingRow{family->name(), n, realizable,
                                              subject->name(), "wall_total",
                                              wall_total, false});
            }
            if (rr.timeouts > 0 && rr.successes > 0) {
              // Partial timeout: some repeats succeeded (used above for the
              // min), some did not -- flagged, not hidden, without
              // discarding the successful measurements (PRD "Edge cases":
              // "Structural rows already gathered for that case stay
              // valid").
              timing_rows.push_back(TimingRow{
                  family->name(), n, realizable, subject->name(),
                  "PARTIAL_TIMEOUT_" + std::to_string(rr.timeouts) + "_of_" +
                      std::to_string(args.repeat),
                  0, true});
            }

            // --- cross-method summary bookkeeping (only the historical
            // headline polarity, realizable=1; see this PRD's Developer
            // comments for the rationale) ---
            if (realizable && !rr.min_values.empty()) {
              SubjectProgress& p = progress[{family->name(), subject->name()}];
              if (n >= p.largest_n) {
                p.completed_any = true;
                p.largest_n = n;
                p.best_time_ns = wall_total;
                // OtfMtdfaProduct has no automaton_construction span (PRD
                // "Edge cases"): summed with 0 contribution rather than
                // compared bare, or the comparison flatters MtdfaProduct by
                // the spot::ltlf_to_mtdfa cost under test.
                p.construction_ns = stage_sum(
                    rr.min_values,
                    {Stage::automaton_construction, Stage::product_construction});
                const auto ps = rr.min_values.find(
                    std::string(size_metric_name(SizeMetric::product_states)));
                p.product_states = ps != rr.min_values.end()
                                       ? std::optional<std::uint64_t>(ps->second)
                                       : std::nullopt;
                const auto pmr = rr.min_values.find(std::string(
                    size_metric_name(SizeMetric::product_mtdfa_roots)));
                p.product_mtdfa_roots =
                    pmr != rr.min_values.end()
                        ? std::optional<std::uint64_t>(pmr->second)
                        : std::nullopt;
              }
            }
          }
          if (stopped_early) break;

          // --- the ltlfsynt T1 race (PRD "The ltlfsynt T1 race") ---
          if (c->tier != ComparabilityTier::t1) {
            ltlfsynt_rows.push_back(LtlfsyntRow{
                family->name(), n, realizable,
                std::string(comparability_tier_name(c->tier)), c->psi_in,
                "n/a -- by expressibility", std::nullopt, std::nullopt,
                std::nullopt, false});
            continue;
          }
          LtlfsyntRow row{family->name(),      n,  realizable,
                         std::string(comparability_tier_name(c->tier)),
                         c->psi_in,           "",  std::nullopt,
                         std::nullopt,         std::nullopt, false};
          row.ek_verdict = c->expected_realizable ? "REALIZABLE" : "UNREALIZABLE";
          if (ltlfsynt_binary.empty()) {
            row.status = "skipped";
            ltlfsynt_rows.push_back(row);
            continue;
          }
          const std::string phi_str = spot::str_psl(c->phi);
          const std::string reduced =
              "(" + c->psi_in.value_or("1") + ") -> (" + phi_str + ")";
          std::string ins, outs;
          for (const auto& ap : c->vars.inputs()) ins += (ins.empty() ? "" : ",") + ap;
          for (const auto& ap : c->vars.outputs()) outs += (outs.empty() ? "" : ",") + ap;

          std::optional<std::uint64_t> best_ns;
          std::optional<bool> synt_realizable;
          bool any_success = false, any_timeout = false;
          for (int rep = 0; rep < args.repeat; ++rep) {
            const auto t0 = std::chrono::steady_clock::now();
            const SubprocessResult sr = RunSubprocessCaptured(
                ltlfsynt_binary,
                {"--ins=" + ins, "--outs=" + outs, "--semantics=Mealy",
                 "--realizability", "-f", reduced},
                args.timeout_seconds);
            const auto t1 = std::chrono::steady_clock::now();
            if (sr.timed_out) {
              any_timeout = true;
              continue;
            }
            any_success = true;
            const std::uint64_t ns = static_cast<std::uint64_t>(
                std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0)
                    .count());
            if (!best_ns || ns < *best_ns) best_ns = ns;
            synt_realizable = ParseLtlfsyntVerdict(sr);
          }
          if (!any_success) {
            row.status = any_timeout ? "timeout" : "error";
          } else if (!synt_realizable) {
            row.status = "error";  // ltlfsynt ran but printed no verdict word
          } else {
            row.status = "ok";
            row.ltlfsynt_ns = best_ns;
            row.ltlfsynt_verdict = *synt_realizable ? "REALIZABLE" : "UNREALIZABLE";
            // Stop-list item 4: a verdict disagreement is an O5-class
            // theory finding, recorded prominently, never "fixed" here.
            if (*synt_realizable != c->expected_realizable) {
              row.verdict_mismatch = true;
              std::cerr << "*** ltlfsynt verdict disagreement (Stop-list "
                          "item 4, O5-class): family="
                       << family->name() << " n=" << n
                       << " realizable=" << realizable
                       << " ek=" << *row.ek_verdict
                       << " ltlfsynt=" << *row.ltlfsynt_verdict << " ***\n";
            }
          }
          ltlfsynt_rows.push_back(row);
        }
      }
    }

    const auto sweep_end = std::chrono::steady_clock::now();
    const double sweep_wall_seconds =
        std::chrono::duration<double>(sweep_end - sweep_start).count();

    // ---- the cross-method summary table (PRD item 4) ----
    std::vector<SummaryRow> summary_rows;
    for (const BenchFamily* family : families) {
      for (const BenchSubject* subject : subjects) {
        const auto it = progress.find({family->name(), subject->name()});
        SummaryRow row;
        row.family = family->name();
        row.subject = subject->name();
        row.tier = "";  // filled below once a case for this family is known
        if (it == progress.end() || !it->second.completed_any) {
          summary_rows.push_back(row);
          continue;
        }
        const SubjectProgress& p = it->second;
        row.largest_n_completed = p.largest_n;
        row.best_time_ns = p.best_time_ns;
        row.construction_ns = p.construction_ns;
        row.product_states = p.product_states;
        row.product_mtdfa_roots = p.product_mtdfa_roots;
        const auto mtdfa_it = progress.find({family->name(), "mtdfa-product"});
        if (mtdfa_it != progress.end() && mtdfa_it->second.completed_any &&
            mtdfa_it->second.largest_n == p.largest_n &&
            p.best_time_ns > 0) {
          row.speedup_vs_mtdfa_product =
              static_cast<double>(mtdfa_it->second.best_time_ns) /
              static_cast<double>(p.best_time_ns);
        }
        summary_rows.push_back(row);
      }
    }
    // Fill each summary row's declared tier from the family (a family
    // declares one tier for every n/realizable it instantiates, B3).
    {
      std::map<std::string, std::string> tier_by_family;
      for (const auto& lr : ltlfsynt_rows) tier_by_family[lr.family] = lr.tier;
      for (auto& row : summary_rows) {
        const auto it = tier_by_family.find(row.family);
        if (it != tier_by_family.end()) row.tier = it->second;
      }
    }

    // ---- write the JSON report (always, even on a budget stop: "writes
    // what it has") ----
    fs::create_directories(out_path.parent_path());
    std::ostringstream json;
    json << "{\"provenance\":{";
    json << "\"machine\":"; WriteJsonString(json, hostname);
    json << ",\"spot_version\":"; WriteJsonString(json, spot_version);
    json << ",\"spot_resolved_path\":"; WriteJsonString(json, spot_resolved_path);
    json << ",\"repo_commit\":"; WriteJsonString(json, repo_commit);
    json << ",\"cmake_build_type\":"; WriteJsonString(json, LTLF_EK_BUILD_TYPE);
    json << ",\"date\":"; WriteJsonString(json, date);
    json << ",\"timeout_seconds\":" << args.timeout_seconds;
    json << ",\"repeat\":" << args.repeat;
    json << ",\"budget_seconds\":" << args.budget_seconds;
    json << ",\"n_min\":" << args.n_min << ",\"n_max\":" << args.n_max;
    json << ",\"sweep_wall_seconds\":" << sweep_wall_seconds;
    json << ",\"stopped_early\":" << (stopped_early ? "true" : "false");
    json << ",\"families\":[";
    for (std::size_t i = 0; i < families.size(); ++i) {
      if (i) json << ",";
      WriteJsonString(json, families[i]->name());
    }
    json << "],\"subjects\":[";
    for (std::size_t i = 0; i < subjects.size(); ++i) {
      if (i) json << ",";
      WriteJsonString(json, subjects[i]->name());
    }
    json << "],\"ltlfsynt_binary\":"; WriteJsonString(json, ltlfsynt_binary);
    int mismatch_count = 0;
    for (const auto& lr : ltlfsynt_rows) if (lr.verdict_mismatch) ++mismatch_count;
    json << ",\"verdict_mismatch_count\":" << mismatch_count;
    json << "}";

    json << ",\"timings\":[";
    for (std::size_t i = 0; i < timing_rows.size(); ++i) {
      const TimingRow& r = timing_rows[i];
      if (i) json << ",";
      json << "{\"family\":"; WriteJsonString(json, r.family);
      json << ",\"n\":" << r.n << ",\"realizable\":" << (r.realizable ? "true" : "false");
      json << ",\"subject\":"; WriteJsonString(json, r.subject);
      json << ",\"stage\":"; WriteJsonString(json, r.stage);
      json << ",\"ns\":" << r.ns;
      json << ",\"timed_out\":" << (r.timed_out ? "true" : "false") << "}";
    }
    json << "]";

    json << ",\"structural\":[";
    for (std::size_t i = 0; i < structural_rows.size(); ++i) {
      const StructuralRow& r = structural_rows[i];
      if (i) json << ",";
      json << "{\"family\":"; WriteJsonString(json, r.family);
      json << ",\"n\":" << r.n << ",\"realizable\":" << (r.realizable ? "true" : "false");
      json << ",\"subject\":"; WriteJsonString(json, r.subject);
      json << ",\"metric\":"; WriteJsonString(json, r.metric);
      json << ",\"value\":" << r.value << "}";
    }
    json << "]";

    json << ",\"ltlfsynt\":[";
    for (std::size_t i = 0; i < ltlfsynt_rows.size(); ++i) {
      const LtlfsyntRow& r = ltlfsynt_rows[i];
      if (i) json << ",";
      json << "{\"family\":"; WriteJsonString(json, r.family);
      json << ",\"n\":" << r.n << ",\"realizable\":" << (r.realizable ? "true" : "false");
      json << ",\"tier\":"; WriteJsonString(json, r.tier);
      json << ",\"psi_in\":"; WriteJsonOptStr(json, r.psi_in);
      json << ",\"status\":"; WriteJsonString(json, r.status);
      json << ",\"ek_verdict\":"; WriteJsonOptStr(json, r.ek_verdict);
      json << ",\"ltlfsynt_verdict\":"; WriteJsonOptStr(json, r.ltlfsynt_verdict);
      json << ",\"ltlfsynt_ns\":"; WriteJsonOptU64(json, r.ltlfsynt_ns);
      json << ",\"verdict_mismatch\":" << (r.verdict_mismatch ? "true" : "false") << "}";
    }
    json << "]";

    json << ",\"summary\":[";
    for (std::size_t i = 0; i < summary_rows.size(); ++i) {
      const SummaryRow& r = summary_rows[i];
      if (i) json << ",";
      json << "{\"family\":"; WriteJsonString(json, r.family);
      json << ",\"subject\":"; WriteJsonString(json, r.subject);
      json << ",\"tier\":"; WriteJsonString(json, r.tier);
      json << ",\"largest_n_completed\":"; WriteJsonOptI64(json, r.largest_n_completed);
      json << ",\"best_time_ns\":"; WriteJsonOptU64(json, r.best_time_ns);
      json << ",\"construction_ns\":"; WriteJsonOptU64(json, r.construction_ns);
      json << ",\"speedup_vs_mtdfa_product\":";
      WriteJsonOptDouble(json, r.speedup_vs_mtdfa_product);
      json << ",\"product_states\":"; WriteJsonOptU64(json, r.product_states);
      json << ",\"product_mtdfa_roots\":"; WriteJsonOptU64(json, r.product_mtdfa_roots);
      json << "}";
    }
    json << "]}";

    {
      std::ofstream out(out_path);
      if (!out) {
        std::cerr << "internal error: could not open --out for writing: "
                 << out_path << "\n";
        std::cerr << "--- accumulated report (last resort, not lost) ---\n"
                 << json.str() << "\n";
        return 1;
      }
      out << json.str();
    }
    std::cout << "ltlf-ek-bench: wrote " << out_path
             << (stopped_early ? " (sweep budget exceeded -- partial)" : "")
             << "\n";

    if (xlsx_path) {
      const std::string cmd = "python3 " + ShellQuote(LTLF_EK_BENCH_XLSX_SCRIPT) +
                              " " + ShellQuote(out_path.string()) + " " +
                              ShellQuote(xlsx_path->string());
      const int rc = std::system(cmd.c_str());
      const int code = WIFEXITED(rc) ? WEXITSTATUS(rc) : -1;
      if (code == 0) {
        std::cout << "ltlf-ek-bench: wrote " << *xlsx_path << "\n";
      } else if (code == 2) {
        std::cerr << "ltlf-ek-bench: WARNING -- openpyxl unavailable; "
                    "fell back to CSV next to "
                 << *xlsx_path
                 << " (the sweep itself is safe, see --out) \n";
      } else {
        std::cerr << "ltlf-ek-bench: WARNING -- xlsx export failed (exit "
                 << code << "); the sweep JSON at " << out_path
                 << " is unaffected\n";
      }
    }

    return 0;
  } catch (const UsageError& e) {
    std::cerr << "usage error: " << e.what() << "\n";
    return 2;
  } catch (const std::exception& e) {
    std::cerr << "internal error: " << e.what() << "\n";
    return 1;
  }
}
