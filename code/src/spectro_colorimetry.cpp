#include "camera_iq/spectro_colorimetry.hpp"

#include <algorithm>
#include <cmath>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace camera_iq {
namespace {

std::array<std::string_view, 4> split_row(std::string_view row) {
  std::array<std::string_view, 4> fields{};
  std::size_t start = 0;
  for (std::size_t index = 0; index < fields.size(); ++index) {
    const std::size_t comma = row.find(',', start);
    if (index + 1 == fields.size()) {
      if (comma != std::string_view::npos) {
        throw std::runtime_error("spectro CMF: row has too many fields");
      }
      fields[index] = row.substr(start);
      return fields;
    }
    if (comma == std::string_view::npos) {
      throw std::runtime_error("spectro CMF: row has too few fields");
    }
    fields[index] = row.substr(start, comma - start);
    start = comma + 1;
  }
  return fields;
}

double number(std::string_view token) {
  try {
    std::size_t consumed = 0;
    const std::string text(token);
    const double value = std::stod(text, &consumed);
    if (consumed != text.size() || !std::isfinite(value)) {
      throw std::runtime_error("");
    }
    return value;
  } catch (...) {
    throw std::runtime_error("spectro CMF: invalid numeric field");
  }
}

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

  double value() const { return sum_ + correction_; }

private:
  double sum_ = 0.0;
  double correction_ = 0.0;
};

std::array<double, 3> integrate_unscaled(const SpectroMeasurement &reading,
                                         const SpectroCmfTable &observer) {
  if (reading.wavelength_nm.size() < 2 ||
      reading.wavelength_nm.size() != reading.spectral_radiance.size()) {
    throw std::runtime_error("spectro closure: invalid measurement grid");
  }
  const double step = reading.wavelength_nm[1] - reading.wavelength_nm[0];
  if (!std::isfinite(step) || step <= 0.0) {
    throw std::runtime_error("spectro closure: invalid wavelength step");
  }
  std::vector<std::size_t> observer_indices;
  observer_indices.reserve(reading.wavelength_nm.size());
  std::array<double, 3> ordinary_result{};
  bool ordinary_representable = true;
  double radiance_magnitude = 0.0;
  for (const double sample : reading.spectral_radiance) {
    if (!std::isfinite(sample)) {
      throw std::runtime_error(
          "spectro closure: spectral radiance must be finite");
    }
    radiance_magnitude = std::max(radiance_magnitude, std::fabs(sample));
  }
  for (std::size_t index = 0; index < reading.wavelength_nm.size(); ++index) {
    if (index > 0 && std::fabs((reading.wavelength_nm[index] -
                                reading.wavelength_nm[index - 1]) -
                               step) > 1e-9) {
      throw std::runtime_error(
          "spectro closure: equal weighting requires a uniform grid");
    }
    const auto found = std::lower_bound(observer.wavelength_nm.begin(),
                                        observer.wavelength_nm.end(),
                                        reading.wavelength_nm[index]);
    if (found == observer.wavelength_nm.end() ||
        *found != reading.wavelength_nm[index]) {
      throw std::runtime_error(
          "spectro closure: observer and measurement axes do not match");
    }
    const std::size_t observer_index =
        static_cast<std::size_t>(found - observer.wavelength_nm.begin());
    observer_indices.push_back(observer_index);
    for (std::size_t channel = 0; channel < ordinary_result.size(); ++channel) {
      const double term = reading.spectral_radiance[index] *
                          observer.xyz_bar[channel][observer_index] * step;
      const double next = ordinary_result[channel] + term;
      if (!std::isfinite(term) || !std::isfinite(next)) {
        ordinary_representable = false;
      } else {
        ordinary_result[channel] = next;
      }
    }
  }
  if (ordinary_representable)
    return ordinary_result;

  int radiance_exponent = 0;
  if (radiance_magnitude > 0.0)
    (void)std::frexp(radiance_magnitude, &radiance_exponent);

  int step_exponent = 0;
  const double step_fraction = std::frexp(step, &step_exponent);
  std::array<CompensatedSum, 3> sums;
  std::array<int, 3> observer_exponents{};
  for (std::size_t channel = 0; channel < observer.xyz_bar.size(); ++channel) {
    double magnitude = 0.0;
    for (const double value : observer.xyz_bar[channel])
      magnitude = std::max(magnitude, std::fabs(value));
    if (magnitude > 0.0)
      (void)std::frexp(magnitude, &observer_exponents[channel]);
  }
  for (std::size_t index = 0; index < reading.wavelength_nm.size(); ++index) {
    const double scaled_radiance =
        std::ldexp(reading.spectral_radiance[index], -radiance_exponent);
    for (std::size_t channel = 0; channel < sums.size(); ++channel) {
      const double scaled_observer = std::ldexp(
          observer.xyz_bar[channel][observer_indices[index]],
          -observer_exponents[channel]);
      sums[channel].add(scaled_radiance * scaled_observer);
    }
  }

  std::array<double, 3> result{};
  for (std::size_t channel = 0; channel < result.size(); ++channel) {
    result[channel] = std::ldexp(
        sums[channel].value() * step_fraction,
        radiance_exponent + observer_exponents[channel] + step_exponent);
  }
  return result;
}

} // namespace

