#include "ltlf_ek/window_source.hpp"

#include <random>
#include <sstream>
#include <string>
#include <vector>

namespace ltlf_ek {

std::string free_input_name(int v) { return "a" + std::to_string(v); }

namespace {

constexpr int kCoGenFreeInputsMax = 2;       // m in [1, kCoGenFreeInputsMax].
constexpr int kCoGenDepthMax = 2;            // d in [0, kCoGenDepthMax].
constexpr int kCoGenWindowBitsMax = 4;       // m * (d + 1) <= this.

// X^n(inner) with weak X.
std::string weak_next(int n, const std::string& inner) {
  std::string text = inner;
  for (int i = 0; i < n; ++i) text = "X(" + text + ")";
  return text;
}

// Conjunction of literals over `names` selected by the bits of `bits`.
std::string cube_text(const std::vector<std::string>& names, unsigned bits) {
  std::string text = "1";
  for (std::size_t i = 0; i < names.size(); ++i)
    text += std::string(" & ") + (((bits >> i) & 1u) ? "" : "!") + names[i];
  return "(" + text + ")";
}

}  // namespace

WindowSource delay_window_source(int depth) {
  WindowSource src;
  src.kind = WindowKind::kDelay;
  src.free_inputs = 1;
  src.depth = depth;
  src.table.assign(1u << (depth + 1), false);
  for (unsigned idx = 0; idx < src.table.size(); ++idx)
    src.table[idx] = (idx >> depth) & 1u;
  return src;
}

WindowSource draw_window_source(std::mt19937& rng) {
  WindowSource src;
  const int kind = std::uniform_int_distribution<int>(0, 3)(rng);
  std::bernoulli_distribution coin(0.5);
  switch (kind) {
    case 0:  // const: ignores the window.
      src.kind = WindowKind::kConst;
      src.free_inputs = 1;
      src.depth = 0;
      src.table = {false, false};
      if (coin(rng)) src.table = {true, true};
      break;
    case 1:  // copy: k_t = a0_t.
      src.kind = WindowKind::kCopy;
      src.free_inputs = 1;
      src.depth = 0;
      src.table = {false, true};
      break;
    case 2:  // delay: k_t = a0_{t-d}.
      src = delay_window_source(
          std::uniform_int_distribution<int>(1, kCoGenDepthMax)(rng));
      break;
    default: {  // window: a random boolean function of the whole window.
      src.kind = WindowKind::kWindow;
      do {
        src.free_inputs =
            std::uniform_int_distribution<int>(1, kCoGenFreeInputsMax)(rng);
        src.depth =
            std::uniform_int_distribution<int>(0, kCoGenDepthMax)(rng);
      } while (src.free_inputs * (src.depth + 1) > kCoGenWindowBitsMax);
      src.table.assign(1u << (src.free_inputs * (src.depth + 1)), false);
      for (unsigned idx = 0; idx < src.table.size(); ++idx)
        src.table[idx] = coin(rng);
      break;
    }
  }
  return src;
}

// The table T_in of a window source: state = the last d free-input letters
// (initially all false), edge on the current letter shifts it in, lambda(q)
// sets k to f(current letter, q).
std::string window_tin_file(const WindowSource& src) {
  const int m = src.free_inputs;
  const unsigned letters = 1u << m;
  const unsigned states = 1u << (m * src.depth);
  std::vector<std::string> in_names;
  for (int v = 0; v < m; ++v) in_names.push_back(free_input_name(v));

  std::ostringstream os;
  os << "HOA: v1\nStates: " << states << "\nStart: 0\nAP: " << (m + 1);
  for (const std::string& n : in_names) os << " \"" << n << "\"";
  os << " \"k\"\nacc-name: all\nAcceptance: 0 t\n--BODY--\n";
  for (unsigned q = 0; q < states; ++q) {
    os << "State: " << q << "\n";
    for (unsigned c = 0; c < letters; ++c) {
      std::string guard;
      for (int v = 0; v < m; ++v)
        guard += std::string(v ? "&" : "") + (((c >> v) & 1u) ? "" : "!") +
                 std::to_string(v);
      const unsigned next = ((q << m) | c) & (states - 1u);
      os << "  [" << guard << "] " << next << "\n";
    }
  }
  os << "--END--\n%%LAMBDA\n";
  for (unsigned q = 0; q < states; ++q) {
    os << "state " << q << ": ";
    for (unsigned c = 0; c < letters; ++c) {
      const bool k = src.table[c | (q << m)];
      os << (c ? " | " : "") << "(" << cube_text(in_names, c) << " & "
         << (k ? "k" : "!k") << ")";
    }
    os << "\n";
  }
  return os.str();
}

// psi_in of a window source, in the guarded weak-X shape described above.
// `h` assigns the free inputs at positions 0..len-1 of a window starting at
// the current position (bit i*m + v = input v at offset i); f at its last
// position reads a_{last-j} from offset last-j, and false beyond the start.
std::string window_psi_in(const WindowSource& src) {
  const int m = src.free_inputs;
  const int d = src.depth;
  auto clause = [&](int len) {
    const int last = len - 1;
    std::string conj = "1";
    for (unsigned h = 0; h < (1u << (m * len)); ++h) {
      std::string premise = "1";
      for (int i = 0; i < len; ++i)
        for (int v = 0; v < m; ++v)
          premise += std::string(" & ") +
                     (((h >> (i * m + v)) & 1u) ? "" : "!") +
                     weak_next(i, free_input_name(v));
      unsigned idx = 0;
      for (int j = 0; j <= last; ++j)
        for (int v = 0; v < m; ++v)
          if ((h >> ((last - j) * m + v)) & 1u) idx |= 1u << (j * m + v);
      conj += " & ((" + premise + ") -> " +
              weak_next(last, src.table[idx] ? "k" : "!k") + ")";
    }
    return conj;
  };
  std::string psi = "1";
  for (int s = 0; s < d; ++s) psi += " & (" + clause(s + 1) + ")";
  psi += " & G(" + clause(d + 1) + ")";
  return psi;
}

}  // namespace ltlf_ek
