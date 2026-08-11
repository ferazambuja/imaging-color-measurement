#include "camera_iq/spectro_analysis.hpp"

#include "camera_iq/sampled_spectrum.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>

namespace camera_iq {
namespace {

SpectroChromaticity chromaticity(const std::array<double, 3>& xyz) {
  double max_magnitude = 0.0;
  for (const double value : xyz) {
    if (!std::isfinite(value)) {
      throw std::runtime_error(
          "spectro analysis: recorded XYZ cannot form chromaticity");
    }
    max_magnitude = std::max(max_magnitude, std::fabs(value));
  }
  int exponent = 0;
  if (max_magnitude > 0.0) {
    (void)std::frexp(max_magnitude, &exponent);
  }
  const std::array<double, 3> scaled = {
      std::ldexp(xyz[0], -exponent), std::ldexp(xyz[1], -exponent),
      std::ldexp(xyz[2], -exponent)};
  const double xyz_sum = scaled[0] + scaled[1] + scaled[2];
  const double uv_denominator =
      scaled[0] + 15.0 * scaled[1] + 3.0 * scaled[2];
  if (!std::isfinite(xyz_sum) || !std::isfinite(uv_denominator) ||
      xyz_sum <= 0.0 || uv_denominator <= 0.0) {
    throw std::runtime_error(
        "spectro analysis: recorded XYZ cannot form chromaticity");
  }
  return SpectroChromaticity{scaled[0] / xyz_sum, scaled[1] / xyz_sum,
                             4.0 * scaled[0] / uv_denominator,
                             9.0 * scaled[1] / uv_denominator};
}

}  // namespace

SpectroGroupAnalysis analyze_spectro_group(
    const std::vector<SpectroMeasurement>& readings) {
  std::vector<SampledSpectrum> spectra;
  spectra.reserve(readings.size());
  for (const auto& reading : readings) {
    spectra.push_back(
        SampledSpectrum{reading.wavelength_nm, reading.spectral_radiance});
  }
  const auto generic = analyze_sampled_spectrum_group(spectra);

  SpectroGroupAnalysis result;
  result.count = generic.count;
  result.wavelength_step_nm = generic.wavelength_step_nm;
  result.sample_weighting = generic.sample_weighting;
  result.mean_spectral_integral = generic.mean_spectral_integral;
  result.sample_stddev_spectral_integral =
      generic.sample_stddev_spectral_integral;
  result.coefficient_of_variation = generic.coefficient_of_variation;
  result.mean_normalized_spectrum = generic.mean_normalized_spectrum;
  result.sample_stddev_normalized_spectrum =
      generic.sample_stddev_normalized_spectrum;
  result.max_shape_relative_l2 = generic.max_shape_relative_l2;
  result.readings.reserve(readings.size());
  for (std::size_t index = 0; index < readings.size(); ++index) {
    result.readings.push_back(SpectroReadingAnalysis{
        generic.readings[index].spectral_integral,
        generic.readings[index].normalized_spectrum,
        chromaticity(readings[index].recorded_xyz)});
  }

  if (readings.size() < 2) {
    return result;
  }
  double maximum = 0.0;
  for (std::size_t first = 0; first < result.readings.size(); ++first) {
    for (std::size_t second = first + 1; second < result.readings.size();
         ++second) {
      const auto& a = result.readings[first].recorded_xyz_chromaticity;
      const auto& b = result.readings[second].recorded_xyz_chromaticity;
      maximum = std::max(
          maximum,
          std::hypot(a.u_prime - b.u_prime, a.v_prime - b.v_prime));
    }
  }
  result.max_pair_delta_u_prime_v_prime = maximum;
  return result;
}

}  // namespace camera_iq
