#include "camera_iq/spectral_quality.hpp"

#include <cmath>
#include <stdexcept>
#include <string>
#include <vector>

namespace camera_iq {
namespace {

constexpr double kEpsilon = 1e-12;

double det3(const double m[3][3]) {
  return m[0][0] * (m[1][1] * m[2][2] - m[1][2] * m[2][1]) -
         m[0][1] * (m[1][0] * m[2][2] - m[1][2] * m[2][0]) +
         m[0][2] * (m[1][0] * m[2][1] - m[1][1] * m[2][0]);
}

std::vector<double> normalized_finite_vector(const std::vector<double>& values,
                                             const std::string& label) {
  double norm = 0.0;
  for (double value : values) {
    if (!std::isfinite(value)) {
      throw std::runtime_error("spectral quality: non-finite " + label);
    }
    norm = std::hypot(norm, value);
  }
  if (!(norm > 0.0) || !std::isfinite(norm)) {
    throw std::runtime_error("spectral quality: zero-norm " + label);
  }
  std::vector<double> normalized;
  normalized.reserve(values.size());
  for (double value : values) normalized.push_back(value / norm);
  return normalized;
}

}  // namespace

SpectralQualityResult compute_spectral_quality(
    const SpectralQualityInputs& in) {
  const std::size_t n = in.grid_nm.size();
  if (n < 4) {
    throw std::runtime_error(
        "spectral quality: need at least 4 samples to overdetermine a 3-param "
        "fit");
  }
  for (std::size_t i = 1; i < n; ++i) {
    if (in.grid_nm[i] <= in.grid_nm[i - 1]) {
      throw std::runtime_error(
          "spectral quality: wavelength grid must be strictly increasing");
    }
  }
  for (int c = 0; c < 3; ++c) {
    if (in.ssf[static_cast<std::size_t>(c)].size() != n ||
        in.cmf[static_cast<std::size_t>(c)].size() != n) {
      throw std::runtime_error("spectral quality: SSF/CMF size != grid");
    }
  }

  std::array<std::vector<double>, 3> ssf;
  std::array<std::vector<double>, 3> cmf;
  for (std::size_t channel = 0; channel < 3; ++channel) {
    ssf[channel] = normalized_finite_vector(
        in.ssf[channel], "SSF channel " + std::to_string(channel));
    cmf[channel] = normalized_finite_vector(
        in.cmf[channel], "CMF channel " + std::to_string(channel));
  }

  // Normalize every SSF column before solving. The Luther residual depends on
  // the subspace spanned by the channels, not their arbitrary amplitude units;
  // normalization also prevents a valid basis from being rejected solely
  // because all three channels were expressed at a small numeric scale.
  // CMFs are normalized as well because the reported residual is relative.
  // Gram matrix G = A^T A with A = [R | G | B]; b_m = A^T cmf_m.
  double gram[3][3] = {{0}};
  for (int i = 0; i < 3; ++i) {
    for (int j = 0; j < 3; ++j) {
      double s = 0;
      for (std::size_t k = 0; k < n; ++k) {
        s += ssf[static_cast<std::size_t>(i)][k] *
             ssf[static_cast<std::size_t>(j)][k];
      }
      gram[i][j] = s;
    }
  }
  const double det = det3(gram);
  const double diag_product = gram[0][0] * gram[1][1] * gram[2][2];
  // Hadamard: for a Gram matrix 0 <= det <= product(diagonals). A ratio near 0
  // means the SSF channels are (near-)linearly dependent -> no unique fit.
  if (diag_product <= kEpsilon || std::abs(det) <= 1e-9 * diag_product) {
    throw std::runtime_error(
        "spectral quality: rank-deficient SSF basis "
        "(channels not independent)");
  }

  SpectralQualityResult res;
  double sum_sq = 0;
  for (int m = 0; m < 3; ++m) {
    double b[3] = {0, 0, 0};
    for (int i = 0; i < 3; ++i) {
      for (std::size_t k = 0; k < n; ++k) {
        b[i] += ssf[static_cast<std::size_t>(i)][k] *
                cmf[static_cast<std::size_t>(m)][k];
      }
    }
    // Solve gram * coef = b by Cramer's rule.
    double coef[3];
    for (int col = 0; col < 3; ++col) {
      double mc[3][3];
      for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j) mc[i][j] = (j == col) ? b[i] : gram[i][j];
      coef[col] = det3(mc) / det;
    }
    double num = 0, den = 0;
    for (std::size_t k = 0; k < n; ++k) {
      const double fit =
          coef[0] * ssf[0][k] + coef[1] * ssf[1][k] + coef[2] * ssf[2][k];
      const double target = cmf[static_cast<std::size_t>(m)][k];
      const double d = target - fit;
      num += d * d;
      den += target * target;
    }
    res.cmf_residual[static_cast<std::size_t>(m)] = std::sqrt(num / den);
    sum_sq += res.cmf_residual[static_cast<std::size_t>(m)] *
              res.cmf_residual[static_cast<std::size_t>(m)];
  }
  res.combined_residual = std::sqrt(sum_sq / 3.0);
  res.quality_index = 1.0 - res.combined_residual;
  return res;
}

}  // namespace camera_iq
