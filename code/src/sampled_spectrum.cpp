#include "camera_iq/sampled_spectrum.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace camera_iq {
namespace {

class CompensatedSum {
 public:
  void add(double value) {
    const double next = sum_ + value;
    if (std::fabs(sum_) >= std::fabs(value)) {
      correction_ += (sum_ - next) + value;
    } else {
      correction_ += (value - next) + sum_;
    }
    sum_ = next;
  }

  [[nodiscard]] double value() const { return sum_ + correction_; }

 private:
  double sum_ = 0.0;
  double correction_ = 0.0;
};

double equal_weight_integral(const SampledSpectrum& reading,
                             double wavelength_step_nm) {
  double max_magnitude = 0.0;
  double ordinary_sum = 0.0;
  double ordinary_correction = 0.0;
  bool ordinary_representable = true;
  for (const double sample : reading.values) {
    if (!std::isfinite(sample)) {
      throw std::runtime_error(
          "sampled spectrum: values must be finite");
    }
    max_magnitude = std::max(max_magnitude, std::fabs(sample));
    if (ordinary_representable) {
      const double next = ordinary_sum + sample;
      if (!std::isfinite(next)) {
        ordinary_representable = false;
      } else {
        if (std::fabs(ordinary_sum) >= std::fabs(sample)) {
          ordinary_correction += (ordinary_sum - next) + sample;
        } else {
          ordinary_correction += (sample - next) + ordinary_sum;
        }
        ordinary_sum = next;
        ordinary_representable = std::isfinite(ordinary_correction);
      }
    }
  }

  if (ordinary_representable) {
    const double integral =
        (ordinary_sum + ordinary_correction) * wavelength_step_nm;
    if (std::isfinite(integral)) {
      if (integral <= 0.0) {
        throw std::runtime_error(
            "sampled spectrum: integral must be finite and positive");
      }
      return integral;
    }
  }

  int sample_exponent = 0;
  if (max_magnitude > 0.0) {
    (void)std::frexp(max_magnitude, &sample_exponent);
  }
  CompensatedSum sum;
  for (const double sample : reading.values) {
    sum.add(std::ldexp(sample, -sample_exponent));
  }
  int step_exponent = 0;
  const double step_fraction =
      std::frexp(wavelength_step_nm, &step_exponent);
  const double integral = std::ldexp(sum.value() * step_fraction,
                                     sample_exponent + step_exponent);
  if (!std::isfinite(integral) || integral <= 0.0) {
    throw std::runtime_error(
        "sampled spectrum: integral must be finite and positive");
  }
  return integral;
}

double representable_mean(const std::vector<double>& values) {
  double max_magnitude = 0.0;
  for (const double value : values) {
    max_magnitude = std::max(max_magnitude, std::fabs(value));
  }
  int exponent = 0;
  if (max_magnitude > 0.0) {
    (void)std::frexp(max_magnitude, &exponent);
  }
  CompensatedSum sum;
  const double divisor = static_cast<double>(values.size());
  for (const double value : values) {
    sum.add(std::ldexp(value, -exponent) / divisor);
  }
  const double result = std::ldexp(sum.value(), exponent);
  if (!std::isfinite(result)) {
    throw std::runtime_error("sampled spectrum: mean is not representable");
  }
  return result;
}

double sample_stddev(const std::vector<double>& values, double mean) {
  double max_magnitude = std::fabs(mean);
  for (const double value : values) {
    max_magnitude = std::max(max_magnitude, std::fabs(value));
  }
  int exponent = 0;
  if (max_magnitude > 0.0) {
    (void)std::frexp(max_magnitude, &exponent);
  }
  const double scaled_mean = std::ldexp(mean, -exponent);
  CompensatedSum sum;
  const double divisor = static_cast<double>(values.size() - 1);
  for (const double value : values) {
    const double difference = std::ldexp(value, -exponent) - scaled_mean;
    sum.add((difference * difference) / divisor);
  }
  const double result = std::ldexp(std::sqrt(sum.value()), exponent);
  if (!std::isfinite(result)) {
    throw std::runtime_error(
        "sampled spectrum: sample standard deviation is not representable");
  }
  return result;
}

}  // namespace

