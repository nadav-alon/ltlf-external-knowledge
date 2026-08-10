#include "ltlf_ek/role.hpp"

namespace ltlf_ek {

// Sigma1 --- the *known* vars --- is mode-invariant; Sigma0 is the Mealy
// observed slice.  A future Moore mode would shrink Sigma0 here without
// touching the file format (see the PRD's mode note).
SigmaSlices sigma_slices(const VariablePartition& p, Role role) {
  SigmaSlices s;
  switch (role) {
    case Role::t_in:  // Sigma0 = Ifree, Sigma1 = Iknown.
      s.sigma0 = p.input_free;
      s.sigma1 = p.input_known;
      break;
    case Role::t_out:  // Sigma0 = I ∪ Ofree, Sigma1 = Oknown.
      s.sigma0 = p.inputs();
      s.sigma0.insert(p.output_free.begin(), p.output_free.end());
      s.sigma1 = p.output_known;
      break;
    case Role::t_c:  // Sigma0 = I, Sigma1 = Ofree (main.tex:136).
      s.sigma0 = p.inputs();
      s.sigma1 = p.output_free;
      break;
  }
  return s;
}

}  // namespace ltlf_ek
