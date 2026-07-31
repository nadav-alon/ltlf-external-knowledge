#pragma once

#include <cstdlib>
#include <set>
#include <string>

#include <bddx.h>
#include <spot/twa/twagraph.hh>

// Internal BuDDy/string infrastructure --- not domain concepts, no glossary
// entries (same policy as Transducer::dict()).  See
// docs/prd/architecture-cleanup.md.
namespace ltlf_ek::detail {

// Positive-literal variable cube of `names`, resolved on `aut`'s dict
// (register_ap is idempotent: an already-declared AP keeps its variable).
inline bdd cube_of(const std::set<std::string>& names,
                   const spot::twa_graph_ptr& aut) {
  bdd cube = bddtrue;
  for (const auto& n : names) cube &= bdd_ithvar(aut->register_ap(n));
  return cube;
}

// mkstemp/mkdtemp template for `stem`, rooted at $TMPDIR when it is set and at
// /tmp otherwise.  Hardcoding /tmp breaks under a sandbox that grants only the
// session temp dir --- mkstemp then fails on a read-only filesystem.
inline std::string temp_template(const std::string& stem) {
  const char* env = std::getenv("TMPDIR");
  std::string dir = (env && *env) ? env : "/tmp";
  while (dir.size() > 1 && dir.back() == '/') dir.pop_back();
  return dir + "/" + stem + "_XXXXXX";
}

inline std::string trim(const std::string& s) {
  const std::size_t a = s.find_first_not_of(" \t\r\n");
  if (a == std::string::npos) return "";
  const std::size_t b = s.find_last_not_of(" \t\r\n");
  return s.substr(a, b - a + 1);
}

}  // namespace ltlf_ek::detail
