#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace camera_iq {

// A sampled scalar spectrum with no implied radiometric or colorimetric unit.
// Algorithms that consume it must state their normalization and weighting.
struct SampledSpectrum {
  std::vector<double> wavelength_nm;
  std::vector<double> values;
};

struct SampledSpectrumReadingAnalysis {
  double spectral_integral = 0.0;
  std::vector<double> normalized_spectrum;
};

struct SampledSpectrumGroupAnalysis {
  std::size_t count = 0;
  double wavelength_step_nm = 0.0;
  std::string sample_weighting = "uniform_equal_weight";
  std::vector<SampledSpectrumReadingAnalysis> readings;

  double mean_spectral_integral = 0.0;
  std::optional<double> sample_stddev_spectral_integral;
  std::optional<double> coefficient_of_variation;

  std::vector<double> mean_normalized_spectrum;
  std::optional<std::vector<double>> sample_stddev_normalized_spectrum;
  std::optional<double> max_shape_relative_l2;
};

// Separates absolute sampled level from normalized shape. Every reading must
// share one finite, strictly increasing uniform wavelength grid. The integral
// uses the native sample width and equal weights at every retained wavelength.
SampledSpectrumGroupAnalysis analyze_sampled_spectrum_group(
    const std::vector<SampledSpectrum>& readings);

}  // namespace camera_iq
