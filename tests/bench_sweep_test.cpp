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

// slippery-onehot's enumerated psi_in passes 128 KiB (MAX_ARG_STRLEN) at N=5,
// so a formula handed over as one argv string fails the exec.
TEST(BenchSweep, LtlfsyntRacesAPsiInLargerThanOneArgvString) {
  if (std::system("command -v ltlfsynt >/dev/null 2>&1") != 0)
    GTEST_SKIP() << "no ltlfsynt on PATH";
  const std::string json = RunSweep(
      "bench_sweep_big_psi",
      "--families=slippery-onehot --subjects=dfa-product --n-min=5 --n-max=5 "
      "--repeat=1 --timeout=5");
  const std::size_t at = json.find("\"ltlfsynt\":[");
  ASSERT_NE(at, std::string::npos);
  const std::regex row("\"status\":\"(ok|timeout|partial_timeout)\"");
  std::size_t seen = 0;
  const std::string tail = json.substr(at);
  for (auto it = std::sregex_iterator(tail.begin(), tail.end(), row);
       it != std::sregex_iterator(); ++it)
    ++seen;
  EXPECT_EQ(seen, 2u) << "both polarities raced, neither errored";
}

// A failing ltlfsynt leaves status "error" with the reason beside it.
TEST(BenchSweep, LtlfsyntFailureRecordsItsReason) {
  const std::string stub =
      std::string(LTLF_EK_REPO_ROOT) + "/build/benchout/failing_ltlfsynt.sh";
  {
    std::ofstream f(stub);
    f << "#!/bin/sh\n[ \"$1\" = --version ] && exit 0\n"
         "echo 'stub: cannot parse formula' >&2\nexit 2\n";
  }
  ASSERT_EQ(std::system(("chmod +x '" + stub + "'").c_str()), 0);
  const std::string json = RunSweep(
      "bench_sweep_failure_reason",
      "--families=cons-prunes --subjects=dfa-product --n-min=2 --n-max=2 "
      "--repeat=1 --timeout=20 --ltlfsynt=" + stub);
  std::remove(stub.c_str());
  const std::regex row("\"status\":\"error\"[^}]*\"error\":"
                       "\"stub: cannot parse formula\"");
  EXPECT_TRUE(std::regex_search(json, row));
}

}  // namespace
