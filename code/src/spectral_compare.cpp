#include "camera_iq/spectral_compare.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <string_view>

namespace camera_iq {
namespace {

constexpr std::size_t kMaxSpectralSamples = 1'000'000;

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

void validate_axis(const std::vector<double>& axis, std::string_view name) {
  if (axis.size() < 2) {
    throw std::runtime_error("spectral compare: " + std::string(name) +
                             " requires at least two samples");
  }
  if (axis.size() > kMaxSpectralSamples) {
    throw std::runtime_error("spectral compare: " + std::string(name) +
                             " exceeds the sample-count limit");
  }
  for (std::size_t index = 0; index < axis.size(); ++index) {
    if (!std::isfinite(axis[index]) ||
        (index > 0 && axis[index] <= axis[index - 1])) {
      throw std::runtime_error("spectral compare: " + std::string(name) +
                               " must be finite and strictly increasing");
    }
  }
  if (axis.size() > 2) {
    const double step = axis[1] - axis[0];
    for (std::size_t index = 2; index < axis.size(); ++index) {
      if (std::fabs((axis[index] - axis[index - 1]) - step) > 1e-9) {
        throw std::runtime_error("spectral compare: " + std::string(name) +
                                 " must be uniform for normalization");
      }
    }
  }
}

double interpolate(const std::vector<double>& axis,
                   const std::vector<double>& values, double wavelength) {
  if (wavelength < axis.front() || wavelength > axis.back()) {
    throw std::runtime_error(
        "spectral compare: common wavelength is outside the retained range");
  }
  const auto upper = std::lower_bound(axis.begin(), axis.end(), wavelength);
  if (upper != axis.end() && *upper == wavelength) {
    return values[static_cast<std::size_t>(upper - axis.begin())];
  }
  if (upper == axis.begin() || upper == axis.end()) {
    throw std::runtime_error(
        "spectral compare: common wavelength is outside the retained range");
  }
  const std::size_t high = static_cast<std::size_t>(upper - axis.begin());
  const std::size_t low = high - 1;
  const double fraction =
      (wavelength - axis[low]) / (axis[high] - axis[low]);
  // The algebraically familiar low + f * (high - low) can overflow when
  // finite endpoints have opposite signs. Convex weighting keeps each
  // intermediate within the endpoint magnitudes.
  const double result =
      (1.0 - fraction) * values[low] + fraction * values[high];
  if (!std::isfinite(result)) {
    throw std::runtime_error(
        "spectral compare: interpolated value is not representable");
  }
  return result;
}

SpectralL2Objective l2_objective(
    const std::vector<double>& reference,
    const std::vector<double>& candidate,
    const std::vector<bool>* retained = nullptr) {
  double residual = 0.0;
  double reference_norm = 0.0;
  std::size_t count = 0;
  for (std::size_t index = 0; index < reference.size(); ++index) {
    if (retained != nullptr && !(*retained)[index]) {
      continue;
    }
    residual = std::hypot(residual, candidate[index] - reference[index]);
    reference_norm = std::hypot(reference_norm, reference[index]);
    ++count;
  }
  if (count == 0 || !std::isfinite(reference_norm) || reference_norm <= 0.0) {
    throw std::runtime_error(
        "spectral compare: reference L2 norm must be finite and positive");
  }
  const double directional_relative_l2 = residual / reference_norm;
  if (!std::isfinite(directional_relative_l2)) {
    throw std::runtime_error(
        "spectral compare: relative L2 result is not representable");
  }
  return {residual, reference_norm, directional_relative_l2};
}

std::vector<double> resample(const std::vector<double>& source_axis,
                             const std::vector<double>& source_values,
                             const std::vector<double>& target_axis,
                             double source_offset_nm = 0.0) {
  std::vector<double> result;
  result.reserve(target_axis.size());
  for (const double wavelength : target_axis) {
    // If candidate nominal wavelength + offset is the actual wavelength, the
    // nominal coordinate corresponding to a target is target - offset.
    result.push_back(interpolate(source_axis, source_values,
                                 wavelength - source_offset_nm));
  }
  return result;
}

void normalize_on_grid(std::vector<double>& values,
                       const std::vector<double>& axis) {
  if (values.size() != axis.size()) {
    throw std::runtime_error(
        "spectral compare: common-grid values and wavelength counts differ");
  }
  const double step = axis.size() == 1 ? 1.0 : axis[1] - axis[0];
  double max_magnitude = 0.0;
  for (const double value : values) {
    if (!std::isfinite(value)) {
      throw std::runtime_error(
          "spectral compare: common-grid values must be finite");
    }
    max_magnitude = std::max(max_magnitude, std::fabs(value));
  }
  int value_exponent = 0;
  if (max_magnitude > 0.0) {
    (void)std::frexp(max_magnitude, &value_exponent);
  }
  CompensatedSum scaled_sum;
  for (const double value : values) {
    scaled_sum.add(std::ldexp(value, -value_exponent));
  }
  if (!std::isfinite(scaled_sum.value()) || scaled_sum.value() <= 0.0) {
    throw std::runtime_error(
        "spectral compare: common-grid integral must be finite and positive");
  }

  int step_exponent = 0;
  const double step_fraction = std::frexp(step, &step_exponent);
  int denominator_exponent = 0;
  const double denominator = scaled_sum.value() * step_fraction;
  if (!std::isfinite(denominator) || denominator <= 0.0) {
    throw std::runtime_error(
        "spectral compare: common-grid integral is not representable");
  }
  const double denominator_fraction =
      std::frexp(denominator, &denominator_exponent);
  const int normalization_exponent =
      -(step_exponent + denominator_exponent);
  for (double& value : values) {
    const double scaled_value = std::ldexp(value, -value_exponent);
    value = std::ldexp(scaled_value / denominator_fraction,
                       normalization_exponent);
    if (!std::isfinite(value)) {
      throw std::runtime_error(
          "spectral compare: common-grid normalized value is not representable");
    }
  }
}

std::vector<SpectralComparisonBand> comparison_bands(
    const std::vector<double>& axis, const std::vector<double>& reference,
    const std::vector<double>& candidate) {
  double residual_norm = 0.0;
  std::vector<SpectralComparisonBand> result;
  result.reserve(axis.size());
  for (std::size_t index = 0; index < axis.size(); ++index) {
    const double residual = candidate[index] - reference[index];
    if (!std::isfinite(residual)) {
      throw std::runtime_error(
          "spectral compare: signed residual is not representable");
    }
    residual_norm = std::hypot(residual_norm, residual);
    result.push_back({axis[index], residual, 0.0});
  }
  CompensatedSum fraction_sum;
  for (auto& band : result) {
    if (residual_norm == 0.0) {
      band.squared_residual_fraction = 0.0;
      continue;
    }
    const double ratio = std::fabs(band.signed_residual) / residual_norm;
    band.squared_residual_fraction = ratio * ratio;
    fraction_sum.add(band.squared_residual_fraction);
  }
  if (residual_norm > 0.0) {
    const double total = fraction_sum.value();
    if (!std::isfinite(total) || total <= 0.0) {
      throw std::runtime_error(
          "spectral compare: residual contributions are not representable");
    }
    for (auto& band : result) {
      band.squared_residual_fraction /= total;
    }
  }
  return result;
}

}  // namespace

SpectralComparison compare_spectral_groups(
    const std::vector<SampledSpectrum>& reference,
    const std::vector<SampledSpectrum>& candidate,
    const SpectralComparisonOptions& options) {
  validate_axis(options.common_wavelength_nm, "common wavelength grid");
  SpectralComparison result;
  result.reference_group = analyze_sampled_spectrum_group(reference);
  result.candidate_group = analyze_sampled_spectrum_group(candidate);
  if (options.offset_series == SpectralOffsetSeries::Reference) {
    result.offset_convention =
        "reference_nominal_wavelength_plus_offset_is_actual_wavelength";
  }
  result.common_wavelength_nm = options.common_wavelength_nm;
  result.reference_on_common_grid = resample(
      reference.front().wavelength_nm,
      result.reference_group.mean_normalized_spectrum,
      options.common_wavelength_nm);
  result.candidate_on_common_grid = resample(
      candidate.front().wavelength_nm,
      result.candidate_group.mean_normalized_spectrum,
      options.common_wavelength_nm);
  normalize_on_grid(result.reference_on_common_grid,
                    options.common_wavelength_nm);
  normalize_on_grid(result.candidate_on_common_grid,
                    options.common_wavelength_nm);
  result.directional_relative_l2 =
      l2_objective(result.reference_on_common_grid,
                   result.candidate_on_common_grid)
          .directional_relative_l2;

  result.bands = comparison_bands(options.common_wavelength_nm,
                                  result.reference_on_common_grid,
                                  result.candidate_on_common_grid);

  if (!options.excluded_wavelength_nm.empty()) {
    std::vector<bool> retained(options.common_wavelength_nm.size(), true);
    for (const double excluded : options.excluded_wavelength_nm) {
      const auto found = std::lower_bound(options.common_wavelength_nm.begin(),
                                          options.common_wavelength_nm.end(),
                                          excluded);
      if (found == options.common_wavelength_nm.end() || *found != excluded) {
        throw std::runtime_error(
            "spectral compare: excluded wavelength is not on the common grid");
      }
      retained[static_cast<std::size_t>(found -
                                        options.common_wavelength_nm.begin())] =
          false;
    }
    const std::size_t retained_count = static_cast<std::size_t>(
        std::count(retained.begin(), retained.end(), true));
    result.exclusion_results.push_back(SpectralExclusionResult{
        options.excluded_wavelength_nm, retained_count,
        l2_objective(result.reference_on_common_grid,
                     result.candidate_on_common_grid, &retained)
            .directional_relative_l2});
  }

  if (options.offset_step_nm != 0.0) {
    if (!std::isfinite(options.offset_min_nm) ||
        !std::isfinite(options.offset_max_nm) ||
        !std::isfinite(options.offset_step_nm) || options.offset_step_nm <= 0.0 ||
        options.offset_max_nm < options.offset_min_nm) {
      throw std::runtime_error("spectral compare: invalid offset sweep");
    }
    std::vector<double> sweep_grid;
    const auto& shifted_axis =
        options.offset_series == SpectralOffsetSeries::Reference
            ? reference.front().wavelength_nm
            : candidate.front().wavelength_nm;
    for (const double wavelength : options.common_wavelength_nm) {
      if (wavelength - options.offset_max_nm >= shifted_axis.front() &&
          wavelength - options.offset_min_nm <= shifted_axis.back()) {
        sweep_grid.push_back(wavelength);
      }
    }
    if (sweep_grid.size() < 2) {
      throw std::runtime_error(
          "spectral compare: offset sweep requires at least two common supported samples");
    }
    result.offset_common_grid_sample_count = sweep_grid.size();
    auto zero_reference = resample(
        reference.front().wavelength_nm,
        result.reference_group.mean_normalized_spectrum, sweep_grid);
    auto zero_candidate = resample(
        candidate.front().wavelength_nm,
        result.candidate_group.mean_normalized_spectrum, sweep_grid);
    normalize_on_grid(zero_reference, sweep_grid);
    normalize_on_grid(zero_candidate, sweep_grid);
    result.zero_offset_objective = l2_objective(zero_reference, zero_candidate);
    result.best_offset_directional_relative_l2 =
        std::numeric_limits<double>::infinity();
    const double raw_interval_count =
        (options.offset_max_nm - options.offset_min_nm) /
        options.offset_step_nm;
    const double rounded_interval_count = std::round(raw_interval_count);
    if (!std::isfinite(raw_interval_count) ||
        !std::isfinite(rounded_interval_count) ||
        rounded_interval_count < 0.0 ||
        rounded_interval_count > static_cast<double>(kMaxSpectralSamples) ||
        std::fabs(raw_interval_count - rounded_interval_count) > 1e-9) {
      throw std::runtime_error(
          "spectral compare: offset range must be finite, bounded, and an integer number of steps");
    }
    const std::size_t interval_count =
        static_cast<std::size_t>(rounded_interval_count);
    for (std::size_t offset_index = 0; offset_index <= interval_count;
         ++offset_index) {
      double offset =
          options.offset_min_nm +
          static_cast<double>(offset_index) * options.offset_step_nm;
      if (offset_index == interval_count) offset = options.offset_max_nm;
      if (std::fabs(offset) < options.offset_step_nm * 1e-12) offset = 0.0;
      auto reference_sweep = resample(
          reference.front().wavelength_nm,
          result.reference_group.mean_normalized_spectrum, sweep_grid,
          options.offset_series == SpectralOffsetSeries::Reference ? offset
                                                                    : 0.0);
      auto candidate_sweep = resample(
          candidate.front().wavelength_nm,
          result.candidate_group.mean_normalized_spectrum, sweep_grid,
          options.offset_series == SpectralOffsetSeries::Candidate ? offset
                                                                    : 0.0);
      normalize_on_grid(reference_sweep, sweep_grid);
      normalize_on_grid(candidate_sweep, sweep_grid);
      const auto objective = l2_objective(reference_sweep, candidate_sweep);
      result.offset_sensitivity.push_back({offset, objective});
      if (objective.directional_relative_l2 <
          result.best_offset_directional_relative_l2) {
        result.best_offset_directional_relative_l2 =
            objective.directional_relative_l2;
        result.best_wavelength_offset_nm = offset;
      }
    }
    auto best_reference = resample(
        reference.front().wavelength_nm,
        result.reference_group.mean_normalized_spectrum, sweep_grid,
        options.offset_series == SpectralOffsetSeries::Reference
            ? result.best_wavelength_offset_nm
            : 0.0);
    auto best_candidate = resample(
        candidate.front().wavelength_nm,
        result.candidate_group.mean_normalized_spectrum, sweep_grid,
        options.offset_series == SpectralOffsetSeries::Candidate
            ? result.best_wavelength_offset_nm
            : 0.0);
    normalize_on_grid(best_reference, sweep_grid);
    normalize_on_grid(best_candidate, sweep_grid);
    result.best_offset_bands =
        comparison_bands(sweep_grid, best_reference, best_candidate);
  }
  return result;
}

}  // namespace camera_iq
