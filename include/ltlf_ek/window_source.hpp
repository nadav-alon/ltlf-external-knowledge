#pragma once

#include <random>
#include <string>
#include <vector>

namespace ltlf_ek {

// The co-generated (T_in, psi_in) family (docs/prd/generated-corpus-oracle.md):
// one bounded-memory source, Iknown_t a total boolean function f
// of the free-input window ifree_t .. ifree_{t-d} (ifree_{<0} = false), seen
// two ways.  window_tin_file is the table T_in whose state is the last d
// free-input letters; window_psi_in is the safety formula in the guarded
// weak-X shape: pure safety, each clause "the free-input window seen so far
// -> weak X^i of the value f gives k", with the window cut short at the start
// (ifree_{<0} = false) and no strong X, so a total strategy imposes safety
// only (the delay member at d = 1 is (!k) & G(a -> X k) & G(!a -> X !k)).  The two denote the same language by
// construction, so an external psi_in is declared as data, never inferred
// from the transducer (comparability tier t1).
//
// Free inputs are a0, a1, ...; the one known input is k.

// The four members of the bounded-memory family.
enum class WindowKind { kConst, kCopy, kDelay, kWindow };

// The lowercase name of `kind` ("const", "copy", "delay", "window").
const char* WindowKindName(WindowKind kind);

// The bounded-memory source: which Iknown, window depth, boolean function.
struct WindowSource {
  WindowKind kind = WindowKind::kConst;
  int free_inputs = 1;  // m
  int depth = 0;        // d
  // Truth table of f, indexed by bit j*m + v = value of free input v at
  // position t-j.
  std::vector<bool> table;
};

// The window source's free input number `v`: "a<v>".
std::string free_input_name(int v);

// X^n(inner) with weak X.
std::string weak_next(int n, const std::string& inner);

// The delay member (k_t = a0_{t-depth}) at `depth` >= 1: one free input.
WindowSource delay_window_source(int depth);

// A random member of the family, drawn from `rng`.
WindowSource draw_window_source(std::mt19937& rng);

// The HOA + %%LAMBDA text of the table T_in of `src` (input file format of
// parse_transducer).
std::string window_tin_file(const WindowSource& src);

// The LTLf psi_in of `src`.
std::string window_psi_in(const WindowSource& src);

}  // namespace ltlf_ek
