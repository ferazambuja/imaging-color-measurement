#include "camera_iq/spectral_smi.hpp"

#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

namespace {

using camera_iq::SpectralSmiInputs;

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

double gaussian(double value, double mean, double sigma) {
  const double distance = (value - mean) / sigma;
  return std::exp(-0.5 * distance * distance);
}

SpectralSmiInputs base_inputs() {
  SpectralSmiInputs in;
  for (int wavelength = 380; wavelength <= 730; wavelength += 10) {
    in.grid_nm.push_back(wavelength);
  }
  const std::size_t count = in.grid_nm.size();
  in.illuminant.resize(count);
  for (std::size_t i = 0; i < count; ++i) {
    const double wavelength = in.grid_nm[i];
    in.illuminant[i] = 80.0 + 0.05 * (wavelength - 380.0);
    in.cmf[0].push_back(gaussian(wavelength, 600, 40) +
                        0.35 * gaussian(wavelength, 445, 20));
    in.cmf[1].push_back(gaussian(wavelength, 555, 45));
    in.cmf[2].push_back(1.7 * gaussian(wavelength, 445, 22));
  }

  const std::array<std::array<double, 3>, 6> shapes{{
      {600, 60, 0.8}, {450, 50, 0.7}, {550, 40, 0.9},
      {500, 120, 0.5}, {680, 55, 0.85}, {520, 200, 0.6},
  }};
  for (const auto& shape : shapes) {
    std::vector<double> reflectance(count);
    for (std::size_t i = 0; i < count; ++i) {
      reflectance[i] =
          0.05 + shape[2] * gaussian(in.grid_nm[i], shape[0], shape[1]);
    }
    in.reflectance.push_back(std::move(reflectance));
  }
  return in;
}

void test_luther_satisfying_camera() {
  auto input = base_inputs();
  input.ssf = input.cmf;
  const auto result = camera_iq::compute_spectral_smi(input);
  check(result.patch_count == 6, "all synthetic test colors are evaluated");
  check_near(result.mean_delta_e_76, 0.0, 0.05,
             "a Luther-satisfying camera has near-zero mean dE76");
  check_near(result.smi, 100.0, 0.5,
             "a Luther-satisfying camera scores near 100");
  check_near(result.white_preserving_mean_delta_e_76, 0.0, 0.05,
             "the white-preserving Luther fit remains near exact");
  check_near(result.white_preserving_white_delta_e_76, 0.0, 1e-9,
             "the constrained fit maps camera white to reference white");
}

void test_shifted_sensitivity_moves_the_metric() {
  auto input = base_inputs();
  for (double wavelength : input.grid_nm) {
    input.ssf[0].push_back(gaussian(wavelength, 625, 40) +
                              0.35 * gaussian(wavelength, 470, 20));
    input.ssf[1].push_back(gaussian(wavelength, 580, 45));
    input.ssf[2].push_back(1.7 * gaussian(wavelength, 470, 22));
  }
  const auto result = camera_iq::compute_spectral_smi(input);
  check(result.mean_delta_e_76 > 0.1,
        "a shifted sensitivity basis produces non-trivial color error");
  check(result.smi < 100.0,
        "a metameric mismatch moves the SMI below its ideal value");
}

void test_score_definition_and_white_constraint() {
  auto input = base_inputs();
  input.ssf = input.cmf;
  for (std::size_t i = 0; i < input.grid_nm.size(); ++i) {
    input.ssf[2][i] = 1.0;
  }
  input.smi_slope = 5.5;
  const auto result = camera_iq::compute_spectral_smi(input);
  check_near(result.smi, 100.0 - 5.5 * result.mean_delta_e_76, 1e-9,
             "SMI is exactly 100 minus 5.5 times mean dE76");
  check_near(result.white_preserving_smi,
             100.0 - 5.5 * result.white_preserving_mean_delta_e_76, 1e-9,
             "the white-preserving score uses the same declared slope");
  check_near(result.white_preserving_white_delta_e_76, 0.0, 1e-9,
             "the constrained non-Luther fit still preserves white");
  check(std::abs(result.white_preserving_delta_smi) > 1e-6,
        "the constrained sensitivity check is not a duplicate calculation");
}

void test_refusals() {
  check(throws([] {
          auto input = base_inputs();
          input.ssf = input.cmf;
          input.ssf[0].pop_back();
          (void)camera_iq::compute_spectral_smi(input);
        }),
        "an SSF axis that does not match the grid is refused");
  check(throws([] {
          auto input = base_inputs();
          input.ssf = input.cmf;
          input.reflectance.resize(2);
          (void)camera_iq::compute_spectral_smi(input);
        }),
        "fewer than three test colors are refused");
  check(throws([] {
          auto input = base_inputs();
          input.ssf = input.cmf;
          input.grid_nm[2] = input.grid_nm[1];
          (void)camera_iq::compute_spectral_smi(input);
        }),
        "a non-increasing wavelength grid is refused");
  check(throws([] {
          auto input = base_inputs();
          input.ssf = input.cmf;
          input.illuminant[0] =
              std::numeric_limits<double>::quiet_NaN();
          (void)camera_iq::compute_spectral_smi(input);
        }),
        "a non-finite spectral sample is refused");
  check(throws([] {
          auto input = base_inputs();
          input.ssf = input.cmf;
          input.smi_slope = 0.0;
          (void)camera_iq::compute_spectral_smi(input);
        }),
        "a non-positive score slope is refused");
  check(throws([] {
          auto input = base_inputs();
          input.ssf = input.cmf;
          input.grid_nm.front() = -std::numeric_limits<double>::max();
          input.grid_nm[1] = std::numeric_limits<double>::max();
          (void)camera_iq::compute_spectral_smi(input);
        }),
        "a non-representable wavelength step is refused");
}

}  // namespace

int main() {
  test_luther_satisfying_camera();
  test_shifted_sensitivity_moves_the_metric();
  test_score_definition_and_white_constraint();
  test_refusals();
  if (failures != 0) {
    std::cerr << failures << " spectral-SMI test(s) failed\n";
    return EXIT_FAILURE;
  }
  std::cout << "spectral-SMI tests passed\n";
  return EXIT_SUCCESS;
}
