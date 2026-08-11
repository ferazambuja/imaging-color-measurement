// Public tests for the decoder-independent CFA flat-field estimator.
//
// Every mosaic here is synthetic. The fixtures separate headroom, spatial
// response, chromatic response, and comparison behavior without requiring a
// RAW decoder, an archive path, or a camera capture.

#include "camera_iq/flat_field_gate.hpp"
#include "camera_iq/shading.hpp"

#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

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

constexpr int kMosaic = 128;
constexpr int kGrid = 4;
using Bins = std::array<std::array<double, kGrid>, kGrid>;

const std::array<int, 4> kRGGB{0, 1, 3, 2};
const std::array<double, 4> kCeiling{1000.0, 1000.0, 1000.0, 1000.0};
const std::array<double, 4> kHeadroom{4000.0, 4000.0, 4000.0, 4000.0};

std::vector<double> make_binned_mosaic(const std::array<double, 4>& amplitude,
                                       const std::array<const Bins*, 4>& field) {
  std::vector<double> mosaic(
      static_cast<std::size_t>(kMosaic) * kMosaic, 0.0);
  for (int y = 0; y < kMosaic; ++y) {
    for (int x = 0; x < kMosaic; ++x) {
      const int position = (y % 2) * 2 + (x % 2);
      const int row = (y / 2) / (kMosaic / 2 / kGrid);
      const int col = (x / 2) / (kMosaic / 2 / kGrid);
      mosaic[static_cast<std::size_t>(y) * kMosaic + x] =
          amplitude[position] * (*field[position])[row][col];
    }
  }
  return mosaic;
}

bool map_matches(const std::vector<double>& actual, const Bins& expected,
                 double tolerance) {
  if (actual.size() != static_cast<std::size_t>(kGrid * kGrid)) return false;
  for (int row = 0; row < kGrid; ++row) {
    for (int col = 0; col < kGrid; ++col) {
      const std::size_t index = static_cast<std::size_t>(row * kGrid + col);
      if (std::abs(actual[index] - expected[row][col]) > tolerance) {
        return false;
      }
    }
  }
  return true;
}

camera_iq::ShadingOptions small_options() {
  camera_iq::ShadingOptions options;
  options.grid_cols = kGrid;
  options.grid_rows = kGrid;
  options.corner_block_px = 32;
  options.corner_inset_px = 0;
  options.gate_center_frac = 0.5;
  return options;
}

