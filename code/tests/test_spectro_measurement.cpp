#include "camera_iq/spectro_measurement.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

namespace {

using camera_iq::SpectroMeasurement;

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

SpectroMeasurement measurement(double radiance_offset,
                               double xyz_offset = 0.0) {
  SpectroMeasurement result;
  result.wavelength_nm = {380.0, 382.0, 384.0};
  result.spectral_radiance = {1.0 + radiance_offset,
                              -0.5 + radiance_offset,
                              3.0 + radiance_offset};
  result.recorded_xyz = {10.0 + xyz_offset, 20.0 + xyz_offset,
                         30.0 + xyz_offset};
  return result;
}

void test_pointwise_summary_and_singleton() {
  const auto repeats = camera_iq::summarize_spectro_group(
      {measurement(0.0), measurement(2.0, 4.0)});
  check(repeats.count == 2, "reading count is explicit");
  check_near(repeats.mean_spectral_radiance[0], 2.0, 1e-12,
             "radiance mean is pointwise");
  check(repeats.sample_stddev_spectral_radiance.has_value(),
        "spread is present for two readings");
  if (repeats.sample_stddev_spectral_radiance) {
    check_near(repeats.sample_stddev_spectral_radiance->at(0), std::sqrt(2.0),
               1e-12, "radiance spread uses n-1");
  }
  check_near(repeats.mean_recorded_xyz[1], 22.0, 1e-12,
             "recorded XYZ mean is channelwise");
  check(repeats.sample_stddev_recorded_xyz.has_value(),
        "XYZ spread is present for repeats");
  if (repeats.sample_stddev_recorded_xyz) {
    check_near(repeats.sample_stddev_recorded_xyz->at(1), std::sqrt(8.0),
               1e-12, "recorded XYZ spread uses n-1");
  }

  const auto singleton =
      camera_iq::summarize_spectro_group({measurement(0.0)});
  check(!singleton.sample_stddev_spectral_radiance.has_value() &&
            !singleton.sample_stddev_recorded_xyz.has_value(),
        "one reading does not fabricate repeatability");
}

void test_numeric_range_and_permutation() {
  SpectroMeasurement high_a = measurement(0.0);
  SpectroMeasurement high_b = high_a;
  high_a.spectral_radiance[0] = 1.0e16;
  high_b.spectral_radiance[0] =
      std::nextafter(1.0e16, std::numeric_limits<double>::infinity());
  const auto high = camera_iq::summarize_spectro_group({high_a, high_b});
  const double exact_spread =
      (high_b.spectral_radiance[0] - high_a.spectral_radiance[0]) /
      std::sqrt(2.0);
  check_near(high.sample_stddev_spectral_radiance->at(0), exact_spread, 1e-12,
             "variance remains accurate under a high offset");

  SpectroMeasurement wide_low = measurement(0.0);
  SpectroMeasurement wide_high = measurement(0.0);
  wide_low.spectral_radiance[0] = -1.0e308;
  wide_high.spectral_radiance[0] = 1.0e308;
  const auto wide =
      camera_iq::summarize_spectro_group({wide_low, wide_high});
  const double expected = std::sqrt(2.0) * 1.0e308;
  check(std::isfinite(wide.mean_spectral_radiance[0]) &&
            std::isfinite(wide.sample_stddev_spectral_radiance->at(0)),
        "representable high-range mean and spread remain finite");
  check(std::fabs(wide.sample_stddev_spectral_radiance->at(0) - expected) /
                expected <=
            4.0 * std::numeric_limits<double>::epsilon(),
        "the high-range spread remains accurate");

  SpectroMeasurement maximum = measurement(0.0);
  SpectroMeasurement minimum = measurement(0.0);
  SpectroMeasurement unit = measurement(0.0);
  maximum.spectral_radiance[0] = std::numeric_limits<double>::max();
  minimum.spectral_radiance[0] = -std::numeric_limits<double>::max();
  unit.spectral_radiance[0] = 1.0;
  const auto a = camera_iq::summarize_spectro_group({maximum, minimum, unit});
  const auto b = camera_iq::summarize_spectro_group({unit, maximum, minimum});
  check_near(a.mean_spectral_radiance[0], 1.0 / 3.0, 1e-15,
             "cancellation preserves a representable mean");
  check_near(b.mean_spectral_radiance[0], 1.0 / 3.0, 1e-15,
             "the cancellation mean is permutation-invariant");
  check(throws([&] {
          (void)camera_iq::summarize_spectro_group({minimum, maximum});
        }),
        "an unrepresentable spread is refused");
}

void test_refusals() {
  check(throws([] { (void)camera_iq::summarize_spectro_group({}); }),
        "empty groups are refused");
  check(throws([] {
          auto shifted = measurement(1.0);
          shifted.wavelength_nm[1] = 383.0;
          (void)camera_iq::summarize_spectro_group(
              {measurement(0.0), shifted});
        }),
        "different wavelength axes are refused");
  check(throws([] {
          auto invalid = measurement(0.0);
          invalid.spectral_radiance.pop_back();
          (void)camera_iq::summarize_spectro_group({invalid});
        }),
        "radiance length mismatch is refused");
  check(throws([] {
          auto invalid = measurement(0.0);
          invalid.recorded_xyz[0] = std::numeric_limits<double>::quiet_NaN();
          (void)camera_iq::summarize_spectro_group({invalid});
        }),
        "non-finite samples are refused");
}

}  // namespace

int main() {
  test_pointwise_summary_and_singleton();
  test_numeric_range_and_permutation();
  test_refusals();
  if (failures != 0) {
    std::cerr << failures << " spectro-measurement test(s) failed\n";
    return EXIT_FAILURE;
  }
  std::cout << "spectro-measurement tests passed\n";
  return EXIT_SUCCESS;
}
