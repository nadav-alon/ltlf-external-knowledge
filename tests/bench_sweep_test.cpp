#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

#include <sys/wait.h>

#include <gtest/gtest.h>

#ifndef LTLF_EK_BENCH_BINARY
#error "LTLF_EK_BENCH_BINARY must be defined by CMake (see CMakeLists.txt)"
#endif
#ifndef LTLF_EK_REPO_ROOT
#error "LTLF_EK_REPO_ROOT must be defined by CMake (see CMakeLists.txt)"
#endif

// The sweep columns of ltlf-ek-bench's JSON: the median next to the min of the
// repeats, the ltlfsynt column, the prune baseline.
// Subprocess-level like tests/bench_runner_isolation_test.cpp; the report is
// read back with regexes over the emitter's fixed key order rather than a
// JSON parser.
namespace {

std::string RunSweep(const std::string& tag, const std::string& flags) {
  const std::string out = std::string(LTLF_EK_REPO_ROOT) + "/build/benchout/" +
                          tag + ".json";
  const std::string cmd = std::string("timeout 120s ") + LTLF_EK_BENCH_BINARY +
                          " " + flags + " --out=" + out + " >/dev/null 2>&1";
  const int rc = std::system(cmd.c_str());
  EXPECT_TRUE(WIFEXITED(rc) && WEXITSTATUS(rc) == 0) << cmd;
  std::ifstream in(out);
  std::ostringstream ss;
  ss << in.rdbuf();
  std::remove(out.c_str());
  return ss.str();
}

TEST(BenchSweep, WallTotalRowCarriesAMedianNotBelowItsMin) {
  const std::string json = RunSweep(
      "bench_sweep_median",
      "--families=cons-prunes --subjects=dfa-product --n-min=2 --n-max=2 "
      "--repeat=3 --timeout=20 --ltlfsynt=/nonexistent");
  const std::regex row(
      "\\{\"family\":\"cons-prunes\",\"n\":2,[^}]*\"stage\":\"wall_total\","
      "\"ns\":([0-9]+),\"median_ns\":([0-9]+),");
  std::size_t seen = 0;
  for (auto it = std::sregex_iterator(json.begin(), json.end(), row);
       it != std::sregex_iterator(); ++it, ++seen) {
    // wall_total's min is a sum of per-stage minima, so it can sit below
    // every single run's total, and so below the median of the totals.
    EXPECT_LE(std::stoull((*it)[1]), std::stoull((*it)[2]));
  }
  EXPECT_EQ(seen, 2u) << "one wall_total row per polarity (realizable 0/1)";
}

TEST(BenchSweep, NMinBelowAFamilysFloorStartsThatFamilyAtItsFloor) {
  const std::string json = RunSweep(
      "bench_sweep_floor",
      "--families=cons-prunes,hidden-counter --subjects=dfa-product "
      "--n-min=1 --n-max=2 --repeat=1 --timeout=20 --ltlfsynt=/nonexistent");
  EXPECT_NE(json.find("\"family\":\"hidden-counter\",\"n\":1,"),
            std::string::npos);
  EXPECT_NE(json.find("\"family\":\"cons-prunes\",\"n\":2,"),
            std::string::npos);
  EXPECT_EQ(json.find("\"family\":\"cons-prunes\",\"n\":1,"),
            std::string::npos);
}

TEST(BenchSweep, LtlfsyntRowCarriesAMedianNotBelowItsMin) {
  if (std::system("command -v ltlfsynt >/dev/null 2>&1") != 0)
    GTEST_SKIP() << "no ltlfsynt on PATH";
  const std::string json = RunSweep(
      "bench_sweep_ltlfsynt",
      "--families=cons-prunes --subjects=dfa-product --n-min=2 --n-max=2 "
      "--repeat=3 --timeout=20");
  const std::regex row(
      "\\{\"family\":\"cons-prunes\",\"n\":2,[^}]*\"status\":\"ok\"[^}]*"
      "\"ltlfsynt_ns\":([0-9]+),\"ltlfsynt_median_ns\":([0-9]+),");
  std::size_t seen = 0;
  for (auto it = std::sregex_iterator(json.begin(), json.end(), row);
       it != std::sregex_iterator(); ++it, ++seen) {
    EXPECT_LE(std::stoull((*it)[1]), std::stoull((*it)[2]));
  }
  EXPECT_EQ(seen, 2u) << "one raced row per polarity (realizable 0/1)";
}

// slippery-onehot's enumerated psi_in passes 128 KiB (MAX_ARG_STRLEN) from N=5,
// so a formula handed over as one argv string fails the exec.
TEST(BenchSweep, LtlfsyntRacesAPsiInLargerThanOneArgvString) {
  if (std::system("command -v ltlfsynt >/dev/null 2>&1") != 0)
    GTEST_SKIP() << "no ltlfsynt on PATH";
  const std::string json = RunSweep(
      "bench_sweep_big_psi",
      "--families=slippery-onehot --subjects=dfa-product --n-min=5 --n-max=6 "
      "--repeat=1 --timeout=5");
  const std::size_t at = json.find("\"ltlfsynt\":[");
  ASSERT_NE(at, std::string::npos);
  const std::regex row("\"status\":\"(ok|timeout|partial_timeout)\"");
  std::size_t seen = 0;
  const std::string tail = json.substr(at);
  for (auto it = std::sregex_iterator(tail.begin(), tail.end(), row);
       it != std::sregex_iterator(); ++it)
    ++seen;
  EXPECT_EQ(seen, 4u) << "N=5 and N=6, both polarities raced, neither errored";
}

// Writes a stub ltlfsynt that answers --version and otherwise runs `body`.
std::string WriteStubLtlfsynt(const std::string& name, const std::string& body) {
  const std::string stub =
      std::string(LTLF_EK_REPO_ROOT) + "/build/benchout/" + name;
  std::ofstream f(stub);
  f << "#!/bin/sh\n[ \"$1\" = --version ] && exit 0\n" << body;
  f.close();
  EXPECT_EQ(std::system(("chmod +x '" + stub + "'").c_str()), 0);
  return stub;
}

// The ltlfsynt rows of a sweep, as JSON text, for one stub's body.
std::string LtlfsyntRowsWith(const std::string& tag, const std::string& body,
                             const std::string& timeout = "20") {
  const std::string stub = WriteStubLtlfsynt(tag + ".sh", body);
  const std::string json = RunSweep(
      "bench_sweep_" + tag,
      "--families=cons-prunes --subjects=dfa-product --n-min=2 --n-max=2 "
      "--repeat=1 --timeout=" + timeout + " --ltlfsynt=" + stub);
  std::remove(stub.c_str());
  const std::size_t at = json.find("\"ltlfsynt\":[");
  return at == std::string::npos ? "" : json.substr(at);
}

// A failing ltlfsynt leaves status "error" with the reason beside it.
TEST(BenchSweep, LtlfsyntFailureRecordsItsReason) {
  const std::string rows = LtlfsyntRowsWith(
      "failing_ltlfsynt", "echo 'stub: cannot parse formula' >&2\nexit 2\n");
  const std::regex row("\"status\":\"error\"[^}]*\"error\":"
                       "\"stub: cannot parse formula\"");
  EXPECT_TRUE(std::regex_search(rows, row));
}

// The --verbose lines ltlfsynt prints before failing are not its reason.
TEST(BenchSweep, LtlfsyntFailureReasonSkipsVerboseLines) {
  const std::string rows = LtlfsyntRowsWith(
      "failing_verbose_ltlfsynt",
      "echo 'using Mealy semantics' >&2\n"
      "echo 'formula simplified to a' >&2\n"
      "echo 'translation to MTDFA (1 roots, 0 APs) took 0.1 seconds' >&2\n"
      "echo 'stub: out of memory' >&2\nexit 2\n");
  const std::regex row("\"status\":\"error\"[^}]*\"error\":\"stub: out of memory\"");
  EXPECT_TRUE(std::regex_search(rows, row));
}

// Full synthesis adds "translation produced a strategy already" and
// "Mealy machine (N states) created in S seconds" to the --verbose lines.
TEST(BenchSweep, LtlfsyntFailureReasonSkipsSynthesisVerboseLines) {
  const std::string fixtures =
      std::string(LTLF_EK_REPO_ROOT) + "/tests/fixtures/ltlfsynt/";
  const std::string rows = LtlfsyntRowsWith(
      "failing_synthesis_ltlfsynt",
      "cat '" + fixtures + "synthesis-realizable-2.15.1.stderr.txt' >&2\n"
      "echo 'stub: out of memory' >&2\nexit 2\n");
  const std::regex row("\"status\":\"error\"[^}]*\"error\":\"stub: out of memory\"");
  EXPECT_TRUE(std::regex_search(rows, row));
}

TEST(BenchSweep, LtlfsyntRowRecordsInternalTimeFromVerbose) {
  const std::string rows = LtlfsyntRowsWith(
      "verbose_ok",
      "echo 'translation to MTDFA (5 roots, 0 APs) took 0.25 seconds' >&2\n"
      "echo 'simplification took 0.5 seconds' >&2\necho REALIZABLE\nexit 0\n");
  EXPECT_NE(rows.find("\"ltlfsynt_internal_ns\":750000000,"),
            std::string::npos);
}

TEST(BenchSweep, LtlfsyntRowWithoutStageLinesHasNullInternalTimeNotZero) {
  const std::string rows =
      LtlfsyntRowsWith("verbose_absent", "echo REALIZABLE\nexit 0\n");
  EXPECT_NE(rows.find("\"status\":\"ok\""), std::string::npos);
  EXPECT_NE(rows.find("\"ltlfsynt_internal_ns\":null,"),
            std::string::npos);
}

TEST(BenchSweep, LtlfsyntTimeoutHasNullInternalTimeNotZero) {
  const std::string rows = LtlfsyntRowsWith(
      "verbose_timeout",
      "echo 'translation to MTDFA (5 roots, 0 APs) took 0.25 seconds' >&2\n"
      "sleep 30\n",
      "1");
  EXPECT_NE(rows.find("\"status\":\"timeout\""), std::string::npos);
  EXPECT_NE(rows.find("\"ltlfsynt_internal_ns\":null,"),
            std::string::npos);
  EXPECT_EQ(rows.find("\"ltlfsynt_internal_ns\":0"), std::string::npos);
}

// Provenance names the CPU model beside the hostname, as lscpu reports it.
TEST(BenchSweep, ProvenanceRecordsTheCpuModelBesideTheMachine) {
  const std::string json = RunSweep(
      "bench_sweep_cpu_model",
      "--families=cons-prunes --subjects=dfa-product --n-min=2 --n-max=2 "
      "--repeat=1 --timeout=20 --ltlfsynt=/nonexistent");
  const std::regex field("\"cpu_model\":\"([^\"]+)\"");
  std::smatch m;
  ASSERT_TRUE(std::regex_search(json, m, field));
  const std::string got = m[1].str();
  EXPECT_EQ(got.front() == ' ' || got.back() == ' ', false) << "unstripped: '" << got << "'";
  EXPECT_EQ(got.find("Model name"), std::string::npos) << "label leaked into the value: " << got;
  // Wherever the kernel reports a model name, the bench must have found one.
  std::ifstream cpuinfo("/proc/cpuinfo");
  std::string line;
  while (std::getline(cpuinfo, line)) {
    if (line.rfind("model name", 0) != 0) continue;
    EXPECT_NE(got, "unknown");
    EXPECT_NE(line.find(got), std::string::npos) << got << " vs " << line;
    break;
  }
}

}  // namespace
