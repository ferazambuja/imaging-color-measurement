#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include "camera_iq/spectro_measurement.hpp"

namespace camera_iq {

struct SpectroChromaticity {
  double x = 0.0;
  double y = 0.0;
  double u_prime = 0.0;
  double v_prime = 0.0;
};

struct SpectroReadingAnalysis {
  double spectral_integral = 0.0;
  std::vector<double> normalized_spectrum;
  SpectroChromaticity recorded_xyz_chromaticity;
};

struct SpectroGroupAnalysis {
  std::size_t count = 0;
  double wavelength_step_nm = 0.0;
  std::string sample_weighting = "uniform_equal_weight";
  std::vector<SpectroReadingAnalysis> readings;

  double mean_spectral_integral = 0.0;
  std::optional<double> sample_stddev_spectral_integral;
  std::optional<double> coefficient_of_variation;

  std::vector<double> mean_normalized_spectrum;
  std::optional<std::vector<double>> sample_stddev_normalized_spectrum;
  std::optional<double> max_shape_relative_l2;
  std::optional<double> max_pair_delta_u_prime_v_prime;
};

// Separates absolute spectral level from normalized spectral shape. The
// integral uses the archive grid's equal sample width on every point. Recorded
// XYZ supplies chromaticity only; its scale and CCT/Duv conventions are not
// inferred here.
SpectroGroupAnalysis
analyze_spectro_group(const std::vector<SpectroMeasurement> &readings);

} // namespace camera_iq
