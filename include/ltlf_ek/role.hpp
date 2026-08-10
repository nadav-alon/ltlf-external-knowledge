#pragma once

#include <set>
#include <string>

#include "ltlf_ek/variables.hpp"

namespace ltlf_ek {

// Which strategy a transducer materialises --- it selects the align-block
// columns that give the observed/produced slices (main.tex §121-131).  See
// docs/GLOSSARY.md ("Role").
//   t_in  --- Sigma0 = Ifree,        Sigma1 = Iknown.
//   t_out --- Sigma0 = I ∪ Ofree,    Sigma1 = Oknown.
//   t_c   --- Sigma0 = I,            Sigma1 = Ofree (the controller row,
//             main.tex:136, lambda_C: Q_C x 2^I -> 2^Ofree).  Unlike t_in/t_out
//             (external knowledge from a file), a t_c transducer is usually a
//             synthesized Controller viewed as a transducer
//             (controller_as_transducer, docs/prd/controller-verifier.md), or
//             a controller read from a --controller file.
enum class Role { t_in, t_out, t_c };

// Observed (Sigma0) and produced (Sigma1) variable names for (partition, role),
// per the align block (main.tex §121-131, docs/GLOSSARY.md "Role", "Observed /
// produced slice").  Exposed so callers other than parse_transducer (e.g. the
// CLI's trivial-transducer factory, docs/prd/cli-wrapper.md) can derive the
// same slices without duplicating the align-block logic.
struct SigmaSlices {
  std::set<std::string> sigma0;
  std::set<std::string> sigma1;
};
SigmaSlices sigma_slices(const VariablePartition& partition, Role role);

}  // namespace ltlf_ek
