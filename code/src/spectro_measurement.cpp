#include "camera_iq/spectro_measurement.hpp"

#include <algorithm>
#include <cmath>
#include <functional>
#include <stdexcept>
#include <string>
#include <utility>

namespace camera_iq {
namespace {

struct ShiftedStats {
  double mean = 0.0;
  double sample_stddev = 0.0;  // zero for a singleton; the caller drops it
};

// Neumaier summation retains a small addend when the running total is much
// larger. It uses only double operations, so correctness does not depend on
// whether a platform gives long double additional range or precision.
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

// Measurement-group readings often differ far less than they differ from zero.
// Variance therefore runs on offsets from the first reading, keeping the
// accumulation at the scale of the spread. The mean is accumulated separately
// from scaled values so a small, representable residual survives cancellation
// between large positive and negative readings. Selecting the scalar through a
// callable keeps one implementation for radiance bins and XYZ channels.
template <typename Select>
ShiftedStats shifted_stats(const std::vector<SpectroMeasurement>& readings,
                           Select select) {
  // Power-of-two scaling prevents intermediate overflow without relying on
  // optional long-double headroom. On Apple arm64, for example, long double has
  // the same range and precision as double. frexp supplies the exact exponent
  // shift that places every nonzero value within [-1, 1].
  double max_magnitude = 0.0;
  for (const auto& reading : readings) {
    max_magnitude = std::max(max_magnitude, std::fabs(select(reading)));
  }
  int exponent = 0;
  if (max_magnitude > 0.0) {
    (void)std::frexp(max_magnitude, &exponent);
  }
  const auto scaled = [&](const SpectroMeasurement& reading) {
    return std::ldexp(select(reading), -exponent);
  };

  const double divisor = static_cast<double>(readings.size());
  CompensatedSum mean_sum;
  for (const auto& reading : readings) {
    // Divide each bounded term before summation so even a very large group
    // cannot overflow a partial sum merely because similarly signed values
    // appear together.
    mean_sum.add(scaled(reading) / divisor);
  }

  ShiftedStats stats;
  stats.mean = std::ldexp(mean_sum.value(), exponent);
  if (!std::isfinite(stats.mean)) {
    throw std::runtime_error(
        "spectro group summary: mean is not representable");
  }
  if (readings.size() < 2) return stats;

  const double origin = scaled(readings.front());
  CompensatedSum mean_offset_sum;
  for (const auto& reading : readings) {
    mean_offset_sum.add((scaled(reading) - origin) / divisor);
  }
  const double mean_offset = mean_offset_sum.value();

  CompensatedSum variance_sum;
  const double variance_divisor =
      static_cast<double>(readings.size() - 1);
  for (const auto& reading : readings) {
    const double difference = (scaled(reading) - origin) - mean_offset;
    variance_sum.add((difference * difference) / variance_divisor);
  }
  stats.sample_stddev =
      std::ldexp(std::sqrt(variance_sum.value()), exponent);
  if (!std::isfinite(stats.sample_stddev)) {
    throw std::runtime_error(
        "spectro group summary: sample standard deviation is not "
        "representable");
  }
  return stats;
}

}  // namespace

SpectroGroupSummary summarize_spectro_group(
    const std::vector<SpectroMeasurement>& readings) {
  if (readings.empty()) {
    throw std::runtime_error("spectro group summary: group is empty");
  }

  const auto& axis = readings.front().wavelength_nm;
  if (axis.size() < 2 ||
      !std::all_of(axis.begin(), axis.end(),
                   [](double value) { return std::isfinite(value); }) ||
      std::adjacent_find(axis.begin(), axis.end(),
                         std::greater_equal<double>{}) != axis.end()) {
    throw std::runtime_error(
        "spectro group summary: wavelength axis must be finite and strictly "
        "increasing");
  }
  for (std::size_t reading_index = 0; reading_index < readings.size();
       ++reading_index) {
    const auto& reading = readings[reading_index];
    if (reading.wavelength_nm != axis) {
      throw std::runtime_error(
          "spectro group summary: wavelength axis mismatch at reading " +
          std::to_string(reading_index));
    }
    if (reading.spectral_radiance.size() != axis.size()) {
      throw std::runtime_error(
          "spectro group summary: radiance length mismatch at reading " +
          std::to_string(reading_index));
    }
    if (!std::all_of(reading.spectral_radiance.begin(),
                     reading.spectral_radiance.end(),
                     [](double value) { return std::isfinite(value); }) ||
        !std::all_of(reading.recorded_xyz.begin(), reading.recorded_xyz.end(),
                     [](double value) { return std::isfinite(value); })) {
      throw std::runtime_error(
          "spectro group summary: non-finite sample at reading " +
          std::to_string(reading_index));
    }
  }

  SpectroGroupSummary result;
  result.count = readings.size();
  result.wavelength_nm = axis;
  result.mean_spectral_radiance.resize(axis.size());
  std::vector<double> radiance_stddev(axis.size(), 0.0);
  for (std::size_t i = 0; i < axis.size(); ++i) {
    const ShiftedStats stats = shifted_stats(
        readings,
        [i](const SpectroMeasurement& r) { return r.spectral_radiance[i]; });
    result.mean_spectral_radiance[i] = stats.mean;
    radiance_stddev[i] = stats.sample_stddev;
  }

  std::array<double, 3> xyz_stddev{};
  for (std::size_t channel = 0; channel < result.mean_recorded_xyz.size();
       ++channel) {
    const ShiftedStats stats = shifted_stats(
        readings,
        [channel](const SpectroMeasurement& r) {
          return r.recorded_xyz[channel];
        });
    result.mean_recorded_xyz[channel] = stats.mean;
    xyz_stddev[channel] = stats.sample_stddev;
  }

  // A singleton group has a mean but no spread. Reporting zero would read as
  // perfect repeatability rather than as an absent measurement.
  if (readings.size() > 1) {
    result.sample_stddev_spectral_radiance = std::move(radiance_stddev);
    result.sample_stddev_recorded_xyz = xyz_stddev;
  }
  return result;
}

}  // namespace camera_iq
