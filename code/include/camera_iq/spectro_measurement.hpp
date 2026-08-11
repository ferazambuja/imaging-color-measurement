#pragma once

#include <array>
#include <cstddef>
#include <optional>
#include <vector>

namespace camera_iq {

// One validated spectroradiometer reading. CCT and Duv are carried through as
// recorded metadata because the source files do not identify the locus and
// distance conventions needed to recompute them unambiguously.
struct SpectroMeasurement {
  std::vector<double> wavelength_nm;
  std::vector<double> spectral_radiance;
  std::array<double, 3> recorded_xyz{};
  double recorded_total_radiance = 0.0;
  double recorded_cct_k = 0.0;
  double recorded_duv = 0.0;
  bool repeat_on_error = false;
  std::size_t current_repetitions = 0;
};

struct SpectroGroupSummary {
  std::size_t count = 0;
  std::vector<double> wavelength_nm;
  std::vector<double> mean_spectral_radiance;
  std::optional<std::vector<double>> sample_stddev_spectral_radiance;
  std::array<double, 3> mean_recorded_xyz{};
  std::optional<std::array<double, 3>> sample_stddev_recorded_xyz;
};

// Summarizes explicitly grouped readings after exact wavelength-axis
// validation. Sample standard deviations use n-1 and are absent for a singleton;
// one measurement does not establish repeatability.
SpectroGroupSummary summarize_spectro_group(
    const std::vector<SpectroMeasurement>& readings);

}  // namespace camera_iq
