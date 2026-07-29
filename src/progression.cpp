#include "ltlf_ek/progression.hpp"

namespace ltlf_ek {

ForwardProgression::ForwardProgression(const spot::bdd_dict_ptr& dict)
    : translator_(dict) {}

bdd ForwardProgression::progress_row(const spot::formula& psi) {
  return translator_.ltlf_to_mtbdd(psi);
}

std::pair<spot::formula, bool> ForwardProgression::decode(int terminal) const {
  return {translator_.terminal_to_formula(terminal), (terminal & 1) != 0};
}

}  // namespace ltlf_ek
