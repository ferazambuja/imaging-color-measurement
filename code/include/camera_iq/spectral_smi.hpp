#pragma once

#include <array>
#include <string>
#include <vector>

namespace camera_iq {

// Inputs for the camera Sensitivity Metamerism Index (SMI), evaluated in the
// style of ISO 17321: synthesize the camera's linear RGB response to a set of
// test colors under a reference illuminant, fit the optimal 3x3 RGB->XYZ
// transform, and measure the residual CIELAB color error against the true
// colorimetry of the same test colors. All spectral vectors share grid_nm.
struct SpectralSmiInputs {
  std::vector<double> grid_nm;                    // shared wavelength grid (nm)
  std::vector<double> illuminant;                 // E(lambda) on grid
  std::array<std::vector<double>, 3> cmf;         // xbar, ybar, zbar on grid
  std::array<std::vector<double>, 3> ssf;         // camera R, G, B on grid
  std::vector<std::vector<double>> reflectance;   // [patch][grid] test colors
  double smi_slope = 5.5;                         // SMI = 100 - slope * meanDE76
};

struct SpectralSmiResult {
  std::string method;
  std::size_t patch_count = 0;
  double mean_delta_e_76 = 0;
  double max_delta_e_76 = 0;
  double rms_delta_e_76 = 0;
  double mean_delta_e_2000 = 0;
  double max_delta_e_2000 = 0;
  double rms_delta_e_2000 = 0;
  double smi = 0;          // 100 - smi_slope * mean_delta_e_76
  double smi_slope = 5.5;
  std::array<std::array<double, 3>, 3> matrix{};  // fitted RGB->XYZ transform

  // Sensitivity check for the remaining ISO Annex-B optimization caveat: fit a
  // constrained 3x3 whose perfect-diffuser camera RGB maps exactly to the
  // illuminant white. This is reported alongside the default unconstrained fit;
  // it is not promoted as bit-exact ISO behavior.
  double white_preserving_mean_delta_e_76 = 0;
  double white_preserving_max_delta_e_76 = 0;
  double white_preserving_rms_delta_e_76 = 0;
  double white_preserving_mean_delta_e_2000 = 0;
  double white_preserving_smi = 0;
  double white_preserving_delta_smi = 0;  // constrained_smi - default_smi
  double white_preserving_white_delta_e_76 = 0;
  std::array<std::array<double, 3>, 3> white_preserving_matrix{};
};

// Throws std::runtime_error on a non-increasing or non-finite grid, grid/size
// mismatch, non-finite spectral inputs, fewer than three test colors, an
// invalid SMI slope, or a degenerate (non-positive white Y) illuminant/CMF
// pairing.
SpectralSmiResult compute_spectral_smi(const SpectralSmiInputs& in);

}  // namespace camera_iq
