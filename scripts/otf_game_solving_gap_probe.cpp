// Probe for the game_solving gap between OtfMtdfaProduct and MtdfaProduct at an
// identical state count (docs/runs/2026-10-17-otf-game-solving-gap.md).  Works on the cons-inert
// family (family B of docs/prd/otf-mtdfa-product.md), realizable.
//
// Deliberately NOT a CMake target (precedent: scripts/slippery_world_cert.cpp).
// Compile out of tree, against a Release build/libltlf_ek.a:
//
//   c++ -std=c++20 -O2 -I include -isystem /usr/local/include scripts/otf_game_solving_gap_probe.cpp build/libltlf_ek.a -lspot -lbddx -o build/otf-gap-probe
//
//   otf-gap-probe N [REPEAT] MODE
//
// MODE, by the question it answers:
//   both | spot | otf   Structure: roots, BDD nodes, sharing, variable-order
//                       violations of each product mtdfa, plus the solve
//                       stages.  (Pre-solve printing perturbs the heap; see
//                       docs/runs/2026-10-17-otf-game-solving-gap.md.)
//   spot-then-otf, otf-then-spot
//                       Both paths in one process, the order a sweep runs them.
//   synth-spot, synth-otf
//                       The real ::synthesize under a BenchScope: the span
//                       the benchmark reports.
//   steps-spot, steps-otf
//                       synthesize()'s steps by hand with nothing printed
//                       before the timed solve, so the heap is left exactly as
//                       the build left it.
// Environment: MALLINFO=1 prints the heap shape just before the solve;
// CONSOLIDATE=1 frees one 100000-byte block first, TRIM=1 runs malloc_trim(0)
// first; both consolidate the free lists.
#include <algorithm>
#include <chrono>
#include <cmath>
#include <malloc.h>
#include <sys/resource.h>

#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <set>
#include <unordered_set>

#include <bddx.h>
#include <spot/twaalgos/ltlf2dfa.hh>
#include <spot/twaalgos/synthesis.hh>

#include "ltlf_ek/bench.hpp"
#include "ltlf_ek/bench_suite.hpp"
#include "ltlf_ek/emits_dfa.hpp"
#include "ltlf_ek/mtdfa_product.hpp"
#include "ltlf_ek/otf_mtdfa_product.hpp"
#include "ltlf_ek/product.hpp"
#include "ltlf_ek/solve_mtdfa.hpp"

using namespace ltlf_ek;
using Clock = std::chrono::steady_clock;

