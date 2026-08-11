// Public tests for the slanted-edge estimator.
//
// Every input here is synthetic and generated in this file, so the numerical
// behavior of the published estimator can be checked without any camera
// capture. Numeric tolerances retain the estimator's reviewed synthetic
// regression bounds; they were not tightened around this copy's output.

#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <numbers>
#include <span>
#include <string>
#include <vector>

#include "camera_iq/cfa_image_view.hpp"
#include "camera_iq/roi.hpp"
#include "camera_iq/sfr.hpp"

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

// A Gaussian-blurred step has a known MTF, which is what makes it usable as an
// oracle: for a Gaussian of width sigma the MTF50 sits at 0.18739 / sigma
// cycles per pixel.
double erf_edge(double distance, double sigma) {
  return 0.5 * (1.0 + std::erf(distance / (sigma * std::numbers::sqrt2)));
}

// Owns the samples so the returned view stays valid for the caller's scope.
struct SyntheticEdge {
  std::vector<double> samples;
  camera_iq::CfaImageView view;
};

SyntheticEdge synthetic_green_edge(int width, int height, double angle_deg,
                                   double sigma, bool horizontal,
                                   double edge_offset_px = 0.0) {
  SyntheticEdge edge;
  edge.samples.assign(static_cast<std::size_t>(width * height), 0.0);

  const double slope = std::tan(angle_deg * std::numbers::pi / 180.0);
  const double cx = 0.5 * static_cast<double>(width - 1);
  const double cy = 0.5 * static_cast<double>(height - 1);
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      double distance = 0.0;
      if (horizontal) {
        const double edge_y =
            cy + edge_offset_px + slope * (static_cast<double>(x) - cx);
        distance =
            (static_cast<double>(y) - edge_y) / std::sqrt(1.0 + slope * slope);
      } else {
        const double edge_x =
            cx + edge_offset_px + slope * (static_cast<double>(y) - cy);
        distance =
            (static_cast<double>(x) - edge_x) / std::sqrt(1.0 + slope * slope);
      }
      edge.samples[static_cast<std::size_t>(y * width + x)] =
          1000.0 * erf_edge(distance, sigma);
    }
  }

  edge.view.width = width;
  edge.view.height = height;
  edge.view.row_stride_pixels = width;
  edge.view.cdesc = "RGBG";
  edge.view.color_at_position = {0, 1, 1, 2};
  edge.view.white_level = 4095.0;
  edge.view.black_per_channel = {0.0, 0.0, 0.0, 0.0};
  edge.view.samples = std::span<const double>(edge.samples);
  return edge;
}

SyntheticEdge synthetic_ringing_green_edge(double amplitude = 200.0,
                                            double frequency = 2.0,
                                            double decay = 8.0) {
  constexpr int width = 160;
  constexpr int height = 144;
  constexpr double sigma = 0.4;
  constexpr double angle_deg = -6.0;
  SyntheticEdge edge;
  edge.samples.resize(static_cast<std::size_t>(width * height));

  const double slope = std::tan(angle_deg * std::numbers::pi / 180.0);
  const double cx = 0.5 * static_cast<double>(width - 1);
  const double cy = 0.5 * static_cast<double>(height - 1);
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      const double edge_y = cy + slope * (static_cast<double>(x) - cx);
      const double distance =
          (static_cast<double>(y) - edge_y) / std::sqrt(1.0 + slope * slope);
      const double base = 10000.0 * erf_edge(distance, sigma);
      const double ring =
          amplitude * std::sin(2.0 * std::numbers::pi * frequency * distance) *
          std::exp(-std::abs(distance) / decay);
      edge.samples[static_cast<std::size_t>(y * width + x)] = base + ring;
    }
  }

  edge.view.width = width;
  edge.view.height = height;
  edge.view.row_stride_pixels = width;
  edge.view.cdesc = "RGBG";
  edge.view.color_at_position = {0, 1, 1, 2};
  edge.view.white_level = 65535.0;
  edge.view.black_per_channel = {0.0, 0.0, 0.0, 0.0};
  edge.view.samples = std::span<const double>(edge.samples);
  return edge;
}

using camera_iq::RoiRect;