void test_headroom_and_cfa_geometry() {
  constexpr int width = 20;
  constexpr int height = 20;
  const camera_iq::RoiRect full{0, 0, width, height};
  std::vector<double> mosaic(static_cast<std::size_t>(width) * height, 50.0);

  // One 2x2 block contributes exactly one of 100 samples to every CFA
  // position. The declared one-percent policy is inclusive.
  for (int y = 0; y < 2; ++y) {
    for (int x = 0; x < 2; ++x) {
      mosaic[static_cast<std::size_t>(y) * width + x] = 990.0;
    }
  }
  auto measured = camera_iq::measure_cfa_near_ceiling(
      mosaic.data(), width, height, width, full, kCeiling, 0.98);
  check(measured.has_value(), "balanced full-frame gate is measurable");
  for (std::size_t position = 0; position < 4; ++position) {
    check_near(measured->fraction_frame[position], 0.01, 1e-12,
               "one of 100 samples measures exactly one percent");
    check(camera_iq::flat_field_near_ceiling_passes(
              measured->fraction_frame[position],
              measured->fraction_gate[position],
              measured->finite_fraction_frame[position],
              measured->finite_fraction_gate[position], 0.01, 0.90),
          "one-percent boundary is accepted inclusively");
  }

  for (int y = 2; y < 4; ++y) {
    for (int x = 0; x < 2; ++x) {
      mosaic[static_cast<std::size_t>(y) * width + x] = 990.0;
    }
  }
  measured = camera_iq::measure_cfa_near_ceiling(
      mosaic.data(), width, height, width, full, kCeiling, 0.98);
  for (std::size_t position = 0; position < 4; ++position) {
    check_near(measured->fraction_frame[position], 0.02, 1e-12,
               "two of 100 samples measures exactly two percent");
    check(!camera_iq::flat_field_near_ceiling_passes(
              measured->fraction_frame[position],
              measured->fraction_gate[position],
              measured->finite_fraction_frame[position],
              measured->finite_fraction_gate[position], 0.01, 0.90),
          "two-percent plane is rejected by the one-percent policy");
  }

  // Ten complete 2x2 blocks remove exactly ten of 100 samples from each CFA
  // position. Coverage is inclusive at 90%; one more block falls below it.
  std::fill(mosaic.begin(), mosaic.end(), 50.0);
  const double missing = std::numeric_limits<double>::quiet_NaN();
  for (int block = 0; block < 10; ++block) {
    const int x0 = 2 * block;
    for (int y = 0; y < 2; ++y) {
      for (int x = x0; x < x0 + 2; ++x) {
        mosaic[static_cast<std::size_t>(y) * width + x] = missing;
      }
    }
  }
  measured = camera_iq::measure_cfa_near_ceiling(
      mosaic.data(), width, height, width, full, kCeiling, 0.98);
  for (std::size_t position = 0; position < 4; ++position) {
    check_near(measured->finite_fraction_frame[position], 0.90, 1e-12,
               "ten missing samples measure exactly 90 percent coverage");
    check(camera_iq::flat_field_near_ceiling_passes(
              measured->fraction_frame[position],
              measured->fraction_gate[position],
              measured->finite_fraction_frame[position],
              measured->finite_fraction_gate[position], 0.01, 0.90),
          "90-percent coverage boundary is accepted inclusively");
  }
  for (int y = 2; y < 4; ++y) {
    for (int x = 0; x < 2; ++x) {
      mosaic[static_cast<std::size_t>(y) * width + x] = missing;
    }
  }
  measured = camera_iq::measure_cfa_near_ceiling(
      mosaic.data(), width, height, width, full, kCeiling, 0.98);
  for (std::size_t position = 0; position < 4; ++position) {
    check_near(measured->finite_fraction_frame[position], 0.89, 1e-12,
               "eleven missing samples measure 89 percent coverage");
    check(!camera_iq::flat_field_near_ceiling_passes(
              measured->fraction_frame[position],
              measured->fraction_gate[position],
              measured->finite_fraction_frame[position],
              measured->finite_fraction_gate[position], 0.01, 0.90),
          "coverage below 90 percent is rejected");
  }

  const std::vector<double> odd(static_cast<std::size_t>(19) * height, 50.0);
  std::fill(mosaic.begin(), mosaic.end(), 50.0);
  check(!camera_iq::measure_cfa_near_ceiling(
             odd.data(), 19, height, 19, {0, 0, 18, height}, kCeiling, 0.98)
             .has_value(),
        "odd active width is refused rather than trimmed");
  check(!camera_iq::measure_cfa_near_ceiling(
             mosaic.data(), width, height, width, {1, 2, 4, 4}, kCeiling,
             0.98)
             .has_value(),
        "odd-origin gate is refused rather than re-aligned");
  check(!camera_iq::measure_cfa_near_ceiling(
             mosaic.data(), width, height, width, {18, 2, 4, 4}, kCeiling,
             0.98)
             .has_value(),
        "gate must equal its clipped CFA-balanced rectangle");
}