SampledSpectrumGroupAnalysis analyze_sampled_spectrum_group(
    const std::vector<SampledSpectrum>& readings) {
  if (readings.empty()) {
    throw std::runtime_error("sampled spectrum: group is empty");
  }
  const auto& wavelengths = readings.front().wavelength_nm;
  if (wavelengths.size() < 2 ||
      readings.front().values.size() != wavelengths.size()) {
    throw std::runtime_error("sampled spectrum: wavelength grid is too short");
  }
  const double step = wavelengths[1] - wavelengths[0];
  if (!std::isfinite(step) || step <= 0.0) {
    throw std::runtime_error(
        "sampled spectrum: wavelength step must be finite and positive");
  }
  for (std::size_t index = 1; index < wavelengths.size(); ++index) {
    const double wavelength = wavelengths[index];
    const double observed_step = wavelength - wavelengths[index - 1];
    if (!std::isfinite(wavelength) ||
        std::fabs(observed_step - step) > 1e-9) {
      throw std::runtime_error(
          "sampled spectrum: analysis requires a uniform grid");
    }
  }

  SampledSpectrumGroupAnalysis result;
  result.count = readings.size();
  result.wavelength_step_nm = step;
  result.mean_normalized_spectrum.assign(wavelengths.size(), 0.0);
  result.readings.reserve(readings.size());
  std::vector<double> integrals;
  integrals.reserve(readings.size());

  for (const auto& reading : readings) {
    if (reading.wavelength_nm != wavelengths ||
        reading.values.size() != wavelengths.size()) {
      throw std::runtime_error(
          "sampled spectrum: readings must share one wavelength grid");
    }
    SampledSpectrumReadingAnalysis analyzed;
    analyzed.spectral_integral = equal_weight_integral(reading, step);
    analyzed.normalized_spectrum.reserve(wavelengths.size());
    for (const double value : reading.values) {
      const double normalized = value / analyzed.spectral_integral;
      if (!std::isfinite(normalized)) {
        throw std::runtime_error(
            "sampled spectrum: normalized value is not representable");
      }
      analyzed.normalized_spectrum.push_back(normalized);
    }
    integrals.push_back(analyzed.spectral_integral);
    result.readings.push_back(std::move(analyzed));
  }

  result.mean_spectral_integral = representable_mean(integrals);
  for (std::size_t wavelength = 0; wavelength < wavelengths.size();
       ++wavelength) {
    std::vector<double> values;
    values.reserve(readings.size());
    for (const auto& reading : result.readings) {
      values.push_back(reading.normalized_spectrum[wavelength]);
    }
    result.mean_normalized_spectrum[wavelength] = representable_mean(values);
  }
  if (readings.size() == 1) {
    return result;
  }

  result.sample_stddev_spectral_integral =
      sample_stddev(integrals, result.mean_spectral_integral);
  result.coefficient_of_variation =
      *result.sample_stddev_spectral_integral / result.mean_spectral_integral;
  if (!std::isfinite(*result.coefficient_of_variation)) {
    throw std::runtime_error(
        "sampled spectrum: coefficient of variation is not representable");
  }

  std::vector<double> normalized_stddev(wavelengths.size(), 0.0);
  for (std::size_t wavelength = 0; wavelength < wavelengths.size();
       ++wavelength) {
    std::vector<double> values;
    values.reserve(readings.size());
    for (const auto& reading : result.readings) {
      values.push_back(reading.normalized_spectrum[wavelength]);
    }
    normalized_stddev[wavelength] =
        sample_stddev(values, result.mean_normalized_spectrum[wavelength]);
  }
  result.sample_stddev_normalized_spectrum = std::move(normalized_stddev);

  double mean_norm = 0.0;
  for (const double sample : result.mean_normalized_spectrum) {
    mean_norm = std::hypot(mean_norm, sample);
  }
  if (!std::isfinite(mean_norm) || mean_norm <= 0.0) {
    throw std::runtime_error(
        "sampled spectrum: normalized mean has no representable L2 norm");
  }
  double maximum = 0.0;
  for (const auto& reading : result.readings) {
    double residual_norm = 0.0;
    for (std::size_t index = 0; index < wavelengths.size(); ++index) {
      residual_norm = std::hypot(
          residual_norm, reading.normalized_spectrum[index] -
                             result.mean_normalized_spectrum[index]);
    }
    const double relative = residual_norm / mean_norm;
    if (!std::isfinite(relative)) {
      throw std::runtime_error(
          "sampled spectrum: relative L2 residual is not representable");
    }
    maximum = std::max(maximum, relative);
  }
  result.max_shape_relative_l2 = maximum;
  return result;
}

}  // namespace camera_iq
