#include "camera_iq/sampled_spectrum.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

namespace {

using camera_iq::SampledSpectrum;

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

SampledSpectrum spectrum(double scale) {
  return SampledSpectrum{{380.0, 382.0, 384.0},
                         {1.0 * scale, 2.0 * scale, 1.0 * scale}};
}

void test_level_and_shape_separation() {
  const auto result =
      camera_iq::analyze_sampled_spectrum_group({spectrum(1.0), spectrum(2.0)});
  check(result.count == 2 && result.wavelength_step_nm == 2.0,
        "group size and native step are retained");
  check(result.sample_weighting == "uniform_equal_weight",
        "integration rule is explicit");
  check_near(result.readings[0].spectral_integral, 8.0, 1e-12,
             "integral includes the sample width");
  check_near(result.mean_spectral_integral, 12.0, 1e-12,
             "mean level is independent of normalized shape");
  check(result.coefficient_of_variation.has_value(),
        "repeated levels carry a sample CV");
  if (result.coefficient_of_variation) {
    check_near(*result.coefficient_of_variation, std::sqrt(32.0) / 12.0,
               1e-12, "CV uses the n-1 sample deviation");
  }
  check(result.mean_normalized_spectrum ==
            std::vector<double>({0.125, 0.25, 0.125}),
        "normalization precedes the group mean");
  check(result.max_shape_relative_l2.has_value() &&
            *result.max_shape_relative_l2 == 0.0,
        "a dyadic scale-only fixture has exactly zero shape residual");

  SampledSpectrum redistributed = spectrum(1.0);
  redistributed.values = {2.0, 1.0, 1.0};
  const auto changed = camera_iq::analyze_sampled_spectrum_group(
      {spectrum(1.0), redistributed});
  check(changed.coefficient_of_variation.has_value() &&
            *changed.coefficient_of_variation == 0.0,
        "equal integrals have zero level variation");
  check(changed.max_shape_relative_l2.has_value() &&
            *changed.max_shape_relative_l2 > 0.0,
        "redistribution at fixed integral changes normalized shape");
}

void test_singleton_and_numeric_range() {
  const auto singleton =
      camera_iq::analyze_sampled_spectrum_group({spectrum(1.0)});
  check(!singleton.sample_stddev_spectral_integral.has_value() &&
            !singleton.coefficient_of_variation.has_value() &&
            !singleton.sample_stddev_normalized_spectrum.has_value() &&
            !singleton.max_shape_relative_l2.has_value(),
        "singleton variation is absent rather than zero");

  SampledSpectrum negative{{1.0, 2.0, 3.0}, {-0.5, 2.0, 0.5}};
  const auto retained =
      camera_iq::analyze_sampled_spectrum_group({negative});
  check(retained.readings[0].normalized_spectrum[0] < 0.0,
        "a finite negative sample is retained when total integral is positive");

  SampledSpectrum subnormal{{1.0, 2.0},
                            {std::numeric_limits<double>::denorm_min(), 1.0}};
  const auto tiny = camera_iq::analyze_sampled_spectrum_group({subnormal});
  check(tiny.readings[0].normalized_spectrum[0] ==
            std::numeric_limits<double>::denorm_min(),
        "a representable subnormal normalized sample is retained");

  const double three_quarters_max =
      0.75 * std::numeric_limits<double>::max();
  SampledSpectrum cancellation{{1.0, 2.0, 3.0},
                               {three_quarters_max, three_quarters_max,
                                -three_quarters_max}};
  const auto wide =
      camera_iq::analyze_sampled_spectrum_group({cancellation});
  check(wide.readings[0].spectral_integral == three_quarters_max,
        "a representable integral survives intermediate overflow");
}

void test_refusals() {
  check(throws([] { (void)camera_iq::analyze_sampled_spectrum_group({}); }),
        "empty groups are refused");
  check(throws([] {
          SampledSpectrum invalid{{380.0}, {1.0}};
          (void)camera_iq::analyze_sampled_spectrum_group({invalid});
        }),
        "grids shorter than two samples are refused");
  check(throws([] {
          auto invalid = spectrum(1.0);
          invalid.wavelength_nm[2] = 385.0;
          (void)camera_iq::analyze_sampled_spectrum_group({invalid});
        }),
        "non-uniform grids are refused");
  check(throws([] {
          auto second = spectrum(1.0);
          second.wavelength_nm[1] = 381.0;
          (void)camera_iq::analyze_sampled_spectrum_group(
              {spectrum(1.0), second});
        }),
        "mismatched repeat axes are refused");
  check(throws([] {
          auto invalid = spectrum(1.0);
          invalid.values[1] = std::numeric_limits<double>::infinity();
          (void)camera_iq::analyze_sampled_spectrum_group({invalid});
        }),
        "non-finite samples are refused");
  check(throws([] {
          auto invalid = spectrum(1.0);
          invalid.values = {1.0, -2.0, 1.0};
          (void)camera_iq::analyze_sampled_spectrum_group({invalid});
        }),
        "non-positive normalization is refused");
}

}  // namespace

int main() {
  test_level_and_shape_separation();
  test_singleton_and_numeric_range();
  test_refusals();
  if (failures != 0) {
    std::cerr << failures << " sampled-spectrum test(s) failed\n";
    return EXIT_FAILURE;
  }
  std::cout << "sampled-spectrum tests passed\n";
  return EXIT_SUCCESS;
}