void test_spatial_and_chromatic_maps() {
  const Bins luminance{{{0.50, 0.60, 0.70, 0.80},
                        {0.55, 1.00, 1.00, 0.85},
                        {0.65, 1.00, 1.00, 0.90},
                        {0.75, 0.95, 0.98, 0.99}}};
  const Bins red_factor{{{0.980, 0.985, 0.990, 0.975},
                         {0.982, 1.000, 1.000, 0.978},
                         {0.984, 1.000, 1.000, 0.976},
                         {0.986, 0.988, 0.992, 0.974}}};
  const Bins blue_factor{{{1.030, 1.035, 1.040, 1.025},
                          {1.032, 1.000, 1.000, 1.028},
                          {1.034, 1.000, 1.000, 1.026},
                          {1.036, 1.038, 1.042, 1.024}}};
  const Bins unity{{{1.0, 1.0, 1.0, 1.0},
                     {1.0, 1.0, 1.0, 1.0},
                     {1.0, 1.0, 1.0, 1.0},
                     {1.0, 1.0, 1.0, 1.0}}};

  Bins red_field = luminance;
  Bins blue_field = luminance;
  for (int row = 0; row < kGrid; ++row) {
    for (int col = 0; col < kGrid; ++col) {
      red_field[row][col] *= red_factor[row][col];
      blue_field[row][col] *= blue_factor[row][col];
    }
  }
  const std::array<double, 4> amplitude{1000.0, 2000.0, 2100.0, 800.0};
  const std::array<const Bins*, 4> fields{&red_field, &luminance, &luminance,
                                          &blue_field};
  const auto mosaic = make_binned_mosaic(amplitude, fields);
  const auto options = small_options();
  const auto response = camera_iq::measure_shading_field(
      mosaic.data(), kMosaic, kMosaic, kMosaic, options, kHeadroom);
  const auto chromatic =
      camera_iq::chromatic_response(response, kRGGB, "RGBG", options);

  check(response.valid && response.geometry.valid,
        "synthetic binned field is accepted with explicit geometry");
  check(map_matches(response.relative[1], luminance, 1e-12),
        "green spatial field is recovered exactly");
  check(chromatic.valid && chromatic.complete,
        "complete Bayer chromatic maps are declared complete");
  check(map_matches(chromatic.c_rg, red_factor, 1e-12),
        "center-normalized red-to-green field is recovered exactly");
  check(map_matches(chromatic.c_bg, blue_factor, 1e-12),
        "center-normalized blue-to-green field is recovered exactly");
  check(map_matches(chromatic.c_g1g2, unity, 1e-12),
        "a constant green gain difference cancels after normalization");

  Bins dead_green = luminance;
  dead_green[0][0] = 0.0;
  const std::array<const Bins*, 4> incomplete_fields{
      &luminance, &luminance, &dead_green, &luminance};
  const auto incomplete_mosaic = make_binned_mosaic(amplitude, incomplete_fields);
  const auto incomplete_response = camera_iq::measure_shading_field(
      incomplete_mosaic.data(), kMosaic, kMosaic, kMosaic, options, kHeadroom);
  const auto incomplete = camera_iq::chromatic_response(
      incomplete_response, kRGGB, "RGBG", options);
  check(incomplete.valid && !incomplete.complete &&
            incomplete.missing_bin_count == 1,
        "one undefined ratio makes the accepted document explicitly incomplete");
  check(std::isnan(incomplete.c_g1g2[0]),
        "undefined chromatic ratio is NaN rather than zero or infinity");
}

void test_equal_radius_asymmetry() {
  constexpr int width = 256;
  constexpr int height = 256;
  std::vector<double> radial(static_cast<std::size_t>(width) * height, 0.0);
  std::vector<double> tilted(radial.size(), 0.0);
  const double cx = (width - 1) / 2.0;
  const double cy = (height - 1) / 2.0;
  const double radius_max = std::sqrt(cx * cx + cy * cy);
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      const double dx = x - cx;
      const double dy = y - cy;
      const double radius = std::sqrt(dx * dx + dy * dy) / radius_max;
      const double base = 1000.0 * (1.0 - 0.3 * radius * radius);
      const int position = (y % 2) * 2 + (x % 2);
      const bool green = position == 1 || position == 2;
      const std::size_t index = static_cast<std::size_t>(y) * width + x;
      radial[index] = base;
      tilted[index] = base * (1.0 + (green ? 0.20 : -0.20) * dx / cx);
    }
  }

  auto options = small_options();
  options.corner_inset_px = 32;
  const auto symmetric = camera_iq::measure_shading_field(
      radial.data(), width, height, width, options, kHeadroom);
  const auto symmetric_chromatic =
      camera_iq::chromatic_response(symmetric, kRGGB, "RGBG", options);
  check(symmetric.valid && symmetric_chromatic.asymmetry_valid,
        "centered radial fixture is measurable");
  check(symmetric_chromatic.green_asymmetry < 1e-3,
        "sampled radial fixture stays below its fixture-specific bound");
  check(!symmetric_chromatic.asymmetry_exceeds_policy,
        "centered radial fixture stays below the declared 0.05 policy");

  const auto asymmetric = camera_iq::measure_shading_field(
      tilted.data(), width, height, width, options, kHeadroom);
  const auto asymmetric_chromatic =
      camera_iq::chromatic_response(asymmetric, kRGGB, "RGBG", options);
  check(asymmetric_chromatic.green_asymmetry > 0.05 &&
            asymmetric_chromatic.asymmetry_exceeds_policy,
        "off-center green field exceeds the declared asymmetry policy");
  check(asymmetric.blocks.corner_relative[1][1] >
            asymmetric.blocks.corner_relative[0][1],
        "separate corner values retain the direction of the gradient");
}