void test_dft_and_difference_response() {
  const auto magnitude = camera_iq::dft_magnitude({1.0, 2.0, 3.0, 4.0});
  check(magnitude.size() == 3, "DFT returns only the non-redundant bins");

  // A pure cosine must concentrate in exactly one bin. Anything else means the
  // bin-to-frequency mapping is wrong, which would shift every reported MTF.
  constexpr int kLength = 32;
  constexpr int kBin = 4;
  std::vector<double> cosine(kLength);
  for (int i = 0; i < kLength; ++i) {
    cosine[static_cast<std::size_t>(i)] = std::cos(
        2.0 * std::numbers::pi * kBin * static_cast<double>(i) / kLength);
  }
  const auto spectrum = camera_iq::dft_magnitude(cosine);
  double largest_other = 0.0;
  for (std::size_t i = 0; i < spectrum.size(); ++i) {
    if (i != static_cast<std::size_t>(kBin)) {
      largest_other = std::max(largest_other, spectrum[i]);
    }
  }
  check(spectrum[kBin] > 15.9, "cosine energy lands in its own bin");
  check(largest_other < 1e-10, "cosine energy does not leak into other bins");

  // Differentiating a binned signal attenuates high frequencies by a known
  // amount. Pinning three non-zero frequencies checks the formula rather than
  // only its sign and monotonic direction.
  constexpr double delta = 0.25;
  check_near(camera_iq::adjacent_difference_response(0.0, delta), 1.0, 1e-12,
             "difference response is unity at DC");
  check_near(camera_iq::adjacent_difference_response(0.25, delta), 0.993587,
             1e-6, "difference response at 0.25 cycles/pixel");
  check_near(camera_iq::adjacent_difference_response(0.5, delta), 0.974495,
             1e-6, "difference response at sensor Nyquist");
  check_near(camera_iq::adjacent_difference_response(1.0, delta), 0.900316,
             1e-6, "difference response at 1.0 cycles/pixel");
}

void test_horizontal_edge_against_the_gaussian_oracle() {
  constexpr double sigma = 1.25;
  auto edge = synthetic_green_edge(160, 144, -6.0, sigma, true);
  const auto result =
      camera_iq::analyze_green_sfr(edge.view, RoiRect{20, 16, 120, 112});

  check(result.accepted, "synthetic horizontal green edge accepted");
  check(result.orientation == "horizontal", "horizontal orientation detected");
  check_near(result.edge_angle_deg, -6.0, 0.08, "edge angle recovered");
  check_near(result.mtf50_cy_per_px, 0.18739 / sigma, 0.018,
             "MTF50 matches the Gaussian oracle");
  check_near(result.mtf50p_cy_per_px, 0.18739 / sigma, 0.018,
             "MTF50P matches the Gaussian oracle");
  // Bounded on both sides: a blur this broad must suppress Nyquist, but an
  // implementation returning exactly zero is also wrong and is rejected here.
  check(result.mtf_at_nyquist > 0.0 && result.mtf_at_nyquist < 0.01,
        "a broad Gaussian strongly suppresses, but does not null, Nyquist");
}

void test_vertical_edge() {
  auto edge = synthetic_green_edge(144, 160, 5.5, 1.4, false);
  const auto result =
      camera_iq::analyze_green_sfr(edge.view, RoiRect{16, 20, 112, 120});
  check(result.accepted, "synthetic vertical green edge accepted");
  check(result.orientation == "vertical", "vertical orientation detected");
  check_near(result.edge_angle_deg, 5.5, 0.08, "vertical edge angle recovered");
}

void test_narrow_gaussian_nyquist_accuracy() {
  constexpr double sigma = 0.5;
  auto edge = synthetic_green_edge(160, 144, -6.0, sigma, true);
  const auto result =
      camera_iq::analyze_green_sfr(edge.view, RoiRect{20, 16, 120, 112});
  const double expected = std::exp(-2.0 * std::numbers::pi *
                                   std::numbers::pi * sigma * sigma * 0.25);
  check(result.accepted, "narrow Gaussian edge accepted");
  check_near(result.mtf_at_nyquist, expected, 0.03,
             "narrow Gaussian Nyquist response matches its oracle");
}

void test_first_frequency_interval_crossings_are_retained() {
  auto edge = synthetic_green_edge(64, 64, 6.0, 12.0, true);
  const auto result =
      camera_iq::analyze_green_sfr(edge.view, RoiRect{20, 20, 24, 24});
  check(result.accepted, "broad edge with a first-interval crossing accepted");
  check(result.mtf_frequency_cy_per_px.size() > 1 &&
            result.mtf50_cy_per_px > 0.0 &&
            result.mtf50_cy_per_px < result.mtf_frequency_cy_per_px[1],
        "MTF50 crossing between DC and the first non-DC bin retained");
  check(result.mtf_frequency_cy_per_px.size() > 1 &&
            result.mtf50p_cy_per_px > 0.0 &&
            result.mtf50p_cy_per_px < result.mtf_frequency_cy_per_px[1],
        "MTF50P crossing between DC and the first non-DC bin retained");
}