namespace {

long minflt() {
  rusage ru;
  getrusage(RUSAGE_SELF, &ru);
  return ru.ru_minflt;
}

double ms_since(Clock::time_point t0) {
  return std::chrono::duration<double, std::milli>(Clock::now() - t0).count();
}

// Distinct internal nodes reachable from all roots (shared nodes counted once),
// and the sum over roots of each root's own tree-of-DAG size (sharing lost).
void walk(const bdd& b, std::unordered_set<int>& seen) {
  if (b == bddtrue || b == bddfalse || bdd_is_terminal(b)) return;
  if (!seen.insert(b.id()).second) return;
  walk(bdd_high(b), seen);
  walk(bdd_low(b), seen);
}

// CONSOLIDATE=1 frees one 100000-byte block, TRIM=1 runs malloc_trim(0) first; MALLINFO=1 prints the heap shape.
void heap_note() {
  if (std::getenv("CONSOLIDATE")) {
    // Freeing a chunk >= 64 KiB (FASTBIN_CONSOLIDATION_THRESHOLD) makes glibc
    // coalesce every fastbin/tcache chunk, without malloc_trim's munmap work.
    const auto t0 = Clock::now();
    void* volatile block = std::malloc(100000);  // volatile: the compiler elides a bare malloc/free pair
    std::free(block);
    std::fprintf(stderr, "free(malloc(100000)): %.2f ms\n", ms_since(t0));
  }
  if (std::getenv("TRIM")) {
    const auto t0 = Clock::now();
    malloc_trim(0);  // consolidates fastbin/tcache chunks
    std::fprintf(stderr, "malloc_trim(0): %.2f ms\n", ms_since(t0));
  }
  if (std::getenv("MALLINFO")) {
    const struct mallinfo2 mi = mallinfo2();
    std::fprintf(stderr, "heap before solve: arena=%zu free chunks=%zu free bytes=%zu top=%zu mmapped=%zu\n",
                 mi.arena, mi.ordblks, mi.fordblks, mi.keepcost, mi.hblkhd);
  }
}

// Memory-layout signature: mean |id(root i+1) - id(root i)| over the roots in
// state order, and the same for each root's low child.  BuDDy ids are table
// indices, so a small stride means the rows the solver visits consecutively
// sit next to each other in memory.
void layout(const char* tag, const spot::mtdfa_ptr& m) {
  double stride = 0, child = 0;
  const std::size_t k = m->states.size();
  for (std::size_t i = 0; i + 1 < k; ++i)
    stride += std::abs(m->states[i + 1].id() - m->states[i].id());
  for (const bdd& r : m->states)
    if (r != bddtrue && r != bddfalse && !bdd_is_terminal(r))
      child += std::abs(bdd_low(r).id() - r.id()) / 2 + std::abs(bdd_high(r).id() - r.id()) / 2;
  std::printf("[%s] layout: mean root-to-next-root id stride=%.1f, mean root-to-child id distance=%.1f\n",
              tag, stride / (k ? k : 1), child / (k ? k : 1));
}

void report(const char* tag, const spot::mtdfa_ptr& m, const VariablePartition& vars,
            int repeat) {
  if (std::getenv("LAYOUT")) layout(tag, m);
  // Solve FIRST: the structural walk below touches every node and would
  // otherwise pre-warm the cache for the first solve.
  double t_first = 1e300;
  {
    heap_note();
    const long f0 = minflt();
    auto t0 = Clock::now();
    auto c = solve_mtdfa(m, vars);
    t_first = ms_since(t0);
    std::printf("[%s] first (cold) solve: %ld minor page faults\n", tag, minflt() - f0);
    (void)c;
  }
  std::printf("[%s] first (cold) game_solving=%.2f ms\n", tag, t_first);
  std::unordered_set<int> all;
  std::size_t per_root_sum = 0;
  std::set<int> top_vars;
  std::size_t inversions = 0;  // parent var > child var along some edge
  std::unordered_set<int> visited;
  std::vector<bdd> stack;
  for (const bdd& r : m->states) {
    std::unordered_set<int> own;
    walk(r, own);
    per_root_sum += own.size();
    walk(r, all);
    if (r != bddtrue && r != bddfalse && !bdd_is_terminal(r)) top_vars.insert(bdd_var(r));
    stack.push_back(r);
  }
  while (!stack.empty()) {
    bdd b = stack.back();
    stack.pop_back();
    if (b == bddtrue || b == bddfalse || bdd_is_terminal(b)) continue;
    if (!visited.insert(b.id()).second) continue;
    for (const bdd& c : {bdd_high(b), bdd_low(b)}) {
      if (c != bddtrue && c != bddfalse && !bdd_is_terminal(c) && bdd_var(c) <= bdd_var(b))
        ++inversions;
      stack.push_back(c);
    }
  }
  // synthesize() runs this under an active BenchScope (product_bdd_nodes)
  // immediately before game_solving; so does the probe.
  const auto st = m->get_stats(true, false);
  std::printf("[%s] roots=%u nodes=%u terminals=%u distinct_nodes=%zu "
              "sum_per_root_nodes=%zu sharing=%.2fx top_vars={", tag, st.states, st.nodes,
              st.terminals, all.size(), per_root_sum,
              all.empty() ? 0.0 : double(per_root_sum) / all.size());
  for (int v : top_vars) std::printf("%d,", v);
  std::printf("} order_violations=%zu\n", inversions);

  // Stage timings, min over `repeat`.  Each repeat re-sets the controllable
  // cube exactly as solve_mtdfa does, so the stages are the ones game_solving
  // is made of.
  double t_strat = 1e300, t_mealy = 1e300, t_total = 1e300;
  for (int i = 0; i < repeat; ++i) {
    auto t0 = Clock::now();
    auto c = solve_mtdfa(m, vars);
    t_total = std::min(t_total, ms_since(t0));
    (void)c;
  }
  {
    const auto dict = m->get_dict();
    bdd cube = bddtrue;
    std::set<std::string> ctl = vars.output_free;
    for (auto& n : vars.known()) ctl.insert(n);
    for (auto& n : ctl) cube &= bdd_ithvar(dict->varnum(spot::formula::ap(n)));
    m->set_controllable_variables(cube);
    spot::mtdfa_ptr strat;
    for (int i = 0; i < repeat; ++i) {
      auto t0 = Clock::now();
      strat = spot::mtdfa_winning_strategy(m, true);
      t_strat = std::min(t_strat, ms_since(t0));
    }
    for (int i = 0; i < repeat; ++i) {
      auto t0 = Clock::now();
      auto mealy = spot::mtdfa_strategy_to_mealy(strat, false, true);
      t_mealy = std::min(t_mealy, ms_since(t0));
    }
  }
  std::printf("[%s] game_solving=%.2f ms  (winning_strategy=%.2f, strategy_to_mealy=%.2f)\n",
              tag, t_total, t_strat, t_mealy);
}

}  // namespace

