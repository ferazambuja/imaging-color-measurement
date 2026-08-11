#include "camera_iq/spectral_compare.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace {

using camera_iq::SampledSpectrum;
using camera_iq::SpectralComparisonOptions;
using camera_iq::SpectralOffsetSeries;

int failures = 0;

void check(bool condition, const std::string& what) {
  if (!condition) {
    std::cerr << "FAIL: " << what << "\n";
    ++failures;
  }
}

void check_near(double actual, double expected, double tolerance,
                const std::string& what) {
  if (!(std::abs(actual - expected) <= tolerance)) {
    std::cerr << "FAIL: " << what << " (expected " << expected << " +/- "
              << tolerance << ", got " << actual << ")\n";
    ++failures;
  }
}

template <typename Operation>
bool throws(Operation operation) {
  try {
    operation();
  } catch (const std::exception&) {
    return true;
  }
  return false;
}

void test_common_grid_and_directional_residual() {
  const std::vector<SampledSpectrum> reference = {
      {{0.0, 1.0, 2.0}, {1.0, 2.0, 1.0}}};
  const std::vector<SampledSpectrum> candidate = {
      {{0.0, 2.0}, {2.0, 2.0}}};
  SpectralComparisonOptions options;
  options.common_wavelength_nm = {0.0, 1.0, 2.0};
  options.excluded_wavelength_nm = {1.0};

  const auto result =
      camera_iq::compare_spectral_groups(reference, candidate, options);
  check(result.normalization == "common_grid_equal_weight_integral" &&
            result.interpolation == "linear" &&
            result.relative_l2_denominator == "reference_l2_norm",
        "normalization, interpolation, and residual direction are explicit");
  check(result.reference_on_common_grid ==
            std::vector<double>({0.25, 0.5, 0.25}),
        "reference mean is normalized after resampling");
  check(std::all_of(result.candidate_on_common_grid.begin(),
                    result.candidate_on_common_grid.end(), [](double value) {
                      return std::abs(value - 1.0 / 3.0) < 1e-12;
                    }),
        "candidate mean is normalized on the same grid");
  check_near(result.directional_relative_l2, 1.0 / 3.0, 1e-12,
             "directional residual uses the reference L2 norm");
  check_near(result.bands[1].squared_residual_fraction, 2.0 / 3.0,
             1e-12, "per-band fraction localizes the residual");
  check(result.exclusion_results.size() == 1 &&
            result.exclusion_results[0].retained_sample_count == 2,
        "diagnostic exclusion is separate from the primary result");
}

void test_offset_sensitivity() {
  const std::vector<SampledSpectrum> centered = {
      {{0.0, 1.0, 2.0, 3.0, 4.0}, {1.0, 1.0, 3.0, 1.0, 1.0}}};
  const std::vector<SampledSpectrum> displaced = {
      {{0.0, 1.0, 2.0, 3.0, 4.0}, {1.0, 1.0, 1.0, 3.0, 1.0}}};
  SpectralComparisonOptions options;
  options.common_wavelength_nm = {0.0, 1.0, 2.0, 3.0, 4.0};
  options.offset_min_nm = -1.0;
  options.offset_max_nm = 1.0;
  options.offset_step_nm = 1.0;

  const auto result =
      camera_iq::compare_spectral_groups(centered, displaced, options);
  check(result.offset_sensitivity.size() == 3 &&
            result.offset_common_grid_sample_count == 3,
        "offset sweep uses one fixed supported interior");
  check(result.zero_offset_objective.has_value(),
        "offset sweep retains its zero-offset baseline");
  check_near(result.best_wavelength_offset_nm, -1.0, 1e-12,
             "candidate-axis sign convention is pinned");
  check_near(result.best_offset_directional_relative_l2, 0.0, 1e-12,
             "synthetic translated feature aligns at the known offset");

  options.offset_series = SpectralOffsetSeries::Reference;
  const auto reversed =
      camera_iq::compare_spectral_groups(centered, displaced, options);
  check_near(reversed.best_wavelength_offset_nm, 1.0, 1e-12,
             "changing the shifted series reverses the fitted sign");
}

void test_refusals() {
  const std::vector<SampledSpectrum> valid = {
      {{0.0, 1.0, 2.0}, {1.0, 2.0, 1.0}}};
  SpectralComparisonOptions singleton;
  singleton.common_wavelength_nm = {1.0};
  check(throws([&] {
          camera_iq::compare_spectral_groups(valid, valid, singleton);
        }),
        "singleton common grid is refused");

  SpectralComparisonOptions extrapolated;
  extrapolated.common_wavelength_nm = {-1.0, 0.0};
  check(throws([&] {
          camera_iq::compare_spectral_groups(valid, valid, extrapolated);
        }),
        "common-grid extrapolation is refused");

  SpectralComparisonOptions missing_exclusion;
  missing_exclusion.common_wavelength_nm = {0.0, 1.0, 2.0};
  missing_exclusion.excluded_wavelength_nm = {0.5};
  check(throws([&] {
          camera_iq::compare_spectral_groups(valid, valid, missing_exclusion);
        }),
        "excluded wavelength must exist on the common grid");
}

}  // namespace

int main() {
  test_common_grid_and_directional_residual();
  test_offset_sensitivity();
  test_refusals();
  if (failures != 0) {
    std::cerr << failures << " spectral-comparison test(s) failed\n";
    return EXIT_FAILURE;
  }
  std::cout << "spectral-comparison tests passed\n";
  return EXIT_SUCCESS;
}
