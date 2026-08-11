#include "camera_iq/cam16_equation_audit.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <string>

namespace {

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

void test_brightness_relations() {
  check_near(camera_iq::cam16_normalized_brightness(25.0), 0.5, 1e-15,
             "CAM16 reaches half normalized brightness at J=25");
  check_near(camera_iq::cam16_normalized_brightness(50.0),
             std::sqrt(0.5), 1e-15,
             "CAM16 square-root relation at J=50");
  check_near(camera_iq::hellwig_2022_normalized_brightness(25.0), 0.25,
             0.0, "proposed linear relation at J=25");
  check_near(camera_iq::hellwig_2022_normalized_brightness(50.0), 0.5,
             0.0, "proposed relation reaches half brightness at J=50");
}

void test_background_terms_and_coupling() {
  check_near(camera_iq::cam16_isolated_ncb_chroma_factor(20.0), 1.0,
             0.0, "reference background normalizes to one");
  check_near(camera_iq::cam16_isolated_ncb_chroma_factor(5.0),
             1.2834258975629043, 1e-14, "isolated factor at Yb=5");
  check_near(camera_iq::cam16_isolated_ncb_chroma_factor(1.0),
             1.7146891477615709, 1e-14, "isolated factor at Yb=1");
  check_near(camera_iq::cam16_isolated_ncb_chroma_factor(0.1),
             2.595287047166021, 1e-14, "isolated factor at Yb=0.1");

  const double low_j =
      camera_iq::cam16_relative_chroma_fixed_adapted_response(0.1, 10.0);
  const double high_j =
      camera_iq::cam16_relative_chroma_fixed_adapted_response(0.1, 90.0);
  check_near(low_j, 2.6865933941337503, 1e-14,
             "coupled expression at Yb=0.1 and J=10");
  check_near(high_j, 2.1198928552563943, 1e-14,
             "coupled expression at Yb=0.1 and J=90");
  const double isolated =
      camera_iq::cam16_isolated_ncb_chroma_factor(0.1);
  check(high_j < isolated && isolated < low_j,
        "isolated term is neither bound on the declared coupled sweep");
}

void test_corrected_coefficient_and_report_contract() {
  check_near(camera_iq::hellwig_2022_colorfulness(1.0, 1.0, 3.0, 4.0),
             215.0, 0.0, "corrected coefficient 43 with 3-4-5 vector");

  const auto report = camera_iq::build_cam16_equation_audit();
  check(report.brightness_curve.size() == 21,
        "brightness curve has J=0 through 100 in steps of five");
  check(report.background_curve.size() == 8,
        "isolated curve has eight declared backgrounds");
  check(report.coupled_chroma_curve.size() == 72,
        "coupled curve has eight backgrounds by nine lightness values");
  check_near(report.performance.brightness_cam16_r_squared, 0.86, 0.0,
             "published CAM16 brightness R-squared retained");
  check_near(report.performance.brightness_proposed_r_squared, 0.95, 0.0,
             "published proposed brightness R-squared retained");
  check_near(report.performance.chroma_cam16_r_squared, 0.87, 0.0,
             "published CAM16 chroma R-squared retained");
  check_near(report.performance.chroma_proposed_r_squared, 0.96, 0.0,
             "published proposed chroma R-squared retained");
  check_near(report.performance.colorfulness_cam16_r_squared, 0.81, 0.0,
             "published CAM16 colorfulness R-squared retained");
  check_near(report.performance.colorfulness_proposed_r_squared, 0.71, 0.0,
             "published colorfulness regression retained");
}

void test_refusals() {
  const double nan = std::numeric_limits<double>::quiet_NaN();
  check(throws([] { camera_iq::cam16_normalized_brightness(-1.0); }),
        "negative J is refused");
  check(throws([] { camera_iq::cam16_normalized_brightness(101.0); }),
        "J above 100 is refused");
  check(throws([&] { camera_iq::cam16_normalized_brightness(nan); }),
        "non-finite J is refused");
  check(throws([] { camera_iq::cam16_isolated_ncb_chroma_factor(0.0); }),
        "zero background is refused");
  check(throws([] {
          camera_iq::cam16_relative_chroma_fixed_adapted_response(1.0, 0.0);
        }),
        "zero reference lightness is refused for the coupled expression");
  check(throws([] {
          camera_iq::hellwig_2022_colorfulness(-1.0, 1.0, 3.0, 4.0);
        }),
        "negative adaptation factor is refused");
}

}  // namespace

int main() {
  test_brightness_relations();
  test_background_terms_and_coupling();
  test_corrected_coefficient_and_report_contract();
  test_refusals();
  if (failures != 0) {
    std::cerr << failures << " CAM16 equation-audit test(s) failed\n";
    return EXIT_FAILURE;
  }
  std::cout << "CAM16 equation-audit tests passed\n";
  return EXIT_SUCCESS;
}
