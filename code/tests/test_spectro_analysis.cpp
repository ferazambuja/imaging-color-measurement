#include "camera_iq/spectro_analysis.hpp"

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

SpectroMeasurement reading(double scale) {
  SpectroMeasurement value;
  value.wavelength_nm = {380.0, 382.0, 384.0};
  value.spectral_radiance = {1.0 * scale, 2.0 * scale, 1.0 * scale};
  value.recorded_xyz = {10.0 * scale, 20.0 * scale, 30.0 * scale};
  return value;
}

void test_three_axes_respond_separately() {
  const auto scaled =
      camera_iq::analyze_spectro_group({reading(1.0), reading(2.0)});
  check_near(scaled.readings[0].spectral_integral, 8.0, 1e-12,
             "integral includes the 2 nm sample width");
  check_near(scaled.mean_spectral_integral, 12.0, 1e-12,
             "absolute level is summarized separately");
  check(scaled.coefficient_of_variation.has_value(),
        "repeated measurements carry level variation");
  if (scaled.coefficient_of_variation) {
    check_near(*scaled.coefficient_of_variation, std::sqrt(32.0) / 12.0,
               1e-12, "level CV uses the n-1 sample deviation");
  }
  check(scaled.max_shape_relative_l2.has_value() &&
            *scaled.max_shape_relative_l2 == 0.0,
        "pure dyadic scaling has exactly zero shape residual");
  check(scaled.max_pair_delta_u_prime_v_prime.has_value() &&
            *scaled.max_pair_delta_u_prime_v_prime == 0.0,
        "proportional XYZ has exactly zero chromaticity separation");

  SpectroMeasurement redistributed = reading(1.0);
  redistributed.spectral_radiance = {2.0, 1.0, 1.0};
  redistributed.recorded_xyz = {12.0, 20.0, 28.0};
  const auto changed =
      camera_iq::analyze_spectro_group({reading(1.0), redistributed});
  check(changed.coefficient_of_variation.has_value() &&
            *changed.coefficient_of_variation == 0.0,
        "fixed integral gives zero level variation");
  check(changed.max_shape_relative_l2.has_value() &&
            *changed.max_shape_relative_l2 > 0.0,
        "spectral redistribution moves normalized shape");
  check(changed.max_pair_delta_u_prime_v_prime.has_value() &&
            *changed.max_pair_delta_u_prime_v_prime > 0.0,
        "a deliberate XYZ change moves chromaticity");
}

void test_singleton_and_numeric_range() {
  const auto singleton = camera_iq::analyze_spectro_group({reading(1.0)});
  check(!singleton.sample_stddev_spectral_integral.has_value() &&
            !singleton.coefficient_of_variation.has_value() &&
            !singleton.sample_stddev_normalized_spectrum.has_value() &&
            !singleton.max_shape_relative_l2.has_value() &&
            !singleton.max_pair_delta_u_prime_v_prime.has_value(),
        "singleton variation fields are absent");

  SpectroMeasurement high_a = reading(1.0);
  high_a.wavelength_nm = {1.0, 2.0};
  high_a.spectral_radiance = {5.0e307, 5.0e307};
  SpectroMeasurement high_b = high_a;
  high_b.spectral_radiance = {2.5e307, 2.5e307};
  const auto high = camera_iq::analyze_spectro_group({high_a, high_b});
  check(high.sample_stddev_spectral_integral.has_value() &&
            std::isfinite(*high.sample_stddev_spectral_integral),
        "representable high-level spread remains finite");
  check_near(*high.sample_stddev_spectral_integral / 1.0e307,
             2.5 * std::sqrt(2.0), 1e-12,
             "high-level spread avoids squared overflow");

  SpectroMeasurement low = reading(1.0);
  low.wavelength_nm = {1.0, 2.0};
  low.spectral_radiance = {std::numeric_limits<double>::denorm_min(), 0.0};
  const auto tiny = camera_iq::analyze_spectro_group({low, low});
  check(tiny.mean_spectral_integral ==
            std::numeric_limits<double>::denorm_min(),
        "positive subnormal level is retained");
  check(tiny.coefficient_of_variation.has_value() &&
            *tiny.coefficient_of_variation == 0.0,
        "identical subnormal levels have finite zero variation");

  SpectroMeasurement tiny_shape = reading(1.0);
  tiny_shape.wavelength_nm = {1.0, 2.0};
  tiny_shape.spectral_radiance = {
      std::numeric_limits<double>::denorm_min(), 1.0};
  const auto shape =
      camera_iq::analyze_spectro_group({tiny_shape, tiny_shape});
  check(shape.mean_normalized_spectrum[0] ==
            std::numeric_limits<double>::denorm_min(),
        "representable subnormal normalized sample is retained");

  SpectroMeasurement largest = reading(1.0);
  largest.recorded_xyz = {std::numeric_limits<double>::max(),
                          std::numeric_limits<double>::max(),
                          std::numeric_limits<double>::max()};
  const auto chromaticity =
      camera_iq::analyze_spectro_group({largest})
          .readings[0]
          .recorded_xyz_chromaticity;
  check_near(chromaticity.x, 1.0 / 3.0, 1e-15,
             "chromaticity avoids XYZ-sum overflow");
  check_near(chromaticity.u_prime, 4.0 / 19.0, 1e-15,
             "u-prime avoids denominator overflow");
  check_near(chromaticity.v_prime, 9.0 / 19.0, 1e-15,
             "v-prime avoids denominator overflow");
}

void test_analysis_refusals() {
  check(throws([] {
          SpectroMeasurement cancellation = reading(1.0);
          cancellation.wavelength_nm = {1.0, 2.0, 3.0};
          cancellation.spectral_radiance = {1.0e308, -1.0e308, 1.0e-308};
          (void)camera_iq::analyze_spectro_group({cancellation});
        }),
        "non-representable normalization is refused");
  check(throws([] {
          auto invalid = reading(1.0);
          invalid.recorded_xyz = {0.0, 0.0, 0.0};
          (void)camera_iq::analyze_spectro_group({invalid});
        }),
        "XYZ that cannot form chromaticity is refused");
}

}  // namespace

int main() {
  test_three_axes_respond_separately();
  test_singleton_and_numeric_range();
  test_analysis_refusals();
  if (failures != 0) {
    std::cerr << failures << " spectro-analysis test(s) failed\n";
    return EXIT_FAILURE;
  }
  std::cout << "spectro-analysis tests passed\n";
  return EXIT_SUCCESS;
}
