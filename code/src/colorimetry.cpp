#include "camera_iq/colorimetry.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <vector>

namespace camera_iq {
namespace {

double lab_f(double t) {
  constexpr double epsilon = 216.0 / 24389.0;
  constexpr double kappa = 24389.0 / 27.0;
  if (t > epsilon) return std::cbrt(t);
  return (kappa * t + 16.0) / 116.0;
}

double lab_f_inverse(double t) {
  constexpr double delta = 6.0 / 29.0;
  if (t > delta) return t * t * t;
  return 3.0 * delta * delta * (t - 4.0 / 29.0);
}

double delta_e_76_impl(const Lab& a, const Lab& b) {
  const std::array<double, 6> components = {a.l, a.a, a.b, b.l, b.a, b.b};
  if (!std::all_of(components.begin(), components.end(),
                   [](double value) { return std::isfinite(value); })) {
    throw std::runtime_error("dE76: Lab values must be finite");
  }
  const double dl = a.l - b.l;
  const double da = a.a - b.a;
  const double db = a.b - b.b;
  const double result = std::hypot(dl, da, db);
  if (!std::isfinite(result)) {
    throw std::runtime_error("dE76: distance is not representable");
  }
  return result;
}

double degrees_to_radians(double degrees) {
  return degrees * 3.141592653589793238462643383279502884 / 180.0;
}

double radians_to_degrees(double radians) {
  return radians * 180.0 / 3.141592653589793238462643383279502884;
}

double square(double value) { return value * value; }

double seventh_power(double value) {
  const double squared = value * value;
  return squared * squared * squared * value;
}

double hue_degrees(double a, double b) {
  if (a == 0.0 && b == 0.0) return 0.0;
  double hue = radians_to_degrees(std::atan2(b, a));
  if (hue < 0.0) hue += 360.0;
  return hue;
}

struct DeltaAccumulator {
  std::size_t count = 0;
  double sum_76 = 0;
  double sumsq_76 = 0;
  double max_76 = 0;
  double sum_2000 = 0;
  double sumsq_2000 = 0;
  double max_2000 = 0;
};

void add_delta(DeltaAccumulator& acc, const Lab& target_lab,
               const Lab& predicted_lab) {
  const double de76 = delta_e_76_impl(target_lab, predicted_lab);
  const double de2000 = delta_e_2000(target_lab, predicted_lab);
  ++acc.count;
  acc.sum_76 += de76;
  acc.sumsq_76 += de76 * de76;
  acc.max_76 = std::max(acc.max_76, de76);
  acc.sum_2000 += de2000;
  acc.sumsq_2000 += de2000 * de2000;
  acc.max_2000 = std::max(acc.max_2000, de2000);
}

std::array<double, 3> solve_3x3(std::array<std::array<double, 3>, 3> a,
                                std::array<double, 3> b) {
  for (std::size_t col = 0; col < 3; ++col) {
    std::size_t pivot = col;
    for (std::size_t row = col + 1; row < 3; ++row) {
      if (std::abs(a[row][col]) > std::abs(a[pivot][col])) pivot = row;
    }
    if (std::abs(a[pivot][col]) < 1e-12) {
      throw std::runtime_error("ccm fit: singular camera design matrix");
    }
    if (pivot != col) {
      std::swap(a[pivot], a[col]);
      std::swap(b[pivot], b[col]);
    }
    const double denom = a[col][col];
    for (std::size_t j = col; j < 3; ++j) a[col][j] /= denom;
    b[col] /= denom;
    for (std::size_t row = 0; row < 3; ++row) {
      if (row == col) continue;
      const double factor = a[row][col];
      for (std::size_t j = col; j < 3; ++j) {
        a[row][j] -= factor * a[col][j];
      }
      b[row] -= factor * b[col];
    }
  }
  return b;
}

std::array<double, 3> rgb_array(const CameraRgbPatch& p) {
  return {p.r, p.g, p.b};
}

CcmFit fit_matrix_only(const std::vector<CameraRgbPatch>& camera_rgb,
                       const std::vector<Xyz>& target_xyz) {
  std::array<std::array<double, 3>, 3> normal{};
  std::array<std::array<double, 3>, 3> rhs{};
  for (std::size_t i = 0; i < camera_rgb.size(); ++i) {
    const auto rgb = rgb_array(camera_rgb[i]);
    const std::array<double, 3> xyz = {
        target_xyz[i].x, target_xyz[i].y, target_xyz[i].z};
    for (std::size_t row = 0; row < 3; ++row) {
      for (std::size_t col = 0; col < 3; ++col) {
        normal[row][col] += rgb[row] * rgb[col];
      }
      for (std::size_t channel = 0; channel < 3; ++channel) {
        rhs[channel][row] += xyz[channel] * rgb[row];
      }
    }
  }

  CcmFit fit;
  for (std::size_t channel = 0; channel < 3; ++channel) {
    fit.matrix[channel] = solve_3x3(normal, rhs[channel]);
  }
  fit.patch_count = camera_rgb.size();
  return fit;
}

DeltaAccumulator evaluate_matrix(
    const std::array<std::array<double, 3>, 3>& matrix,
    const std::vector<CameraRgbPatch>& camera_rgb,
    const std::vector<Xyz>& target_xyz, const Xyz& white_xyz) {
  DeltaAccumulator acc;
  for (std::size_t i = 0; i < camera_rgb.size(); ++i) {
    const Lab target_lab = xyz_to_lab(target_xyz[i], white_xyz);
    const Lab predicted_lab =
        xyz_to_lab(apply_ccm(matrix, camera_rgb[i]), white_xyz);
    add_delta(acc, target_lab, predicted_lab);
  }
  return acc;
}

CcmEvaluation evaluation_from_accumulator(const DeltaAccumulator& acc) {
  CcmEvaluation out;
  out.patch_count = acc.count;
  if (acc.count == 0) return out;
  out.mean_delta_e_76 = acc.sum_76 / static_cast<double>(acc.count);
  out.rms_delta_e_76 =
      std::sqrt(acc.sumsq_76 / static_cast<double>(acc.count));
  out.max_delta_e_76 = acc.max_76;
  out.mean_delta_e_2000 = acc.sum_2000 / static_cast<double>(acc.count);
  out.rms_delta_e_2000 =
      std::sqrt(acc.sumsq_2000 / static_cast<double>(acc.count));
  out.max_delta_e_2000 = acc.max_2000;
  return out;
}

}  // namespace

double delta_e_76(const Lab& first, const Lab& second) {
  return delta_e_76_impl(first, second);
}

Lab xyz_to_lab(const Xyz& xyz, const Xyz& white) {
  if (!std::isfinite(xyz.x) || !std::isfinite(xyz.y) ||
      !std::isfinite(xyz.z)) {
    throw std::runtime_error("Lab: XYZ values must be finite");
  }
  if (!std::isfinite(white.x) || !std::isfinite(white.y) ||
      !std::isfinite(white.z) || white.x <= 0 || white.y <= 0 ||
      white.z <= 0) {
    throw std::runtime_error("Lab: white XYZ must be finite and positive");
  }
  const double fx = lab_f(xyz.x / white.x);
  const double fy = lab_f(xyz.y / white.y);
  const double fz = lab_f(xyz.z / white.z);
  Lab out;
  out.l = 116.0 * fy - 16.0;
  out.a = 500.0 * (fx - fy);
  out.b = 200.0 * (fy - fz);
  return out;
}

Xyz lab_to_xyz(const Lab& lab, const Xyz& white) {
  if (!std::isfinite(lab.l) || !std::isfinite(lab.a) ||
      !std::isfinite(lab.b)) {
    throw std::runtime_error("Lab inverse: Lab values must be finite");
  }
  if (!std::isfinite(white.x) || !std::isfinite(white.y) ||
      !std::isfinite(white.z) || white.x <= 0 || white.y <= 0 ||
      white.z <= 0) {
    throw std::runtime_error("Lab inverse: white XYZ must be finite and positive");
  }

  const double fy = (lab.l + 16.0) / 116.0;
  const double fx = fy + lab.a / 500.0;
  const double fz = fy - lab.b / 200.0;
  return {white.x * lab_f_inverse(fx), white.y * lab_f_inverse(fy),
          white.z * lab_f_inverse(fz)};
}

Ipt xyz_d65_to_ipt(const Xyz& xyz) {
  if (!std::isfinite(xyz.x) || !std::isfinite(xyz.y) ||
      !std::isfinite(xyz.z)) {
    throw std::runtime_error("IPT: XYZ values must be finite");
  }

  const std::array<double, 3> lms = {
      0.4002 * xyz.x + 0.7075 * xyz.y - 0.0807 * xyz.z,
      -0.2280 * xyz.x + 1.1500 * xyz.y + 0.0612 * xyz.z,
      0.9184 * xyz.z,
  };
  if (!std::isfinite(lms[0]) || !std::isfinite(lms[1]) ||
      !std::isfinite(lms[2])) {
    throw std::runtime_error("IPT: LMS transform overflow");
  }
  std::array<double, 3> response{};
  for (std::size_t i = 0; i < response.size(); ++i) {
    response[i] = std::copysign(std::pow(std::abs(lms[i]), 0.43), lms[i]);
  }
  const Ipt result{
      0.4000 * response[0] + 0.4000 * response[1] +
          0.2000 * response[2],
      4.4550 * response[0] - 4.8510 * response[1] +
          0.3960 * response[2],
      0.8056 * response[0] + 0.3572 * response[1] -
          1.1628 * response[2],
  };
  if (!std::isfinite(result.i) || !std::isfinite(result.p) ||
      !std::isfinite(result.t)) {
    throw std::runtime_error("IPT: output overflow");
  }
  return result;
}

Oklab xyz_d65_to_oklab(const Xyz& xyz) {
  if (!std::isfinite(xyz.x) || !std::isfinite(xyz.y) ||
      !std::isfinite(xyz.z)) {
    throw std::runtime_error("OkLab: XYZ must be finite");
  }

  // W3C CSS Color 4 Candidate Recommendation Draft 2026-07-28,
  // sample conversion code with matrices recalculated for a consistent D65
  // reference white and 64-bit precision.
  const std::array<double, 3> lms = {
      0.8190224379967030 * xyz.x + 0.3619062600528904 * xyz.y -
          0.1288737815209879 * xyz.z,
      0.0329836539323885 * xyz.x + 0.9292868615863434 * xyz.y +
          0.0361446663506424 * xyz.z,
      0.0481771893596242 * xyz.x + 0.2642395317527308 * xyz.y +
          0.6335478284694309 * xyz.z,
  };
  if (!std::isfinite(lms[0]) || !std::isfinite(lms[1]) ||
      !std::isfinite(lms[2])) {
    throw std::runtime_error("OkLab: XYZ to LMS overflow");
  }
  const std::array<double, 3> nonlinear = {
      std::cbrt(lms[0]), std::cbrt(lms[1]), std::cbrt(lms[2])};
  const Oklab result{
      0.2104542683093140 * nonlinear[0] +
          0.7936177747023054 * nonlinear[1] -
          0.0040720430116193 * nonlinear[2],
      1.9779985324311684 * nonlinear[0] -
          2.4285922420485799 * nonlinear[1] +
          0.4505937096174110 * nonlinear[2],
      0.0259040424655478 * nonlinear[0] +
          0.7827717124575296 * nonlinear[1] -
          0.8086757549230774 * nonlinear[2],
  };
  if (!std::isfinite(result.l) || !std::isfinite(result.a) ||
      !std::isfinite(result.b)) {
    throw std::runtime_error("OkLab: output overflow");
  }
  return result;
}

Xyz oklab_to_xyz_d65(const Oklab& oklab) {
  if (!std::isfinite(oklab.l) || !std::isfinite(oklab.a) ||
      !std::isfinite(oklab.b)) {
    throw std::runtime_error("OkLab inverse: components must be finite");
  }
  const std::array<double, 3> lms_nonlinear = {
      oklab.l + 0.3963377773761749 * oklab.a +
          0.2158037573099136 * oklab.b,
      oklab.l - 0.1055613458156586 * oklab.a -
          0.0638541728258133 * oklab.b,
      oklab.l - 0.0894841775298119 * oklab.a -
          1.2914855480194092 * oklab.b,
  };
  const std::array<double, 3> lms = {
      lms_nonlinear[0] * lms_nonlinear[0] * lms_nonlinear[0],
      lms_nonlinear[1] * lms_nonlinear[1] * lms_nonlinear[1],
      lms_nonlinear[2] * lms_nonlinear[2] * lms_nonlinear[2],
  };
  const Xyz result{
      1.2268798758459243 * lms[0] - 0.5578149944602171 * lms[1] +
          0.2813910456659647 * lms[2],
      -0.0405757452148008 * lms[0] + 1.1122868032803170 * lms[1] -
          0.0717110580655164 * lms[2],
      -0.0763729366746601 * lms[0] - 0.4214933324022432 * lms[1] +
          1.5869240198367816 * lms[2],
  };
  if (!std::isfinite(result.x) || !std::isfinite(result.y) ||
      !std::isfinite(result.z)) {
    throw std::runtime_error("OkLab inverse: output overflow");
  }
  return result;
}

Oklch oklab_to_oklch(const Oklab& oklab) {
  if (!std::isfinite(oklab.l) || !std::isfinite(oklab.a) ||
      !std::isfinite(oklab.b)) {
    throw std::runtime_error("OkLCh: OkLab components must be finite");
  }
  constexpr double powerless_chroma = 0.000004;
  const double chroma = std::hypot(oklab.a, oklab.b);
  if (!std::isfinite(chroma)) {
    throw std::runtime_error("OkLCh: chroma overflow");
  }
  if (chroma <= powerless_chroma) {
    return {oklab.l, chroma, 0.0, false};
  }
  double hue = radians_to_degrees(std::atan2(oklab.b, oklab.a));
  if (hue < 0.0) hue += 360.0;
  return {oklab.l, chroma, hue, true};
}

Oklab oklch_to_oklab(const Oklch& oklch) {
  if (!std::isfinite(oklch.l) || !std::isfinite(oklch.c) ||
      oklch.c < 0.0) {
    throw std::runtime_error(
        "OkLCh inverse: L and non-negative chroma must be finite");
  }
  // CSS Color 4 treats a missing polar hue as powerless during conversion:
  // the Cartesian opponent components are both zero.  The forward transform
  // marks hue missing through C <= 4e-6, so this also makes the public pair
  // composable with a bounded, explicitly tested near-neutral chroma loss.
  if (!oklch.hue_defined || oklch.c == 0.0) {
    return {oklch.l, 0.0, 0.0};
  }
  if (!std::isfinite(oklch.h_degrees)) {
    throw std::runtime_error(
        "OkLCh inverse: positive chroma requires a finite hue");
  }
  const double hue = degrees_to_radians(oklch.h_degrees);
  const Oklab result{oklch.l, oklch.c * std::cos(hue),
                     oklch.c * std::sin(hue)};
  if (!std::isfinite(result.a) || !std::isfinite(result.b)) {
    throw std::runtime_error("OkLCh inverse: output overflow");
  }
  return result;
}

double delta_e_ok(const Oklab& first, const Oklab& second) {
  if (!std::isfinite(first.l) || !std::isfinite(first.a) ||
      !std::isfinite(first.b) || !std::isfinite(second.l) ||
      !std::isfinite(second.a) || !std::isfinite(second.b)) {
    throw std::runtime_error("deltaEOK: components must be finite");
  }
  return std::hypot(first.l - second.l, first.a - second.a,
                    first.b - second.b);
}

namespace {

double delta_e_94_with_weight_chroma(const Lab& first, const Lab& second,
                                     Cie94Application application,
                                     double weight_chroma) {
  if (!std::isfinite(first.l) || !std::isfinite(first.a) ||
      !std::isfinite(first.b) || !std::isfinite(second.l) ||
      !std::isfinite(second.a) || !std::isfinite(second.b)) {
    throw std::runtime_error("CIE94: Lab components must be finite");
  }

  double k_l = 1.0;
  double k_1 = 0.045;
  double k_2 = 0.015;
  if (application == Cie94Application::Textiles) {
    k_l = 2.0;
    k_1 = 0.048;
    k_2 = 0.014;
  }

  const double first_chroma = std::hypot(first.a, first.b);
  const double second_chroma = std::hypot(second.a, second.b);
  if (!std::isfinite(first_chroma) || !std::isfinite(second_chroma) ||
      !std::isfinite(weight_chroma) || weight_chroma < 0.0) {
    throw std::runtime_error("CIE94: chroma overflow");
  }
  const double delta_l = first.l - second.l;
  const double delta_c = first_chroma - second_chroma;
  const double delta_ab =
      std::hypot(first.a - second.a, first.b - second.b);
  if (!std::isfinite(delta_l) || !std::isfinite(delta_c) ||
      !std::isfinite(delta_ab)) {
    throw std::runtime_error("CIE94: component difference overflow");
  }
  const double chroma_ratio =
      delta_ab == 0.0 ? 0.0 : std::clamp(std::abs(delta_c) / delta_ab, 0.0, 1.0);
  const double delta_h =
      delta_ab * std::sqrt(std::max(0.0, 1.0 - chroma_ratio * chroma_ratio));
  const double s_c = 1.0 + k_1 * weight_chroma;
  const double s_h = 1.0 + k_2 * weight_chroma;
  const double result =
      std::hypot(delta_l / k_l, delta_c / s_c, delta_h / s_h);
  if (!std::isfinite(result)) {
    throw std::runtime_error("CIE94: result overflow");
  }
  return result;
}

}  // namespace

double delta_e_94(const Lab& reference, const Lab& sample,
                  Cie94Application application) {
  const double reference_chroma = std::hypot(reference.a, reference.b);
  return delta_e_94_with_weight_chroma(reference, sample, application,
                                       reference_chroma);
}

double delta_e_94_geometric_mean_chroma(const Lab& first, const Lab& second,
                                        Cie94Application application) {
  const double first_chroma = std::hypot(first.a, first.b);
  const double second_chroma = std::hypot(second.a, second.b);
  const double weight_chroma =
      std::sqrt(first_chroma) * std::sqrt(second_chroma);
  return delta_e_94_with_weight_chroma(first, second, application,
                                       weight_chroma);
}

double delta_e_2000(const Lab& a, const Lab& b) {
  const double c1 = std::sqrt(a.a * a.a + a.b * a.b);
  const double c2 = std::sqrt(b.a * b.a + b.b * b.b);
  const double c_bar = 0.5 * (c1 + c2);
  const double c_bar7 = seventh_power(c_bar);
  const double g =
      0.5 * (1.0 - std::sqrt(c_bar7 / (c_bar7 + seventh_power(25.0))));

  const double a1_prime = (1.0 + g) * a.a;
  const double a2_prime = (1.0 + g) * b.a;
  const double c1_prime = std::sqrt(a1_prime * a1_prime + a.b * a.b);
  const double c2_prime = std::sqrt(a2_prime * a2_prime + b.b * b.b);
  const double h1_prime = hue_degrees(a1_prime, a.b);
  const double h2_prime = hue_degrees(a2_prime, b.b);

  const double delta_l_prime = b.l - a.l;
  const double delta_c_prime = c2_prime - c1_prime;
  double delta_h_prime = 0.0;
  if (c1_prime * c2_prime != 0.0) {
    delta_h_prime = h2_prime - h1_prime;
    if (delta_h_prime > 180.0) {
      delta_h_prime -= 360.0;
    } else if (delta_h_prime < -180.0) {
      delta_h_prime += 360.0;
    }
  }
  const double delta_h_capital =
      2.0 * std::sqrt(c1_prime * c2_prime) *
      std::sin(degrees_to_radians(delta_h_prime * 0.5));

  const double l_bar_prime = 0.5 * (a.l + b.l);
  const double c_bar_prime = 0.5 * (c1_prime + c2_prime);
  double h_bar_prime = h1_prime + h2_prime;
  if (c1_prime * c2_prime != 0.0) {
    if (std::abs(h1_prime - h2_prime) <= 180.0) {
      h_bar_prime = 0.5 * (h1_prime + h2_prime);
    } else if (h1_prime + h2_prime < 360.0) {
      h_bar_prime = 0.5 * (h1_prime + h2_prime + 360.0);
    } else {
      h_bar_prime = 0.5 * (h1_prime + h2_prime - 360.0);
    }
  }

  const double t =
      1.0 - 0.17 * std::cos(degrees_to_radians(h_bar_prime - 30.0)) +
      0.24 * std::cos(degrees_to_radians(2.0 * h_bar_prime)) +
      0.32 * std::cos(degrees_to_radians(3.0 * h_bar_prime + 6.0)) -
      0.20 * std::cos(degrees_to_radians(4.0 * h_bar_prime - 63.0));
  const double delta_theta =
      30.0 * std::exp(-square((h_bar_prime - 275.0) / 25.0));
  const double c_bar_prime7 = seventh_power(c_bar_prime);
  const double r_c =
      2.0 * std::sqrt(c_bar_prime7 /
                      (c_bar_prime7 + seventh_power(25.0)));
  const double s_l =
      1.0 + (0.015 * square(l_bar_prime - 50.0)) /
                std::sqrt(20.0 + square(l_bar_prime - 50.0));
  const double s_c = 1.0 + 0.045 * c_bar_prime;
  const double s_h = 1.0 + 0.015 * c_bar_prime * t;
  const double r_t = -std::sin(degrees_to_radians(2.0 * delta_theta)) * r_c;

  const double l_term = delta_l_prime / s_l;
  const double c_term = delta_c_prime / s_c;
  const double h_term = delta_h_capital / s_h;
  const double value =
      l_term * l_term + c_term * c_term + h_term * h_term +
      r_t * c_term * h_term;
  return std::sqrt(std::max(0.0, value));
}

Xyz apply_ccm(const std::array<std::array<double, 3>, 3>& matrix,
              const CameraRgbPatch& rgb) {
  const auto r = rgb_array(rgb);
  Xyz out;
  out.x = matrix[0][0] * r[0] + matrix[0][1] * r[1] + matrix[0][2] * r[2];
  out.y = matrix[1][0] * r[0] + matrix[1][1] * r[1] + matrix[1][2] * r[2];
  out.z = matrix[2][0] * r[0] + matrix[2][1] * r[1] + matrix[2][2] * r[2];
  return out;
}

CcmFit fit_rgb_to_xyz_ccm(const std::vector<CameraRgbPatch>& camera_rgb,
                          const std::vector<Xyz>& target_xyz,
                          const Xyz& white_xyz) {
  if (camera_rgb.size() != target_xyz.size()) {
    throw std::runtime_error("ccm fit: camera and target patch counts differ");
  }
  if (camera_rgb.size() < 3) {
    throw std::runtime_error("ccm fit: at least three patches required");
  }

  CcmFit fit = fit_matrix_only(camera_rgb, target_xyz);
  const auto eval =
      evaluate_rgb_to_xyz_ccm(fit.matrix, camera_rgb, target_xyz, white_xyz);
  fit.mean_delta_e_76 = eval.mean_delta_e_76;
  fit.rms_delta_e_76 = eval.rms_delta_e_76;
  fit.max_delta_e_76 = eval.max_delta_e_76;
  fit.mean_delta_e_2000 = eval.mean_delta_e_2000;
  fit.rms_delta_e_2000 = eval.rms_delta_e_2000;
  fit.max_delta_e_2000 = eval.max_delta_e_2000;
  return fit;
}

CcmEvaluation evaluate_rgb_to_xyz_ccm(
    const std::array<std::array<double, 3>, 3>& matrix,
    const std::vector<CameraRgbPatch>& camera_rgb,
    const std::vector<Xyz>& target_xyz, const Xyz& white_xyz) {
  if (camera_rgb.size() != target_xyz.size()) {
    throw std::runtime_error("ccm fit: camera and target patch counts differ");
  }
  return evaluation_from_accumulator(
      evaluate_matrix(matrix, camera_rgb, target_xyz, white_xyz));
}

CcmCrossValidation cross_validate_rgb_to_xyz_ccm(
    const std::vector<CameraRgbPatch>& camera_rgb,
    const std::vector<Xyz>& target_xyz, const Xyz& white_xyz,
    std::size_t fold_count) {
  if (camera_rgb.size() != target_xyz.size()) {
    throw std::runtime_error("ccm fit: camera and target patch counts differ");
  }
  if (camera_rgb.size() < 4) {
    throw std::runtime_error("ccm fit: at least four patches required for CV");
  }
  if (fold_count < 2) {
    throw std::runtime_error("ccm fit: at least two CV folds required");
  }
  fold_count = std::min(fold_count, camera_rgb.size());

  DeltaAccumulator acc;
  for (std::size_t fold = 0; fold < fold_count; ++fold) {
    std::vector<CameraRgbPatch> train_rgb;
    std::vector<Xyz> train_xyz;
    std::vector<CameraRgbPatch> test_rgb;
    std::vector<Xyz> test_xyz;
    for (std::size_t i = 0; i < camera_rgb.size(); ++i) {
      if (i % fold_count == fold) {
        test_rgb.push_back(camera_rgb[i]);
        test_xyz.push_back(target_xyz[i]);
      } else {
        train_rgb.push_back(camera_rgb[i]);
        train_xyz.push_back(target_xyz[i]);
      }
    }
    if (train_rgb.size() < 3 || test_rgb.empty()) {
      throw std::runtime_error("ccm fit: invalid CV fold partition");
    }
    const auto fold_fit = fit_matrix_only(train_rgb, train_xyz);
    const auto fold_acc =
        evaluate_matrix(fold_fit.matrix, test_rgb, test_xyz, white_xyz);
    acc.count += fold_acc.count;
    acc.sum_76 += fold_acc.sum_76;
    acc.sumsq_76 += fold_acc.sumsq_76;
    acc.max_76 = std::max(acc.max_76, fold_acc.max_76);
    acc.sum_2000 += fold_acc.sum_2000;
    acc.sumsq_2000 += fold_acc.sumsq_2000;
    acc.max_2000 = std::max(acc.max_2000, fold_acc.max_2000);
  }

  const auto eval = evaluation_from_accumulator(acc);
  CcmCrossValidation out;
  out.patch_count = eval.patch_count;
  out.fold_count = fold_count;
  out.mean_delta_e_76 = eval.mean_delta_e_76;
  out.rms_delta_e_76 = eval.rms_delta_e_76;
  out.max_delta_e_76 = eval.max_delta_e_76;
  out.mean_delta_e_2000 = eval.mean_delta_e_2000;
  out.rms_delta_e_2000 = eval.rms_delta_e_2000;
  out.max_delta_e_2000 = eval.max_delta_e_2000;
  return out;
}

CcmLightnessSelection select_reference_lightness(
    const std::vector<Xyz>& target_xyz, const Xyz& white_xyz,
    double exclude_below_lstar) {
  if (!std::isfinite(exclude_below_lstar) || exclude_below_lstar < 0.0 ||
      exclude_below_lstar > 100.0) {
    throw std::runtime_error("ccm fit: lightness threshold must be in [0,100]");
  }
  CcmLightnessSelection out;
  out.max_lstar = exclude_below_lstar;
  for (std::size_t i = 0; i < target_xyz.size(); ++i) {
    const Lab target_lab = xyz_to_lab(target_xyz[i], white_xyz);
    if (target_lab.l < exclude_below_lstar) {
      out.excluded_indices.push_back(i);
    } else {
      out.kept_indices.push_back(i);
    }
  }
  return out;
}

CcmDarkPatchDiagnostics diagnose_dark_patches(
    const std::vector<CameraRgbPatch>& camera_rgb,
    const std::vector<Xyz>& target_xyz, const Xyz& white_xyz,
    const std::array<std::array<double, 3>, 3>& matrix, double max_lstar) {
  if (camera_rgb.size() != target_xyz.size()) {
    throw std::runtime_error("ccm fit: camera and target patch counts differ");
  }

  DeltaAccumulator acc;
  CcmDarkPatchDiagnostics out;
  out.max_lstar = max_lstar;
  for (std::size_t i = 0; i < camera_rgb.size(); ++i) {
    const Lab target_lab = xyz_to_lab(target_xyz[i], white_xyz);
    if (target_lab.l >= max_lstar) continue;
    const Lab predicted_lab =
        xyz_to_lab(apply_ccm(matrix, camera_rgb[i]), white_xyz);
    const double prior_max = acc.max_76;
    add_delta(acc, target_lab, predicted_lab);
    if (acc.max_76 > prior_max || acc.count == 1) {
      out.worst_patch_index = i;
    }
  }
  out.patch_count = acc.count;
  if (acc.count == 0) return out;

  const auto eval = evaluation_from_accumulator(acc);
  out.mean_delta_e_76 = eval.mean_delta_e_76;
  out.rms_delta_e_76 = eval.rms_delta_e_76;
  out.max_delta_e_76 = eval.max_delta_e_76;
  out.mean_delta_e_2000 = eval.mean_delta_e_2000;
  out.rms_delta_e_2000 = eval.rms_delta_e_2000;
  out.max_delta_e_2000 = eval.max_delta_e_2000;
  return out;
}

}  // namespace camera_iq
