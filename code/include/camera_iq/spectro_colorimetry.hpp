#pragma once

#include <array>
#include <string>
#include <vector>

#include "camera_iq/spectro_measurement.hpp"

namespace camera_iq {

struct SpectroCmfTable {
  std::vector<double> wavelength_nm;
  std::array<std::vector<double>, 3> xyz_bar;
};

struct SpectroClosureReading {
  std::array<double, 3> computed_xyz{};
  std::array<double, 3> signed_relative_residual_percent{};
};

struct SpectroClosureResult {
  std::string sample_weighting = "uniform_equal_weight";
  std::string scale_source = "derived_from_recorded_xyz";
  double scale_value = 0.0;
  double max_absolute_relative_residual_percent = 0.0;
  double rms_relative_residual_percent = 0.0;
  std::vector<SpectroClosureReading> readings;
};

SpectroCmfTable read_spectro_cmf_csv(const std::string &csv_text);

// Recomputes XYZ with equal sample weights and one archive-derived
// proportional scale fitted across every reading and channel. Agreement is a
// numerical closure check; it does not identify undocumented instrument
// software or turn the fitted scale into a standard luminous-efficacy value.
SpectroClosureResult
compute_spectro_closure(const std::vector<SpectroMeasurement> &readings,
                        const SpectroCmfTable &observer);

} // namespace camera_iq