void test_missing_post_peak_crossing_is_refused() {
  auto edge = synthetic_ringing_green_edge();
  const auto result =
      camera_iq::analyze_green_sfr(edge.view, RoiRect{20, 16, 120, 112});
  check(!result.mtf.empty(), "ringing refusal fixture produces an MTF curve");
  check(!result.accepted && result.rejection_reason == "mtf50p_not_found",
        "missing post-peak MTF50P crossing is refused by name");
}

void test_unsampled_nyquist_is_refused() {
  auto edge = synthetic_green_edge(160, 144, -6.0, 1.25, true);
  camera_iq::SfrOptions options;
  options.bin_spacing_px = 2.0;
  const auto result = camera_iq::analyze_green_sfr(
      edge.view, RoiRect{20, 16, 120, 112}, options);
  check(!result.mtf_frequency_cy_per_px.empty() &&
            result.mtf_frequency_cy_per_px.back() < 0.5,
        "coarse-grid fixture ends below sensor Nyquist");
  check(!result.accepted && result.rejection_reason == "nyquist_not_sampled",
        "an unsampled Nyquist value is refused rather than extrapolated");
}

void test_translation_stability() {
  constexpr double sigma = 1.25;
  const RoiRect roi{20, 16, 120, 112};
  auto centered = synthetic_green_edge(160, 144, -6.0, sigma, true);
  auto upper = synthetic_green_edge(160, 144, -6.0, sigma, true, -18.0);
  auto lower = synthetic_green_edge(160, 144, -6.0, sigma, true, 18.0);

  const auto a = camera_iq::analyze_green_sfr(centered.view, roi);
  const auto b = camera_iq::analyze_green_sfr(upper.view, roi);
  const auto c = camera_iq::analyze_green_sfr(lower.view, roi);
  check(a.accepted && b.accepted && c.accepted,
        "translated edges with two-sided support are accepted");
  check_near(b.mtf50_cy_per_px, a.mtf50_cy_per_px, 0.003,
             "MTF50 is stable as the edge moves toward the ROI top");
  check_near(c.mtf50_cy_per_px, a.mtf50_cy_per_px, 0.003,
             "MTF50 is stable as the edge moves toward the ROI bottom");
}

void test_insufficient_edge_support_is_refused() {
  // Pushed far enough toward the ROI edge that one side of the transition is
  // no longer measured. A Fourier estimate needs plateaus on both sides, so
  // this must refuse rather than report a plausible-looking number.
  auto truncated = synthetic_green_edge(160, 144, -6.0, 5.0, true, 44.0);
  const auto result =
      camera_iq::analyze_green_sfr(truncated.view, RoiRect{20, 16, 120, 112});
  check(!result.accepted, "an edge without two-sided support is refused");
  check(result.rejection_reason == "insufficient_edge_support",
        "the refusal names insufficient edge support");
}

void test_low_contrast_is_refused() {
  auto flat = synthetic_green_edge(160, 144, -6.0, 1.25, true);
  for (double& sample : flat.samples) sample = 500.0;
  flat.view.samples = std::span<const double>(flat.samples);
  const auto result =
      camera_iq::analyze_green_sfr(flat.view, RoiRect{20, 16, 120, 112});
  check(!result.accepted && result.rejection_reason == "low_contrast",
        "a field with no edge is refused as low contrast");
}

void test_near_saturation_is_refused() {
  auto edge = synthetic_green_edge(96, 96, 6.0, 1.0, true);
  camera_iq::SfrOptions options;
  options.near_saturation_fraction = 0.2;
  const auto result = camera_iq::analyze_green_sfr(
      edge.view, RoiRect{8, 8, 80, 80}, options);
  check(!result.accepted && result.rejection_reason == "roi_saturated",
        "a near-saturated ROI is refused by name");
  check(result.green_sample_count > 0 && result.saturated_fraction > 0.0 &&
            result.contrast_dn > 0.0,
        "saturation refusal retains measured diagnostics");
}

void test_invalid_geometry_is_refused() {
  auto edge = synthetic_green_edge(160, 144, -6.0, 1.25, true);
  const auto outside =
      camera_iq::analyze_green_sfr(edge.view, RoiRect{2000, 2000, 40, 40});
  check(!outside.accepted && outside.rejection_reason == "roi_too_small",
        "an ROI outside the image is refused by name");

  const auto degenerate =
      camera_iq::analyze_green_sfr(edge.view, RoiRect{20, 16, 0, 0});
  check(!degenerate.accepted && degenerate.rejection_reason == "roi_too_small",
        "a zero-area ROI is refused by name");
}