void test_comparison_and_refusals() {
  constexpr int width = 128;
  constexpr int height = 128;
  std::vector<double> first(static_cast<std::size_t>(width) * height, 0.0);
  std::vector<double> scaled(first.size(), 0.0);
  const double cx = (width - 1) / 2.0;
  const double cy = (height - 1) / 2.0;
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      const double dx = (x - cx) / cx;
      const double dy = (y - cy) / cy;
      const std::size_t index = static_cast<std::size_t>(y) * width + x;
      first[index] = 1000.0 * (1.0 - 0.20 * (dx * dx + dy * dy));
      scaled[index] = 0.5 * first[index];
    }
  }
  auto options = small_options();
  options.corner_inset_px = 16;
  const auto a = camera_iq::measure_shading_field(
      first.data(), width, height, width, options, kHeadroom);
  const auto b = camera_iq::measure_shading_field(
      scaled.data(), width, height, width, options, kHeadroom);
  const auto comparison = camera_iq::compare_shading_fields(a, b);
  check(a.valid && b.valid && comparison.measured,
        "matching accepted fields can be compared");
  check_near(comparison.max_corner_delta_pp, 0.0, 1e-9,
             "multiplicative exposure change cancels after center normalization");

  std::vector<double> offset = first;
  for (double& value : offset) value += 100.0;
  const auto c = camera_iq::measure_shading_field(
      offset.data(), width, height, width, options, kHeadroom);
  const auto additive = camera_iq::compare_shading_fields(a, c);
  check(additive.max_corner_delta_pp > 0.1 &&
            additive.rms_corner_delta_pp > 0.0,
        "additive pedestal change survives center normalization");

  const auto odd = camera_iq::measure_shading_field(
      first.data(), width - 1, height, width, options, kHeadroom);
  check(!odd.valid && !odd.gates.measured,
        "odd mosaic geometry is refused before derived measurements");

  auto invalid = options;
  invalid.asymmetry_policy = 10.01;
  check(!camera_iq::shading_options_valid(invalid),
        "out-of-range asymmetry policy is refused");

  std::array<double, 16> dark{};
  for (std::size_t i = 0; i < dark.size(); ++i) {
    dark[i] = static_cast<double>(i % 4) - 1.0;
  }
  const auto pedestal = camera_iq::measure_pedestal(dark.data(), 4, 4, 4);
  check(pedestal.measured && pedestal.full_finite_coverage,
        "finite dark mosaic produces a bounded pedestal measurement");
  check_near(pedestal.residual_dn[0], 1.0, 1e-12,
             "upper-median pedestal is recovered for CFA position 0");
  check_near(pedestal.residual_dn[1], 2.0, 1e-12,
             "upper-median pedestal is recovered for CFA position 1");
  check_near(pedestal.residual_dn[2], 1.0, 1e-12,
             "upper-median pedestal is recovered for CFA position 2");
  check_near(pedestal.residual_dn[3], 2.0, 1e-12,
             "upper-median pedestal is recovered for CFA position 3");
  check_near(pedestal.max_abs_residual_dn, 2.0, 1e-12,
             "pedestal maximum is measured rather than defaulted");
}

}  // namespace

int main() {
  test_headroom_and_cfa_geometry();
  test_spatial_and_chromatic_maps();
  test_equal_radius_asymmetry();
  test_comparison_and_refusals();
  if (failures == 0) {
    std::cout << "flat-field reference tests passed\n";
    return EXIT_SUCCESS;
  }
  std::cerr << failures << " flat-field reference test(s) failed\n";
  return EXIT_FAILURE;
}
