#include "camera_iq/sfr.hpp"

#include <algorithm>
#include <cmath>
#include <iterator>
#include <limits>
#include <numbers>
#include <numeric>
#include <string>

namespace camera_iq {
namespace {

constexpr double kEps = 1e-12;

bool is_green(const CfaImageView& image, int x, int y) {
  const std::size_t pos = static_cast<std::size_t>((y & 1) * 2 + (x & 1));
  const int color_index = image.color_at_position[pos];
  return color_index >= 0 &&
         static_cast<std::size_t>(color_index) < image.cdesc.size() &&
         image.cdesc[static_cast<std::size_t>(color_index)] == 'G';
}

double sample_at(const CfaImageView& image, int x, int y) {
  return image.samples[static_cast<std::size_t>(y) *
                           static_cast<std::size_t>(image.row_stride_pixels) +
                       static_cast<std::size_t>(x)];
}

SfrResult reject_result(RoiRect roi, std::string reason) {
  SfrResult result;
  result.roi = roi;
  result.rejection_reason = std::move(reason);
  return result;
}

SfrResult reject_result(SfrResult result, std::string reason) {
  result.accepted = false;
  result.rejection_reason = std::move(reason);
  return result;
}

bool valid_sfr_options(const SfrOptions& options) {
  return std::isfinite(options.bin_spacing_px) &&
         options.bin_spacing_px > 0.0 &&
         std::isfinite(options.min_edge_angle_deg) &&
         std::isfinite(options.max_edge_angle_deg) &&
         options.min_edge_angle_deg >= 0.0 &&
         options.max_edge_angle_deg > options.min_edge_angle_deg &&
         options.max_edge_angle_deg <= 90.0 &&
         std::isfinite(options.min_contrast_dn) &&
         options.min_contrast_dn >= 0.0 &&
         std::isfinite(options.near_saturation_fraction) &&
         options.near_saturation_fraction > 0.0 &&
         options.near_saturation_fraction <= 1.0 &&
         options.min_roi_dimension_px >= 2 && options.min_line_samples >= 8;
}

struct LinePoint {
  double scan = 0.0;
  double edge = 0.0;
};

std::optional<double> edge_centroid_for_line(
    const std::vector<std::pair<double, double>>& samples,
    double* line_contrast, std::size_t min_samples) {
  if (samples.size() < min_samples) return std::nullopt;
  const std::size_t q = std::max<std::size_t>(2, samples.size() / 4);
  double start = 0.0;
  double end = 0.0;
  for (std::size_t i = 0; i < q; ++i) {
    start += samples[i].second;
    end += samples[samples.size() - 1 - i].second;
  }
  start /= static_cast<double>(q);
  end /= static_cast<double>(q);
  if (line_contrast) {
    *line_contrast = std::abs(end - start);
  }
  const double threshold = 0.5 * (start + end);
  std::optional<double> coarse;
  double best_step = -1.0;
  for (std::size_t i = 0; i + 1 < samples.size(); ++i) {
    const double v0 = samples[i].second - threshold;
    const double v1 = samples[i + 1].second - threshold;
    const double step = std::abs(samples[i + 1].second - samples[i].second);
    if ((v0 == 0.0 || v0 * v1 <= 0.0) && step > best_step) {
      const double denom = samples[i + 1].second - samples[i].second;
      const double t = std::abs(denom) > kEps
                           ? (threshold - samples[i].second) / denom
                           : 0.5;
      coarse = samples[i].first + std::clamp(t, 0.0, 1.0) *
                                      (samples[i + 1].first - samples[i].first);
      best_step = step;
    }
  }
  if (!coarse) return std::nullopt;

  constexpr double kWindowPx = 8.0;
  double weighted = 0.0;
  double weight_sum = 0.0;
  for (std::size_t i = 0; i + 1 < samples.size(); ++i) {
    const double mid = 0.5 * (samples[i].first + samples[i + 1].first);
    if (std::abs(mid - *coarse) > kWindowPx) continue;
    const double w = std::abs(samples[i + 1].second - samples[i].second);
    weighted += mid * w;
    weight_sum += w;
  }
  if (weight_sum <= kEps) return coarse;
  return weighted / weight_sum;
}

std::optional<std::pair<double, double>> fit_line(
    const std::vector<LinePoint>& points, std::size_t min_samples) {
  if (points.size() < min_samples) return std::nullopt;
  double sx = 0.0;
  double sy = 0.0;
  double sxx = 0.0;
  double sxy = 0.0;
  for (const auto& p : points) {
    sx += p.scan;
    sy += p.edge;
    sxx += p.scan * p.scan;
    sxy += p.scan * p.edge;
  }
  const double n = static_cast<double>(points.size());
  const double denom = n * sxx - sx * sx;
  if (std::abs(denom) <= kEps) return std::nullopt;
  const double slope = (n * sxy - sx * sy) / denom;
  const double intercept = (sy - slope * sx) / n;
  return std::pair{slope, intercept};
}

double interpolate_crossing(const std::vector<double>& x,
                            const std::vector<double>& y, double target) {
  for (std::size_t i = 0; i + 1 < x.size(); ++i) {
    const double y0 = y[i] - target;
    const double y1 = y[i + 1] - target;
    if (y0 == 0.0) return x[i];
    if (y0 * y1 <= 0.0) {
      const double denom = y[i + 1] - y[i];
      const double t = std::abs(denom) > kEps ? (target - y[i]) / denom : 0.0;
      return x[i] + std::clamp(t, 0.0, 1.0) * (x[i + 1] - x[i]);
    }
  }
  return std::numeric_limits<double>::quiet_NaN();
}

double interpolate_curve(const std::vector<double>& x,
                         const std::vector<double>& y, double target_x) {
  if (x.empty() || y.empty() || x.size() != y.size()) {
    return std::numeric_limits<double>::quiet_NaN();
  }
  if (target_x < x.front() || target_x > x.back()) {
    return std::numeric_limits<double>::quiet_NaN();
  }
  if (target_x == x.front()) return y.front();
  if (target_x == x.back()) return y.back();
  for (std::size_t i = 0; i + 1 < x.size(); ++i) {
    if (target_x >= x[i] && target_x <= x[i + 1]) {
      const double denom = x[i + 1] - x[i];
      const double t = std::abs(denom) > kEps ? (target_x - x[i]) / denom : 0.0;
      return y[i] + t * (y[i + 1] - y[i]);
    }
  }
  return std::numeric_limits<double>::quiet_NaN();
}

double find_mtf_crossing(const std::vector<double>& f,
                         const std::vector<double>& mtf, double target,
                         std::size_t start_index = 0) {
  if (start_index >= f.size() || f.size() != mtf.size()) return 0.0;
  for (std::size_t i = start_index; i + 1 < f.size(); ++i) {
    if (mtf[i] >= target && mtf[i + 1] <= target) {
      const double denom = mtf[i + 1] - mtf[i];
      const double t = std::abs(denom) > kEps ? (target - mtf[i]) / denom : 0.0;
      return f[i] + std::clamp(t, 0.0, 1.0) * (f[i + 1] - f[i]);
    }
  }
  return 0.0;
}

}  // namespace

std::vector<double> dft_magnitude(const std::vector<double>& signal) {
  const std::size_t n = signal.size();
  std::vector<double> out(n / 2 + 1, 0.0);
  for (std::size_t k = 0; k < out.size(); ++k) {
    double re = 0.0;
    double im = 0.0;
    for (std::size_t t = 0; t < n; ++t) {
      const double phase = -2.0 * std::numbers::pi * static_cast<double>(k) *
                           static_cast<double>(t) / static_cast<double>(n);
      re += signal[t] * std::cos(phase);
      im += signal[t] * std::sin(phase);
    }
    out[k] = std::hypot(re, im);
  }
  return out;
}

double adjacent_difference_response(double frequency_cy_per_px,
                                    double sample_spacing_px) {
  const double x = std::numbers::pi * frequency_cy_per_px * sample_spacing_px;
  if (std::abs(x) <= kEps) return 1.0;
  return std::sin(x) / x;
}

SfrResult analyze_green_sfr(const CfaImageView& image, const RoiRect& requested,
                            const SfrOptions& options) {
  if (!valid_sfr_options(options)) {
    return reject_result(requested, "invalid_options");
  }
  if (image.width <= 0 || image.height <= 0 ||
      image.row_stride_pixels < image.width ||
      image.samples.size() < static_cast<std::size_t>(image.row_stride_pixels) *
                                 static_cast<std::size_t>(image.height) ||
      !std::isfinite(image.white_level) ||
      !std::all_of(
          image.black_per_channel.begin(),
          image.black_per_channel.end(),
          [](double value) { return std::isfinite(value); })) {
    return reject_result(requested, "invalid_raw_image");
  }
  const auto roi = cfa_balanced_roi(requested, image.width, image.height);
  if (!roi || roi->width < options.min_roi_dimension_px ||
      roi->height < options.min_roi_dimension_px) {
    return reject_result(requested, "roi_too_small");
  }

  SfrResult result;
  result.roi = *roi;
  result.oversample = 1.0 / options.bin_spacing_px;

  double dx_sum = 0.0;
  int dx_n = 0;
  double dy_sum = 0.0;
  int dy_n = 0;
  double mn = std::numeric_limits<double>::infinity();
  double mx = -std::numeric_limits<double>::infinity();
  int saturated = 0;
  for (int y = roi->y; y < roi->y + roi->height; ++y) {
    for (int x = roi->x; x < roi->x + roi->width; ++x) {
      if (!is_green(image, x, y)) continue;
      const double v = sample_at(image, x, y);
      if (!std::isfinite(v)) {
        return reject_result(std::move(result), "invalid_raw_image");
      }
      mn = std::min(mn, v);
      mx = std::max(mx, v);
      ++result.green_sample_count;
      if (image.white_level > 0.0) {
        const std::size_t pos = static_cast<std::size_t>((y & 1) * 2 + (x & 1));
        const double raw = v + image.black_per_channel[pos];
        if (raw >= options.near_saturation_fraction * image.white_level) {
          ++saturated;
        }
      }
      if (x + 2 < roi->x + roi->width && is_green(image, x + 2, y)) {
        dx_sum += std::abs(sample_at(image, x + 2, y) - v);
        ++dx_n;
      }
      if (y + 2 < roi->y + roi->height && is_green(image, x, y + 2)) {
        dy_sum += std::abs(sample_at(image, x, y + 2) - v);
        ++dy_n;
      }
    }
  }
  if (result.green_sample_count == 0) {
    return reject_result(std::move(result), "no_green_samples");
  }
  result.saturated_fraction = static_cast<double>(saturated) /
                              static_cast<double>(result.green_sample_count);
  result.contrast_dn = mx - mn;
  if (result.contrast_dn < options.min_contrast_dn) {
    return reject_result(std::move(result), "low_contrast");
  }
  if (result.saturated_fraction > 0.0) {
    return reject_result(std::move(result), "roi_saturated");
  }

  const double dx_mean = dx_n > 0 ? dx_sum / static_cast<double>(dx_n) : 0.0;
  const double dy_mean = dy_n > 0 ? dy_sum / static_cast<double>(dy_n) : 0.0;
  const bool horizontal = dy_mean >= dx_mean;
  result.orientation = horizontal ? "horizontal" : "vertical";

  std::vector<LinePoint> line_points;
  std::vector<double> line_contrasts;
  if (horizontal) {
    for (int x = roi->x; x < roi->x + roi->width; ++x) {
      std::vector<std::pair<double, double>> samples;
      for (int y = roi->y; y < roi->y + roi->height; ++y) {
        if (is_green(image, x, y))
          samples.push_back({static_cast<double>(y), sample_at(image, x, y)});
      }
      double line_contrast = 0.0;
      const auto edge = edge_centroid_for_line(
          samples, &line_contrast,
          static_cast<std::size_t>(options.min_line_samples));
      if (edge) {
        line_points.push_back({static_cast<double>(x), *edge});
        line_contrasts.push_back(line_contrast);
      }
    }
  } else {
    for (int y = roi->y; y < roi->y + roi->height; ++y) {
      std::vector<std::pair<double, double>> samples;
      for (int x = roi->x; x < roi->x + roi->width; ++x) {
        if (is_green(image, x, y))
          samples.push_back({static_cast<double>(x), sample_at(image, x, y)});
      }
      double line_contrast = 0.0;
      const auto edge = edge_centroid_for_line(
          samples, &line_contrast,
          static_cast<std::size_t>(options.min_line_samples));
      if (edge) {
        line_points.push_back({static_cast<double>(y), *edge});
        line_contrasts.push_back(line_contrast);
      }
    }
  }
  const auto line =
      fit_line(line_points, static_cast<std::size_t>(options.min_line_samples));
  if (!line) return reject_result(std::move(result), "edge_fit_failed");
  const double slope = line->first;
  const double intercept = line->second;
  result.edge_angle_deg = std::atan(slope) * 180.0 / std::numbers::pi;
  const double abs_angle = std::abs(result.edge_angle_deg);
  if (abs_angle < options.min_edge_angle_deg ||
      abs_angle > options.max_edge_angle_deg) {
    return reject_result(std::move(result), "edge_angle_out_of_range");
  }

  struct Bin {
    double sum = 0.0;
    int n = 0;
  };
  std::vector<std::pair<double, double>> projected;
  projected.reserve(static_cast<std::size_t>(result.green_sample_count));
  double d_min = std::numeric_limits<double>::infinity();
  double d_max = -std::numeric_limits<double>::infinity();
  for (int y = roi->y; y < roi->y + roi->height; ++y) {
    for (int x = roi->x; x < roi->x + roi->width; ++x) {
      if (!is_green(image, x, y)) continue;
      double d = 0.0;
      if (horizontal) {
        d = (static_cast<double>(y) -
             (slope * static_cast<double>(x) + intercept)) /
            std::sqrt(1.0 + slope * slope);
      } else {
        d = (static_cast<double>(x) -
             (slope * static_cast<double>(y) + intercept)) /
            std::sqrt(1.0 + slope * slope);
      }
      projected.push_back({d, sample_at(image, x, y)});
      d_min = std::min(d_min, d);
      d_max = std::max(d_max, d);
    }
  }
  if (projected.size() < 32 || !(d_max > d_min)) {
    return reject_result(std::move(result), "insufficient_projected_samples");
  }
  const double first_bin_value = std::floor(d_min / options.bin_spacing_px);
  const double last_bin_value = std::ceil(d_max / options.bin_spacing_px);
  constexpr double kMaxEsfBins = 8192.0;
  if (!std::isfinite(first_bin_value) || !std::isfinite(last_bin_value) ||
      first_bin_value < static_cast<double>(std::numeric_limits<int>::min()) ||
      last_bin_value > static_cast<double>(std::numeric_limits<int>::max()) ||
      last_bin_value < first_bin_value ||
      last_bin_value - first_bin_value + 1.0 > kMaxEsfBins) {
    return reject_result(std::move(result), "invalid_esf_grid");
  }
  const int first_bin = static_cast<int>(first_bin_value);
  const int last_bin = static_cast<int>(last_bin_value);
  std::vector<Bin> bins(static_cast<std::size_t>(last_bin - first_bin + 1));
  for (const auto& p : projected) {
    const int idx =
        static_cast<int>(std::floor(p.first / options.bin_spacing_px)) -
        first_bin;
    if (idx >= 0 && static_cast<std::size_t>(idx) < bins.size()) {
      bins[static_cast<std::size_t>(idx)].sum += p.second;
      ++bins[static_cast<std::size_t>(idx)].n;
    }
  }

  const auto first_filled = std::find_if(
      bins.begin(), bins.end(), [](const Bin& bin) { return bin.n > 0; });
  const auto last_filled = std::find_if(
      bins.rbegin(), bins.rend(), [](const Bin& bin) { return bin.n > 0; });
  if (first_filled == bins.end() || last_filled == bins.rend()) {
    return reject_result(std::move(result), "underfilled_esf");
  }
  const std::size_t first_filled_index =
      static_cast<std::size_t>(std::distance(bins.begin(), first_filled));
  const std::size_t last_filled_index =
      bins.size() - 1 -
      static_cast<std::size_t>(std::distance(bins.rbegin(), last_filled));

  std::vector<double> x;
  std::vector<double> esf;
  x.reserve(last_filled_index - first_filled_index + 1);
  esf.reserve(last_filled_index - first_filled_index + 1);
  for (std::size_t i = first_filled_index; i <= last_filled_index; ++i) {
    x.push_back(
        (static_cast<double>(first_bin) + static_cast<double>(i) + 0.5) *
        options.bin_spacing_px);
    esf.push_back(bins[i].n > 0 ? bins[i].sum / static_cast<double>(bins[i].n)
                                : std::numeric_limits<double>::quiet_NaN());
  }
  std::size_t missing_bins = 0;
  std::size_t max_gap = 0;
  std::size_t current_gap = 0;
  for (const double value : esf) {
    if (std::isfinite(value)) {
      current_gap = 0;
      continue;
    }
    ++missing_bins;
    ++current_gap;
    max_gap = std::max(max_gap, current_gap);
  }
  // Keep interpolation bounded: a minority of missing fine-grid bins is
  // recoverable, but long holes or a mostly synthesized ESF are not usable.
  constexpr std::size_t kMaxInterpolatedGapBins = 16;
  constexpr double kMaxInterpolatedFraction = 0.45;
  if (max_gap > kMaxInterpolatedGapBins ||
      static_cast<double>(missing_bins) / static_cast<double>(esf.size()) >
          kMaxInterpolatedFraction) {
    return reject_result(std::move(result), "underfilled_esf");
  }
  for (std::size_t i = 0; i < esf.size();) {
    if (std::isfinite(esf[i])) {
      ++i;
      continue;
    }
    const std::size_t gap_begin = i;
    while (i < esf.size() && !std::isfinite(esf[i])) ++i;
    const std::size_t gap_end = i;
    const double left = esf[gap_begin - 1];
    const double right = esf[gap_end];
    const double span = static_cast<double>(gap_end - gap_begin + 1);
    for (std::size_t j = gap_begin; j < gap_end; ++j) {
      const double t = static_cast<double>(j - gap_begin + 1) / span;
      esf[j] = left + t * (right - left);
    }
  }
  if (esf.size() < 32) {
    return reject_result(std::move(result), "underfilled_esf");
  }

  const std::size_t tail = std::max<std::size_t>(4, esf.size() / 10);
  const double start_mean =
      std::accumulate(esf.begin(), esf.begin() + static_cast<long>(tail), 0.0) /
      static_cast<double>(tail);
  const double end_mean =
      std::accumulate(esf.end() - static_cast<long>(tail), esf.end(), 0.0) /
      static_cast<double>(tail);
  if (start_mean > end_mean) {
    std::reverse(esf.begin(), esf.end());
    for (double& value : x) value = -value;
    std::reverse(x.begin(), x.end());
  }
  const auto [esf_min_it, esf_max_it] =
      std::minmax_element(esf.begin(), esf.end());
  const double esf_min = *esf_min_it;
  const double esf_range = *esf_max_it - esf_min;
  if (esf_range <= kEps) {
    return reject_result(std::move(result), "low_contrast");
  }
  for (double& value : esf) value = (value - esf_min) / esf_range;

  const double x10 = interpolate_crossing(x, esf, 0.1);
  const double x90 = interpolate_crossing(x, esf, 0.9);
  if (!std::isfinite(x10) || !std::isfinite(x90)) {
    return reject_result(std::move(result), "rise_distance_not_found");
  }
  result.r1090_px = std::abs(x90 - x10);
  const double edge_center = 0.5 * (x10 + x90);
  const double left_support = edge_center - x.front();
  const double right_support = x.back() - edge_center;
  const double symmetric_half_support = std::min(left_support, right_support);
  const double longer_support = std::max(left_support, right_support);
  // A Fourier estimate needs measured plateaus on both sides of the edge.
  // Refuse a transition whose shorter side has less than half the support of
  // its longer side; windowing cannot recover the omitted side of a badly
  // placed ROI. Crop accepted data to the largest symmetric interval so the
  // Hamming window is centered on the measured transition rather than on an
  // arbitrary ROI.
  if (!std::isfinite(symmetric_half_support) ||
      !std::isfinite(longer_support) || longer_support <= 0.0 ||
      2.0 * symmetric_half_support < longer_support) {
    return reject_result(std::move(result), "insufficient_edge_support");
  }
  const auto symmetric_begin = std::lower_bound(
      x.begin(), x.end(), edge_center - symmetric_half_support);
  const auto symmetric_end = std::upper_bound(
      x.begin(), x.end(), edge_center + symmetric_half_support);
  const std::size_t symmetric_size =
      static_cast<std::size_t>(std::distance(symmetric_begin, symmetric_end));
  if (symmetric_size < 32) {
    return reject_result(std::move(result), "insufficient_edge_support");
  }
  const std::size_t symmetric_offset =
      static_cast<std::size_t>(std::distance(x.begin(), symmetric_begin));
  x = std::vector<double>(symmetric_begin, symmetric_end);
  esf = std::vector<double>(
      esf.begin() + static_cast<long>(symmetric_offset),
      esf.begin() + static_cast<long>(symmetric_offset + symmetric_size));

  std::vector<double> lsf;
  lsf.reserve(esf.size() - 1);
  for (std::size_t i = 0; i + 1 < esf.size(); ++i) {
    lsf.push_back(esf[i + 1] - esf[i]);
  }
  for (std::size_t i = 0; i < lsf.size(); ++i) {
    const double w =
        0.54 - 0.46 * std::cos(2.0 * std::numbers::pi * static_cast<double>(i) /
                               static_cast<double>(lsf.size() - 1));
    lsf[i] *= w;
  }
  const auto mag = dft_magnitude(lsf);
  if (mag.empty() || mag[0] <= kEps) {
    return reject_result(std::move(result), "dc_normalization_zero");
  }

  result.mtf_frequency_cy_per_px.reserve(mag.size());
  result.mtf.reserve(mag.size());
  for (std::size_t k = 0; k < mag.size(); ++k) {
    const double f =
        (static_cast<double>(k) / static_cast<double>(lsf.size())) /
        options.bin_spacing_px;
    double mtf = mag[k] / mag[0];
    const double response =
        adjacent_difference_response(f, options.bin_spacing_px);
    if (response > kEps) mtf /= response;
    result.mtf_frequency_cy_per_px.push_back(f);
    result.mtf.push_back(mtf);
  }
  if (result.mtf_frequency_cy_per_px.empty() ||
      result.mtf_frequency_cy_per_px.front() > 0.5 ||
      result.mtf_frequency_cy_per_px.back() < 0.5) {
    return reject_result(std::move(result), "nyquist_not_sampled");
  }
  result.mtf_at_nyquist =
      interpolate_curve(result.mtf_frequency_cy_per_px, result.mtf, 0.5);
  if (!std::isfinite(result.mtf_at_nyquist)) {
    return reject_result(std::move(result), "nyquist_not_sampled");
  }
  result.mtf50_cy_per_px =
      find_mtf_crossing(result.mtf_frequency_cy_per_px, result.mtf, 0.5);
  const auto peak_it = std::max_element(result.mtf.begin(), result.mtf.end());
  const double peak = *peak_it;
  const std::size_t peak_index =
      static_cast<std::size_t>(std::distance(result.mtf.begin(), peak_it));
  std::vector<double> mtfp = result.mtf;
  if (peak > kEps) {
    for (double& v : mtfp) v /= peak;
    result.mtf50p_cy_per_px = find_mtf_crossing(result.mtf_frequency_cy_per_px,
                                                mtfp, 0.5, peak_index);
  }
  if (result.mtf50_cy_per_px <= 0.0) {
    return reject_result(std::move(result), "mtf50_not_found");
  }
  if (!std::isfinite(result.mtf50p_cy_per_px) ||
      result.mtf50p_cy_per_px <= 0.0) {
    return reject_result(std::move(result), "mtf50p_not_found");
  }
  result.accepted = true;
  return result;
}

}  // namespace camera_iq