void test_invalid_image_view_is_refused() {
  auto bad_stride = synthetic_green_edge(160, 144, -6.0, 1.25, true);
  bad_stride.view.row_stride_pixels = bad_stride.view.width - 1;
  const auto stride_result = camera_iq::analyze_green_sfr(
      bad_stride.view, RoiRect{20, 16, 120, 112});
  check(!stride_result.accepted &&
            stride_result.rejection_reason == "invalid_raw_image",
        "a row stride shorter than the image width is refused");

  auto short_buffer = synthetic_green_edge(160, 144, -6.0, 1.25, true);
  short_buffer.view.samples = std::span<const double>(
      short_buffer.samples.data(), short_buffer.samples.size() - 1);
  const auto buffer_result = camera_iq::analyze_green_sfr(
      short_buffer.view, RoiRect{20, 16, 120, 112});
  check(!buffer_result.accepted &&
            buffer_result.rejection_reason == "invalid_raw_image",
        "a sample span shorter than the declared geometry is refused");
}

void test_shallow_edge_angle_is_refused() {
  auto edge = synthetic_green_edge(96, 96, 0.4, 1.0, true);
  const auto result =
      camera_iq::analyze_green_sfr(edge.view, RoiRect{8, 8, 80, 80});
  check(!result.accepted &&
            result.rejection_reason == "edge_angle_out_of_range",
        "a too-shallow edge angle is refused by name");
  check_near(result.edge_angle_deg, 0.4, 0.08,
             "angle refusal retains the measured angle");
}

void test_non_finite_samples_are_refused() {
  auto edge = synthetic_green_edge(160, 144, -6.0, 1.25, true);
  // Must land on a green position inside the ROI, or the estimator never reads
  // it: position index is (y & 1) * 2 + (x & 1), and green sits at 1 and 2.
  constexpr int kX = 51;
  constexpr int kY = 50;
  static_assert(((kY & 1) * 2 + (kX & 1)) == 1, "chosen sample must be green");
  edge.samples[static_cast<std::size_t>(kY * 160 + kX)] =
      std::numeric_limits<double>::quiet_NaN();
  edge.view.samples = std::span<const double>(edge.samples);
  const auto result =
      camera_iq::analyze_green_sfr(edge.view, RoiRect{20, 16, 120, 112});
  check(!result.accepted && result.rejection_reason == "invalid_raw_image",
        "a non-finite sample is refused as an invalid image");
}

void test_cfa_balanced_roi_rule() {
  // Green sits at two of the four mosaic positions, so an ROI that does not
  // cover whole 2x2 blocks would weight one green position more than the
  // other. The rule snaps to complete blocks instead.
  const auto balanced = camera_iq::cfa_balanced_roi(RoiRect{3, 5, 21, 19}, 160, 144);
  check(balanced.has_value(), "an interior odd ROI resolves to a balanced one");
  if (balanced) {
    check(balanced->x % 2 == 0 && balanced->y % 2 == 0,
          "balanced ROI starts on a mosaic block boundary");
    check(balanced->width % 2 == 0 && balanced->height % 2 == 0,
          "balanced ROI spans whole mosaic blocks");
  }
  check(!camera_iq::cfa_balanced_roi(RoiRect{0, 0, 1, 1}, 160, 144).has_value(),
        "an ROI smaller than one mosaic block has no balanced form");
}

}  // namespace

int main() {
  test_dft_and_difference_response();
  test_horizontal_edge_against_the_gaussian_oracle();
  test_vertical_edge();
  test_narrow_gaussian_nyquist_accuracy();
  test_first_frequency_interval_crossings_are_retained();
  test_missing_post_peak_crossing_is_refused();
  test_unsampled_nyquist_is_refused();
  test_translation_stability();
  test_insufficient_edge_support_is_refused();
  test_low_contrast_is_refused();
  test_near_saturation_is_refused();
  test_invalid_geometry_is_refused();
  test_invalid_image_view_is_refused();
  test_shallow_edge_angle_is_refused();
  test_non_finite_samples_are_refused();
  test_cfa_balanced_roi_rule();

  if (failures != 0) {
    std::cerr << failures << " slanted-edge test(s) failed\n";
    return EXIT_FAILURE;
  }
  std::cout << "slanted-edge SFR tests passed\n";
  return EXIT_SUCCESS;
}
