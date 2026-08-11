#include "camera_iq/spectral_smi.hpp"

#include <cmath>
#include <stdexcept>
#include <string>
#include <utility>

#include "camera_iq/camera_rgb.hpp"   // CameraRgbPatch
#include "camera_iq/colorimetry.hpp"  // Xyz, CcmFit, fit/evaluate CCM

namespace camera_iq {
namespace {

// Trapezoidal integration weights over a (not necessarily uniform) grid,
// matching integrate_xyz() in colorimetry.cpp so the synthesized camera RGB and
// the true XYZ are integrated on identical footing.
std::vector<double> trapezoid_weights(const std::vector<double>& g) {
  const std::size_t n = g.size();
  std::vector<double> w(n, 0.0);
  if (n == 1) {
    w[0] = 1.0;
    return w;
  }
  for (std::size_t i = 0; i < n; ++i) {
    if (i == 0) {
      w[i] = (g[1] - g[0]) * 0.5;
    } else if (i + 1 == n) {
      w[i] = (g[i] - g[i - 1]) * 0.5;
    } else {
      w[i] = (g[i + 1] - g[i - 1]) * 0.5;
    }
  }
  return w;
}

void require_len(const std::vector<double>& v, std::size_t n, const char* what) {
  if (v.size() != n)
    throw std::runtime_error(std::string("spectral smi: ") + what +
                             " length does not match grid");
  for (double value : v) {
    if (!std::isfinite(value)) {
      throw std::runtime_error(std::string("spectral smi: non-finite ") + what);
    }
  }
}

std::array<double, 3> rgb_array(const CameraRgbPatch& rgb) {
  return {rgb.r, rgb.g, rgb.b};
}

std::array<double, 3> xyz_component_array(const Xyz& xyz) {
  return {xyz.x, xyz.y, xyz.z};
}

std::array<double, 3> solve_3x3(std::array<std::array<double, 3>, 3> a,
                                std::array<double, 3> b) {
  for (int col = 0; col < 3; ++col) {
    int pivot = col;
    for (int r = col + 1; r < 3; ++r) {
      if (std::abs(a[r][col]) > std::abs(a[pivot][col])) pivot = r;
    }
    if (std::abs(a[pivot][col]) < 1e-12)
      throw std::runtime_error(
          "spectral smi: singular white-preserving design matrix");
    if (pivot != col) {
      std::swap(a[pivot], a[col]);
      std::swap(b[pivot], b[col]);
    }
    const double div = a[col][col];
    for (int c = col; c < 3; ++c) a[col][c] /= div;
    b[col] /= div;
    for (int r = 0; r < 3; ++r) {
      if (r == col) continue;
      const double f = a[r][col];
      for (int c = col; c < 3; ++c) a[r][c] -= f * a[col][c];
      b[r] -= f * b[col];
    }
  }
  return b;
}

std::array<std::array<double, 3>, 3> fit_white_preserving_matrix(
    const std::vector<CameraRgbPatch>& camera,
    const std::vector<Xyz>& target, const CameraRgbPatch& camera_white,
    const Xyz& target_white) {
  std::array<std::array<double, 3>, 3> normal{};
  std::array<std::array<double, 3>, 3> rhs{};
  for (std::size_t i = 0; i < camera.size(); ++i) {
    const auto rgb = rgb_array(camera[i]);
    const std::array<double, 3> xyz = {target[i].x, target[i].y, target[i].z};
    for (int r = 0; r < 3; ++r) {
      for (int c = 0; c < 3; ++c) normal[r][c] += rgb[r] * rgb[c];
      for (int ch = 0; ch < 3; ++ch) rhs[ch][r] += rgb[r] * xyz[ch];
    }
  }

  const auto white_rgb = rgb_array(camera_white);
  const auto white_xyz = xyz_component_array(target_white);
  std::array<std::array<double, 3>, 3> matrix{};
  for (int ch = 0; ch < 3; ++ch) {
    const auto unconstrained = solve_3x3(normal, rhs[ch]);
    const auto normal_inv_white = solve_3x3(normal, white_rgb);
    double current = 0.0;
    double denom = 0.0;
    for (int i = 0; i < 3; ++i) {
      current += white_rgb[i] * unconstrained[i];
      denom += white_rgb[i] * normal_inv_white[i];
    }
    if (std::abs(denom) < 1e-18)
      throw std::runtime_error(
          "spectral smi: degenerate white-preserving constraint");
    const double lambda = (current - white_xyz[ch]) / denom;
    for (int i = 0; i < 3; ++i) {
      matrix[ch][i] = unconstrained[i] - lambda * normal_inv_white[i];
    }
  }
  return matrix;
}

}  // namespace

SpectralSmiResult compute_spectral_smi(const SpectralSmiInputs& in) {
  const std::size_t n = in.grid_nm.size();
  if (n < 2)
    throw std::runtime_error("spectral smi: grid needs at least two samples");
  for (std::size_t i = 0; i < n; ++i) {
    if (!std::isfinite(in.grid_nm[i])) {
      throw std::runtime_error("spectral smi: non-finite wavelength grid");
    }
    if (i > 0) {
      const double step = in.grid_nm[i] - in.grid_nm[i - 1];
      if (!(step > 0.0) || !std::isfinite(step)) {
        throw std::runtime_error(
            "spectral smi: wavelength grid must be strictly increasing with finite steps");
      }
    }
  }
  if (!std::isfinite(in.smi_slope) || !(in.smi_slope > 0.0)) {
    throw std::runtime_error("spectral smi: slope must be finite and positive");
  }
  require_len(in.illuminant, n, "illuminant");
  for (int c = 0; c < 3; ++c) {
    require_len(in.cmf[static_cast<std::size_t>(c)], n, "cmf");
    require_len(in.ssf[static_cast<std::size_t>(c)], n, "ssf");
  }
  if (in.reflectance.size() < 3)
    throw std::runtime_error(
        "spectral smi: at least three test colors are required");
  for (const auto& refl : in.reflectance) require_len(refl, n, "reflectance");

  const std::vector<double> w = trapezoid_weights(in.grid_nm);
  for (double weight : w) {
    if (!(weight > 0.0) || !std::isfinite(weight)) {
      throw std::runtime_error(
          "spectral smi: integration weights must be finite and positive");
    }
  }

  // Perfect-diffuser white, scaled so its luminance Y = 100 (CIELAB reference).
  double xw = 0.0, yw = 0.0, zw = 0.0;
  for (std::size_t i = 0; i < n; ++i) {
    const double ew = in.illuminant[i] * w[i];
    xw += in.cmf[0][i] * ew;
    yw += in.cmf[1][i] * ew;
    zw += in.cmf[2][i] * ew;
  }
  if (!std::isfinite(xw) || !std::isfinite(yw) || !std::isfinite(zw) ||
      yw <= 0.0)
    throw std::runtime_error(
        "spectral smi: illuminant x CMF gives non-positive white luminance");
  const double scale = 100.0 / yw;
  const Xyz white{xw * scale, 100.0, zw * scale};

  double rw = 0.0, gw = 0.0, bw = 0.0;
  for (std::size_t i = 0; i < n; ++i) {
    const double e = in.illuminant[i] * w[i];
    rw += in.ssf[0][i] * e;
    gw += in.ssf[1][i] * e;
    bw += in.ssf[2][i] * e;
  }
  if (!std::isfinite(rw) || !std::isfinite(gw) || !std::isfinite(bw) ||
      rw <= 0.0 || gw <= 0.0 || bw <= 0.0) {
    throw std::runtime_error(
        "spectral smi: camera white response must be positive");
  }
  const CameraRgbPatch camera_white{rw, gw, bw};

  std::vector<Xyz> target;
  std::vector<CameraRgbPatch> camera;
  target.reserve(in.reflectance.size());
  camera.reserve(in.reflectance.size());
  for (const auto& refl : in.reflectance) {
    double x = 0.0, y = 0.0, z = 0.0, r = 0.0, g = 0.0, b = 0.0;
    for (std::size_t i = 0; i < n; ++i) {
      const double e = in.illuminant[i] * refl[i] * w[i];
      x += in.cmf[0][i] * e;
      y += in.cmf[1][i] * e;
      z += in.cmf[2][i] * e;
      r += in.ssf[0][i] * e;
      g += in.ssf[1][i] * e;
      b += in.ssf[2][i] * e;
    }
    if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z) ||
        !std::isfinite(r) || !std::isfinite(g) || !std::isfinite(b)) {
      throw std::runtime_error(
          "spectral smi: synthesized patch response is not representable");
    }
    target.push_back(Xyz{x * scale, y * scale, z * scale});
    camera.push_back(CameraRgbPatch{r, g, b});
  }

  // Unconstrained least-squares 3x3 RGB->XYZ transform plus residual CIELAB
  // error. This is the ISO 17321-style characterization shape, not a statement of
  // bit-exact Annex B optimizer/normalization behavior.
  const CcmFit fit = fit_rgb_to_xyz_ccm(camera, target, white);
  const auto constrained_matrix =
      fit_white_preserving_matrix(camera, target, camera_white, white);
  const auto constrained_eval =
      evaluate_rgb_to_xyz_ccm(constrained_matrix, camera, target, white);
  const Lab white_lab = xyz_to_lab(white, white);
  const Lab constrained_white_lab =
      xyz_to_lab(apply_ccm(constrained_matrix, camera_white), white);

  SpectralSmiResult res;
  res.method = "iso17321_style_smi_optimal_3x3_then_mean_cielab_de";
  res.patch_count = in.reflectance.size();
  res.mean_delta_e_76 = fit.mean_delta_e_76;
  res.max_delta_e_76 = fit.max_delta_e_76;
  res.rms_delta_e_76 = fit.rms_delta_e_76;
  res.mean_delta_e_2000 = fit.mean_delta_e_2000;
  res.max_delta_e_2000 = fit.max_delta_e_2000;
  res.rms_delta_e_2000 = fit.rms_delta_e_2000;
  res.smi_slope = in.smi_slope;
  res.smi = 100.0 - in.smi_slope * fit.mean_delta_e_76;
  res.matrix = fit.matrix;
  res.white_preserving_mean_delta_e_76 = constrained_eval.mean_delta_e_76;
  res.white_preserving_max_delta_e_76 = constrained_eval.max_delta_e_76;
  res.white_preserving_rms_delta_e_76 = constrained_eval.rms_delta_e_76;
  res.white_preserving_mean_delta_e_2000 = constrained_eval.mean_delta_e_2000;
  res.white_preserving_smi =
      100.0 - in.smi_slope * constrained_eval.mean_delta_e_76;
  res.white_preserving_delta_smi = res.white_preserving_smi - res.smi;
  if (!std::isfinite(res.smi) || !std::isfinite(res.white_preserving_smi) ||
      !std::isfinite(res.white_preserving_delta_smi)) {
    throw std::runtime_error("spectral smi: score is not representable");
  }
  const double dl = white_lab.l - constrained_white_lab.l;
  const double da = white_lab.a - constrained_white_lab.a;
  const double db = white_lab.b - constrained_white_lab.b;
  res.white_preserving_white_delta_e_76 = std::sqrt(dl * dl + da * da + db * db);
  res.white_preserving_matrix = constrained_matrix;
  return res;
}

}  // namespace camera_iq
