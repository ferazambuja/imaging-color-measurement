#include "camera_iq/spectro_colorimetry.hpp"

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
  value.wavelength_nm = {1.0, 2.0};
  value.spectral_radiance = {2.0 * scale, 3.0 * scale};
  value.recorded_xyz = {20.0 * scale, 30.0 * scale, 50.0 * scale};
  return value;
}

camera_iq::SpectroCmfTable observer() {
  return camera_iq::read_spectro_cmf_csv(
      "Wavelength (nm),X,Y,Z\n1,1,0,1\n2,0,1,1\n");
}

void test_global_scale_and_residual() {
  const auto closure = camera_iq::compute_spectro_closure(
      {reading(1.0), reading(2.0)}, observer());
  check(closure.sample_weighting == "uniform_equal_weight" &&
            closure.scale_source == "derived_from_recorded_xyz",
        "weighting and scale source are explicit");
  check_near(closure.scale_value, 10.0, 1e-12,
             "one global proportional scale is recovered");
  check(closure.readings.size() == 2 &&
            closure.readings[0].computed_xyz ==
                std::array<double, 3>({20.0, 30.0, 50.0}),
        "equal-weight XYZ includes the sample width");
  check_near(closure.max_absolute_relative_residual_percent, 0.0, 1e-12,
             "an exact relationship closes");

  SpectroMeasurement inconsistent = reading(1.0);
  inconsistent.recorded_xyz[1] += 1.0;
  const auto residual =
      camera_iq::compute_spectro_closure({inconsistent}, observer());
  check(residual.max_absolute_relative_residual_percent > 0.0,
        "one global scale cannot hide a channel-specific disagreement");
}

void test_numeric_range() {
  const auto high =
      camera_iq::compute_spectro_closure({reading(1.0e200)}, observer());
  check(std::isfinite(high.scale_value) &&
            std::fabs(high.scale_value - 10.0) <=
                4.0 * std::numeric_limits<double>::epsilon() * 10.0,
        "representable scale fit survives squared-product overflow");
  check(std::isfinite(high.max_absolute_relative_residual_percent),
        "high-range residuals remain finite");

  const auto flat_observer = camera_iq::read_spectro_cmf_csv(
      "Wavelength (nm),X,Y,Z\n1,1,1,1\n2,1,1,1\n3,1,1,1\n");
  SpectroMeasurement cancellation = reading(1.0);
  cancellation.wavelength_nm = {1.0, 2.0, 3.0};
  const double three_quarters_max =
      0.75 * std::numeric_limits<double>::max();
  cancellation.spectral_radiance = {
      three_quarters_max, three_quarters_max, -three_quarters_max};
  cancellation.recorded_xyz = {
      three_quarters_max, three_quarters_max, three_quarters_max};
  const auto result =
      camera_iq::compute_spectro_closure({cancellation}, flat_observer);
  check(result.readings[0].computed_xyz == cancellation.recorded_xyz &&
            result.max_absolute_relative_residual_percent == 0.0,
        "representable tristimulus survives intermediate overflow");
}

void test_parser_and_closure_refusals() {
  check(throws([] {
          (void)camera_iq::read_spectro_cmf_csv("bad header\n1,1,0,0\n");
        }),
        "unexpected CMF header is refused");
  check(throws([] {
          (void)camera_iq::read_spectro_cmf_csv(
              "Wavelength (nm),X,Y,Z\n1,1,0,0\n1,0,1,0\n");
        }),
        "non-increasing observer grid is refused");
  check(throws([] {
          (void)camera_iq::read_spectro_cmf_csv(
              "Wavelength (nm),X,Y,Z\n1,1,0\n2,0,1,1\n");
        }),
        "malformed observer row is refused");
  check(throws([] {
          (void)camera_iq::read_spectro_cmf_csv(
              "Wavelength (nm),X,Y,Z\n1,1,no,0\n2,0,1,1\n");
        }),
        "non-numeric observer value is refused");
  check(throws([] {
          (void)camera_iq::compute_spectro_closure({}, observer());
        }),
        "empty closure input is refused");
  check(throws([] {
          auto shifted = reading(1.0);
          shifted.wavelength_nm[1] = 3.0;
          (void)camera_iq::compute_spectro_closure({shifted}, observer());
        }),
        "observer and measurement axis mismatch is refused");
  check(throws([] {
          auto invalid = reading(1.0);
          invalid.recorded_xyz[0] = 0.0;
          (void)camera_iq::compute_spectro_closure({invalid}, observer());
        }),
        "zero recorded XYZ is refused because relative residual is undefined");
  check(throws([] {
          auto invalid = reading(1.0);
          invalid.spectral_radiance[0] =
              std::numeric_limits<double>::quiet_NaN();
          (void)camera_iq::compute_spectro_closure({invalid}, observer());
        }),
        "non-finite spectral input is refused");

  const auto crlf = camera_iq::read_spectro_cmf_csv(
      "Wavelength (nm),X,Y,Z\r\n1,1,0,1\r\n2,0,1,1\r\n");
  check(crlf.wavelength_nm.size() == 2,
        "CRLF observer tables are accepted");
}

}  // namespace

int main() {
  test_global_scale_and_residual();
  test_numeric_range();
  test_parser_and_closure_refusals();
  if (failures != 0) {
    std::cerr << failures << " spectro-colorimetry test(s) failed\n";
    return EXIT_FAILURE;
  }
  std::cout << "spectro-colorimetry tests passed\n";
  return EXIT_SUCCESS;
}
