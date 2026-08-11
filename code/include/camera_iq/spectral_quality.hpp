#pragma once

#include <array>
#include <string>
#include <vector>

namespace camera_iq {

// Camera color-quality via the Luther condition: how well the CIE color-matching
// functions can be written as a fixed linear (3x3) transform of the camera's
// spectral sensitivities. A camera whose SSFs span the human visual subspace
// (Luther satisfied) can reproduce color with no metamerism error. The metric
// is the relative residual of fitting each CMF from the three SSF channels;
// lower is better. The normalized metric contains no illuminant or reflectance,
// but comparisons still require commensurate SSF acquisition and processing;
// it does not erase cross-rig measurement systematics.
struct SpectralQualityInputs {
  std::vector<int> grid_nm;                 // common wavelength grid
  std::array<std::vector<double>, 3> ssf;   // camera R,G,B on grid
  std::array<std::vector<double>, 3> cmf;   // CIE xbar,ybar,zbar on grid
};

struct SpectralQualityResult {
  std::string method = "cmf_linear_fit_residual_luther";
  std::array<double, 3> cmf_residual{0, 0, 0};  // relative RMS for x,y,z fits
  double combined_residual = 0;                 // RMS over the three
  double quality_index = 0;                     // 1 - combined_residual
};

// Fits each CMF channel as a least-squares linear combination of the three SSF
// channels over the grid and reports the relative residual per CMF plus a
// combined figure. The wavelength grid must be strictly increasing. Throws on
// grid/size mismatch, non-finite samples, zero-norm channels, or a degenerate
// SSF basis.
SpectralQualityResult compute_spectral_quality(const SpectralQualityInputs& in);

}  // namespace camera_iq
