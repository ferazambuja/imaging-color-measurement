#include "camera_iq/cam16_equation_audit.hpp"

#include <array>
#include <cmath>
#include <stdexcept>

namespace camera_iq {
namespace {

void require_j(double j) {
  if (!std::isfinite(j) || j < 0.0 || j > 100.0) {
    throw std::runtime_error("CAM16 equation audit: J must be within [0,100]");
  }
}

void require_relative_background(double y_background) {
  if (!std::isfinite(y_background) || y_background <= 0.0 ||
      y_background > 100.0) {
    throw std::runtime_error(
        "CAM16 equation audit: relative background must be within (0,100]");
  }
}

}  // namespace

double cam16_normalized_brightness(double j) {
  require_j(j);
  return std::sqrt(j / 100.0);
}

double hellwig_2022_normalized_brightness(double j) {
  require_j(j);
  return j / 100.0;
}

double cam16_isolated_ncb_chroma_factor(double y_background,
                                        double reference_y_background) {
  require_relative_background(y_background);
  require_relative_background(reference_y_background);
  const double result =
      std::pow(reference_y_background / y_background, 0.18);
  if (!std::isfinite(result)) {
    throw std::runtime_error(
        "CAM16 equation audit: isolated Ncb factor overflow");
  }
  return result;
}

double cam16_relative_chroma_fixed_adapted_response(
    double y_background, double reference_j,
    double reference_y_background) {
  require_relative_background(y_background);
  require_relative_background(reference_y_background);
  if (!std::isfinite(reference_j) || reference_j <= 0.0 ||
      reference_j > 100.0) {
    throw std::runtime_error(
        "CAM16 equation audit: reference J must be within (0,100]");
  }

  const double n = y_background / 100.0;
  const double reference_n = reference_y_background / 100.0;
  const double z = 1.48 + std::sqrt(n);
  const double reference_z = 1.48 + std::sqrt(reference_n);
  const double isolated =
      std::pow(reference_n / n, 0.18);
  const double chroma_scale = std::pow(
      (1.64 - std::pow(0.29, n)) /
          (1.64 - std::pow(0.29, reference_n)),
      0.73);
  const double lightness_scale = std::pow(
      reference_j / 100.0,
      (z - reference_z) / (2.0 * reference_z));
  const double result = isolated * chroma_scale * lightness_scale;
  if (!std::isfinite(result)) {
    throw std::runtime_error(
        "CAM16 equation audit: coupled chroma expression overflow");
  }
  return result;
}

double hellwig_2022_colorfulness(double n_c, double eccentricity,
                                 double opponent_a, double opponent_b) {
  if (!std::isfinite(n_c) || n_c < 0.0 || !std::isfinite(eccentricity) ||
      eccentricity < 0.0 || !std::isfinite(opponent_a) ||
      !std::isfinite(opponent_b)) {
    throw std::runtime_error(
        "Hellwig 2022 Equation 23: inputs must be finite and non-negative where required");
  }
  const double result =
      43.0 * n_c * eccentricity * std::hypot(opponent_a, opponent_b);
  if (!std::isfinite(result)) {
    throw std::runtime_error("Hellwig 2022 Equation 23: result overflow");
  }
  return result;
}

Cam16EquationAuditReport build_cam16_equation_audit() {
  Cam16EquationAuditReport report;
  report.brightness_curve.reserve(21);
  for (int j = 0; j <= 100; j += 5) {
    const double value = static_cast<double>(j);
    report.brightness_curve.push_back(
        {value, cam16_normalized_brightness(value),
         hellwig_2022_normalized_brightness(value)});
  }
  constexpr std::array<double, 8> backgrounds = {
      20.0, 10.0, 5.0, 2.0, 1.0, 0.5, 0.2, 0.1};
  report.background_curve.reserve(backgrounds.size());
  for (const double y_background : backgrounds) {
    report.background_curve.push_back(
        {y_background,
         cam16_isolated_ncb_chroma_factor(y_background)});
    for (int reference_j = 10; reference_j <= 90; reference_j += 10) {
      report.coupled_chroma_curve.push_back(
          {y_background, static_cast<double>(reference_j),
           cam16_relative_chroma_fixed_adapted_response(
               y_background, static_cast<double>(reference_j))});
    }
  }
  report.performance = {0.86, 0.95, 0.87, 0.96, 0.81, 0.71};
  return report;
}

}  // namespace camera_iq