SpectroCmfTable read_spectro_cmf_csv(const std::string &csv_text) {
  std::istringstream input(csv_text);
  std::string line;
  if (!std::getline(input, line)) {
    throw std::runtime_error("spectro CMF: unexpected header");
  }
  if (!line.empty() && line.back() == '\r')
    line.pop_back();
  if (line != "Wavelength (nm),X,Y,Z") {
    throw std::runtime_error("spectro CMF: unexpected header");
  }
  SpectroCmfTable result;
  while (std::getline(input, line)) {
    if (!line.empty() && line.back() == '\r')
      line.pop_back();
    if (line.empty()) {
      throw std::runtime_error("spectro CMF: blank row");
    }
    const auto fields = split_row(line);
    const double wavelength = number(fields[0]);
    if (!result.wavelength_nm.empty() &&
        wavelength <= result.wavelength_nm.back()) {
      throw std::runtime_error(
          "spectro CMF: wavelength grid must be strictly increasing");
    }
    result.wavelength_nm.push_back(wavelength);
    for (std::size_t channel = 0; channel < result.xyz_bar.size(); ++channel) {
      result.xyz_bar[channel].push_back(number(fields[channel + 1]));
    }
  }
  if (result.wavelength_nm.size() < 2) {
    throw std::runtime_error("spectro CMF: at least two rows are required");
  }
  return result;
}

SpectroClosureResult
compute_spectro_closure(const std::vector<SpectroMeasurement> &readings,
                        const SpectroCmfTable &observer) {
  if (readings.empty()) {
    throw std::runtime_error("spectro closure: no readings");
  }
  std::vector<std::array<double, 3>> unscaled;
  unscaled.reserve(readings.size());
  double max_unscaled = 0.0;
  double max_recorded = 0.0;
  for (const SpectroMeasurement &reading : readings) {
    const auto xyz = integrate_unscaled(reading, observer);
    unscaled.push_back(xyz);
    for (std::size_t channel = 0; channel < xyz.size(); ++channel) {
      const double recorded = reading.recorded_xyz[channel];
      if (!std::isfinite(xyz[channel]) || !std::isfinite(recorded) ||
          recorded == 0.0) {
        throw std::runtime_error(
            "spectro closure: XYZ values must be finite and recorded XYZ "
            "must be nonzero");
      }
      max_unscaled = std::max(max_unscaled, std::fabs(xyz[channel]));
      max_recorded = std::max(max_recorded, std::fabs(recorded));
    }
  }
  if (max_unscaled == 0.0 || max_recorded == 0.0) {
    throw std::runtime_error("spectro closure: scale fit is not finite");
  }

  int unscaled_exponent = 0;
  int recorded_exponent = 0;
  (void)std::frexp(max_unscaled, &unscaled_exponent);
  (void)std::frexp(max_recorded, &recorded_exponent);
  CompensatedSum numerator;
  CompensatedSum denominator;
  for (std::size_t index = 0; index < readings.size(); ++index) {
    for (std::size_t channel = 0; channel < 3; ++channel) {
      const double scaled_unscaled =
          std::ldexp(unscaled[index][channel], -unscaled_exponent);
      const double scaled_recorded = std::ldexp(
          readings[index].recorded_xyz[channel], -recorded_exponent);
      numerator.add(scaled_recorded * scaled_unscaled);
      denominator.add(scaled_unscaled * scaled_unscaled);
    }
  }
  if (!std::isfinite(numerator.value()) ||
      !std::isfinite(denominator.value()) || denominator.value() <= 0.0) {
    throw std::runtime_error("spectro closure: scale fit is not finite");
  }

  SpectroClosureResult result;
  result.scale_value = std::ldexp(numerator.value() / denominator.value(),
                                  recorded_exponent - unscaled_exponent);
  if (!std::isfinite(result.scale_value) || result.scale_value <= 0.0) {
    throw std::runtime_error("spectro closure: fitted scale is not positive");
  }
  double relative_residual_norm = 0.0;
  std::size_t residual_count = 0;
  result.readings.reserve(readings.size());
  for (std::size_t index = 0; index < readings.size(); ++index) {
    SpectroClosureReading closure;
    for (std::size_t channel = 0; channel < closure.computed_xyz.size();
         ++channel) {
      const double recorded = readings[index].recorded_xyz[channel];
      closure.computed_xyz[channel] =
          result.scale_value * unscaled[index][channel];
      const double residual_percent =
          100.0 * (closure.computed_xyz[channel] / recorded - 1.0);
      if (!std::isfinite(closure.computed_xyz[channel]) ||
          !std::isfinite(residual_percent)) {
        throw std::runtime_error(
            "spectro closure: fitted XYZ or residual is not representable");
      }
      closure.signed_relative_residual_percent[channel] = residual_percent;
      result.max_absolute_relative_residual_percent =
          std::max(result.max_absolute_relative_residual_percent,
                   std::fabs(residual_percent));
      relative_residual_norm =
          std::hypot(relative_residual_norm, residual_percent);
      ++residual_count;
    }
    result.readings.push_back(closure);
  }
  result.rms_relative_residual_percent =
      relative_residual_norm / std::sqrt(static_cast<double>(residual_count));
  return result;
}

} // namespace camera_iq
