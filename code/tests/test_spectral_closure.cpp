#include "camera_iq/spectral_closure.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

namespace {

using camera_iq::SpectralClosureInputs;

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

// Two wavelengths and two patches. The unscaled predictions are
// p1=(1,0,1), p2=(0,1,1); measured RGB is exactly ten times that prediction.
SpectralClosureInputs exact_synthetic(double scale = 10.0) {
  SpectralClosureInputs in;
  in.grid_nm = {500, 510};
  in.ssf = {std::vector<double>{1, 0}, std::vector<double>{0, 1},
            std::vector<double>{1, 1}};
  in.illuminant = {1, 1};
  in.patch_ids = {"p1", "p2"};
  in.reflectance = {{1, 0}, {0, 1}};
  in.measured_rgb = {{scale, 0, scale}, {0, scale, scale}};
  in.white_rgb = {scale, scale, 2 * scale};
  return in;
}

void test_exact_global_scale() {
  const auto result =
      camera_iq::compute_spectral_closure(exact_synthetic());
  check(result.white_card_gate_passes,
        "a compatible white-card pairing passes before closure");
  check_near(result.white_card_max_ratio_error, 0.0, 1e-9,
             "the exact white-card ratio has no error");
  check_near(result.global_scale_k, 10.0, 1e-9,
             "one global exposure scale is recovered");
  check(result.patches.size() == 2, "every supplied patch is reported");
  check_near(result.r.relative_rms, 0.0, 1e-9,
             "the exact R prediction has zero residual");
  check_near(result.g.relative_rms, 0.0, 1e-9,
             "the exact G prediction has zero residual");
  check_near(result.b.relative_rms, 0.0, 1e-9,
             "the exact B prediction has zero residual");
  check_near(result.r.correlation, 1.0, 1e-9,
             "the varying exact channel has unit correlation");
}

void test_white_gate_precedes_fit() {
  auto input = exact_synthetic();
  input.white_rgb = {50.0, 10.0, 20.0};
  const auto result = camera_iq::compute_spectral_closure(input);
  check(!result.white_card_gate_passes,
        "an incompatible white-card ratio fails the pairing gate");
  check(result.patches.empty(),
        "no chart closure is reported after a failed white-card gate");
  check(result.conclusion.find("unresolved") != std::string::npos,
        "a failed pairing is reported as unresolved");
}

void test_one_scale_cannot_hide_channel_imbalance() {
  auto input = exact_synthetic();
  input.measured_rgb = {{20, 0, 10}, {0, 10, 10}};
  const auto result = camera_iq::compute_spectral_closure(input);
  check(result.r.relative_rms > 1e-6,
        "a single global scale leaves a channel imbalance visible");
  check_near(result.r.scale_k_diagnostic, 20.0, 1e-9,
             "the R-only diagnostic recovers its own scale");
  check_near(result.g.scale_k_diagnostic, 10.0, 1e-9,
             "the G-only diagnostic recovers its own scale");
  check(result.global_scale_k > 10.0 && result.global_scale_k < 20.0,
        "the fitted global scale does not silently become a per-channel fit");
}

void test_refusals() {
  check(throws([] {
          auto input = exact_synthetic();
          input.reflectance.pop_back();
          (void)camera_iq::compute_spectral_closure(input);
        }),
        "mismatched patch and reflectance counts are refused");
  check(throws([] {
          auto input = exact_synthetic();
          input.ssf[0].pop_back();
          (void)camera_iq::compute_spectral_closure(input);
        }),
        "an SSF axis that does not match the grid is refused");
  check(throws([] {
          auto input = exact_synthetic();
          input.white_rgb[1] = 0.0;
          (void)camera_iq::compute_spectral_closure(input);
        }),
        "a non-positive measured white green channel is refused");
  check(throws([] {
          auto input = exact_synthetic();
          input.grid_nm = {500, 511, 530};
          for (auto& channel : input.ssf) channel.push_back(0.0);
          input.illuminant.push_back(1.0);
          for (auto& reflectance : input.reflectance) {
            reflectance.push_back(0.0);
          }
          (void)camera_iq::compute_spectral_closure(input);
        }),
        "a nonuniform grid is refused because closure uses equal weights");
  check(throws([] {
          auto input = exact_synthetic();
          input.measured_rgb[0][0] =
              std::numeric_limits<double>::quiet_NaN();
          (void)camera_iq::compute_spectral_closure(input);
        }),
        "a non-finite measured RGB value is refused");
  check(throws([] {
          auto input = exact_synthetic();
          for (auto& channel : input.ssf) {
            for (double& value : channel) value = 0.0;
          }
          input.ssf[1][0] = 1.0;
          input.white_rgb = {0.0, 10.0, 0.0};
          for (auto& reflectance : input.reflectance) {
            for (double& value : reflectance) value = 0.0;
          }
          (void)camera_iq::compute_spectral_closure(input);
        }),
        "an undefined global scale is refused instead of reported as zero");
  check(throws([] {
          auto input = exact_synthetic();
          input.ssf[0][0] = std::numeric_limits<double>::max();
          input.illuminant[0] = 2.0;
          (void)camera_iq::compute_spectral_closure(input);
        }),
        "a non-representable spectral prediction is refused");
}

}  // namespace

int main() {
  test_exact_global_scale();
  test_white_gate_precedes_fit();
  test_one_scale_cannot_hide_channel_imbalance();
  test_refusals();
  if (failures != 0) {
    std::cerr << failures << " spectral-closure test(s) failed\n";
    return EXIT_FAILURE;
  }
  std::cout << "spectral-closure tests passed\n";
  return EXIT_SUCCESS;
}