int main(int argc, char** argv) {
  const long n = argc > 1 ? std::atol(argv[1]) : 14;
  const int repeat = argc > 2 ? std::atoi(argv[2]) : 3;
  const std::string mode = argc > 3 ? argv[3] : "both";
  const BenchFamily* fam = nullptr;
  for (const auto& f : bench_families())
    if (f->name() == "cons-inert") fam = f.get();
  BenchCase c = fam->instantiate({{"n", n}, {"realizable", 1}});
  const auto dict = c.t_in.dict();
  const std::vector<const Transducer*> taus{&c.t_in, &c.t_out};

  if (mode == "steps-otf") {
    // OtfMtdfaProduct::synthesize's steps, by hand, no BenchScope.
    validate_product_inputs(c.phi, c.vars, taus);
    spot::mtdfa_ptr product = otf_product_to_mtdfa(c.phi, taus, c.vars, dict);
    (void)product->get_stats(true, false);
    heap_note();
    const long f0 = minflt();
    auto t0 = Clock::now();
    auto ctl = solve_mtdfa(product, c.vars);
    const double ms = ms_since(t0);
    std::printf("[steps-otf] game_solving=%.2f ms, %ld minor page faults\n", ms, minflt() - f0);
    return 0;
  }
  if (mode == "steps-spot") {
    // MtdfaProduct::synthesize's steps, by hand, no BenchScope: `goal` stays
    // alive to the end, the two agreement mtdfas die at the end of the
    // product block, exactly as in src/mtdfa_product.cpp.
    spot::mtdfa_ptr goal = spot::ltlf_to_mtdfa(c.phi, dict);
    spot::mtdfa_ptr product;
    {
      auto in_a = spot::twadfa_to_mtdfa(emits_dfa(c.t_in, dict));
      auto out_a = spot::twadfa_to_mtdfa(emits_dfa(c.t_out, dict));
      product = spot::product(spot::product(goal, in_a), out_a);
    }
    (void)product->get_stats(true, false);
    heap_note();
    const long f0 = minflt();
    auto t0 = Clock::now();
    auto ctl = solve_mtdfa(product, c.vars);
    const double ms = ms_since(t0);
    std::printf("[steps-spot] game_solving=%.2f ms, %ld minor page faults\n", ms, minflt() - f0);
    return 0;
  }
  if (mode == "synth-spot" || mode == "synth-otf") {
    BenchScope scope;
    if (mode == "synth-spot") MtdfaProduct().synthesize(c.phi, c.vars, c.t_in, c.t_out);
    else OtfMtdfaProduct().synthesize(c.phi, c.vars, c.t_in, c.t_out);
    for (const auto& sp : scope.report().roots)
      std::printf("[%s] %s = %.2f ms\n", mode.c_str(), sp.label.c_str(),
                  std::chrono::duration<double, std::milli>(sp.duration).count());
    return 0;
  }
  const auto build_spot = [&] {
    auto goal = spot::ltlf_to_mtdfa(c.phi, dict);
    auto in_a = spot::twadfa_to_mtdfa(emits_dfa(c.t_in, dict));
    auto out_a = spot::twadfa_to_mtdfa(emits_dfa(c.t_out, dict));
    return spot::product(spot::product(goal, in_a), out_a);
  };
  const auto build_otf = [&] { return otf_product_to_mtdfa(c.phi, taus, c.vars, dict); };
  if (mode == "spot-then-otf" || mode == "otf-then-spot") {
    const bool spot_first = mode == "spot-then-otf";
    {
      auto first = spot_first ? build_spot() : build_otf();
      report(spot_first ? "spot (1st)   " : "otf (1st)    ", first, c.vars, repeat);
    }
    auto second = spot_first ? build_otf() : build_spot();
    report(spot_first ? "otf (2nd)    " : "spot (2nd)   ", second, c.vars, repeat);
    return 0;
  }
  spot::mtdfa_ptr spot_prod, otf_prod;
  if (mode != "otf") spot_prod = build_spot();
  if (mode != "spot") otf_prod = build_otf();

  bddStat s0;
  bdd_stats(&s0);
  std::printf("n=%ld mode=%s  BuDDy before solve: produced=%ld nodenum=%d gc=%d\n", n,
              mode.c_str(), s0.produced, s0.nodenum, s0.gbcnum);
  if (spot_prod) report("spot::product", spot_prod, c.vars, repeat);
  if (otf_prod) report("otf          ", otf_prod, c.vars, repeat);
  bddStat s1;
  bdd_stats(&s1);
  std::printf("BuDDy after solve: produced=%ld nodenum=%d gc=%d\n", s1.produced,
              s1.nodenum, s1.gbcnum);
  return 0;
}
