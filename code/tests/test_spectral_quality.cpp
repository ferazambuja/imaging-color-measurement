#include "camera_iq/spectral_quality.hpp"

#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

namespace {

using camera_iq::SpectralQualityInputs;

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

// Three unit-axis sensitivity channels on a five-sample grid. Two CMF targets
// lie in their span; the middle target lies on an unused axis.
SpectralQualityInputs synthetic() {
  SpectralQualityInputs in;
  in.grid_nm = {500, 510, 520, 530, 540};
  in.ssf = {std::vector<double>{1, 0, 0, 0, 0},
            std::vector<double>{0, 1, 0, 0, 0},
            std::vector<double>{0, 0, 1, 0, 0}};
  in.cmf = {std::vector<double>{1, 0, 0, 0, 0},
            std::vector<double>{0, 0, 0, 1, 0},
            std::vector<double>{1, 1, 0, 0, 0}};
  return in;
}

void test_subspace_residual() {
  const auto result = camera_iq::compute_spectral_quality(synthetic());
  check_near(result.cmf_residual[0], 0.0, 1e-9,
             "a CMF equal to an SSF channel fits exactly");
  check_near(result.cmf_residual[1], 1.0, 1e-9,
             "a target outside the sensitivity span has residual one");
  check_near(result.cmf_residual[2], 0.0, 1e-9,
             "a CMF in the SSF span fits exactly");
  check_near(result.combined_residual, std::sqrt(1.0 / 3.0), 1e-9,
             "the combined residual is the RMS of the three fits");
  check_near(result.quality_index, 1.0 - std::sqrt(1.0 / 3.0), 1e-9,
             "quality index is one minus the combined residual");
}

void test_normalized_subspace_scale_invariance() {
  const auto baseline = camera_iq::compute_spectral_quality(synthetic());

  auto common = synthetic();
  for (auto& channel : common.ssf) {
    for (double& value : channel) value *= 1e-8;
  }
  const auto common_result = camera_iq::compute_spectral_quality(common);
  for (std::size_t i = 0; i < 3; ++i) {
    check_near(common_result.cmf_residual[i], baseline.cmf_residual[i], 1e-12,
               "common SSF scaling preserves the normalized residual");
  }

  auto per_channel = synthetic();
  constexpr std::array<double, 3> scales{1e-6, 1e3, 7.0};
  for (std::size_t channel = 0; channel < 3; ++channel) {
    for (double& value : per_channel.ssf[channel]) value *= scales[channel];
  }
  const auto scaled = camera_iq::compute_spectral_quality(per_channel);
  check_near(scaled.combined_residual, baseline.combined_residual, 1e-12,
             "independent positive channel scaling preserves the normalized subspace metric");
}

void test_refusals() {
  check(throws([] {
          auto input = synthetic();
          input.ssf[1] = input.ssf[0];
          input.ssf[2] = input.ssf[0];
          (void)camera_iq::compute_spectral_quality(input);
        }),
        "a rank-deficient sensitivity basis is refused");
  check(throws([] {
          auto input = synthetic();
          input.cmf[0].pop_back();
          (void)camera_iq::compute_spectral_quality(input);
        }),
        "a CMF axis that does not match the grid is refused");
  check(throws([] {
          auto input = synthetic();
          input.ssf[1][2] = std::numeric_limits<double>::infinity();
          (void)camera_iq::compute_spectral_quality(input);
        }),
        "a non-finite sensitivity sample is refused");
  check(throws([] {
          auto input = synthetic();
          input.cmf[2][4] = std::numeric_limits<double>::quiet_NaN();
          (void)camera_iq::compute_spectral_quality(input);
        }),
        "a non-finite CMF sample is refused");
  check(throws([] {
          auto input = synthetic();
          input.cmf[0].assign(input.grid_nm.size(), 0.0);
          (void)camera_iq::compute_spectral_quality(input);
        }),
        "a zero-norm CMF target is refused");
  check(throws([] {
          auto input = synthetic();
          input.grid_nm[3] = input.grid_nm[2];
          (void)camera_iq::compute_spectral_quality(input);
        }),
        "a non-increasing wavelength grid is refused");
}

}  // namespace

int main() {
  test_subspace_residual();
  test_normalized_subspace_scale_invariance();
  test_refusals();
  if (failures != 0) {
    std::cerr << failures << " spectral-quality test(s) failed\n";
    return EXIT_FAILURE;
  }
  std::cout << "spectral-quality tests passed\n";
  return EXIT_SUCCESS;
}
