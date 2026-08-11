#pragma once

#include <array>
#include <string>
#include <vector>

namespace camera_iq {

// All spectral inputs are pre-aligned to one common wavelength grid before they
// reach the core. Parsing/resampling of the messy archive files (CGATS
// reflectance, PR-655 illuminant, SSF CSV, RawDigger RGB) happens in the command
// layer so the physics here stays testable with small in-memory fixtures.
struct SpectralClosureInputs {
  std::vector<int> grid_nm;                       // common, uniform wavelength grid
  std::array<std::vector<double>, 3> ssf;         // R,G,B responsivity on grid
  std::vector<double> illuminant;                 // E(lambda) on grid
  std::vector<std::string> patch_ids;             // per-patch, aligned to reflectance/measured
  std::vector<std::vector<double>> reflectance;   // [patch][grid]
  std::vector<std::array<double, 3>> measured_rgb;// [patch] dark-subtracted camera RGB
  std::array<double, 3> white_rgb{0, 0, 0};       // white-card measured RGB (gate 1)
  double white_gate_max_ratio_error = 0.05;       // gate-1 tolerance on R/G,B/G
};

struct SpectralClosureChannel {
  double relative_rms = 0;      // rms(measured - k*predicted) / mean(measured)
  double correlation = 0;       // measured vs predicted across patches
  double scale_k_diagnostic = 0;// per-channel k, DIAGNOSTIC ONLY (closure uses one global k)
};

struct SpectralClosurePatch {
  std::string id;
  std::array<double, 3> measured{0, 0, 0};
  std::array<double, 3> predicted{0, 0, 0};  // global-k-scaled prediction
};

struct SpectralClosureResult {
  std::string validation_tier = "tier3_physical_closure";
  bool white_card_gate_attempted = true;
  bool white_card_gate_passes = false;
  double white_card_max_ratio_error = 0;
  std::array<double, 2> white_ratio_measured{0, 0};   // R/G, B/G
  std::array<double, 2> white_ratio_predicted{0, 0};
  double global_scale_k = 0;
  SpectralClosureChannel r, g, b;
  std::vector<SpectralClosurePatch> patches;
  std::string conclusion;
};

// Inputs must be finite and share a strictly increasing, uniformly spaced
// wavelength grid. Gate 1 (white-card illuminant pairing) runs first: the
// white-card measured
// channel ratios must agree with the SSF-times-illuminant neutral prediction
// within white_gate_max_ratio_error. If the gate fails, the closure is NOT
// computed and white_card_gate_passes is false. Otherwise a single global scale
// k is fit across all patches/channels and per-channel residuals are reported.
SpectralClosureResult compute_spectral_closure(const SpectralClosureInputs& in);

}  // namespace camera_iq
