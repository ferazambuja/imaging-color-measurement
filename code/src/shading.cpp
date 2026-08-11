#include "camera_iq/shading.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace camera_iq {
namespace {

// Upper median. The convention is deterministic and documented in the report;
// flat-field bins are large enough that the even-count convention is immaterial.
double median_of(std::vector<double>& values) {
  if (values.empty()) return std::numeric_limits<double>::quiet_NaN();
  const std::size_t mid = values.size() / 2;
  std::nth_element(values.begin(),
                   values.begin() + static_cast<std::ptrdiff_t>(mid),
                   values.end());
  return values[mid];
}

int plane_extent(int extent, int phase) {
  return (extent - phase + 1) / 2;
}

void bin_bounds(int extent, int count, int index, int& lo, int& hi) {
  lo = static_cast<int>(static_cast<long long>(extent) * index / count);
  hi = static_cast<int>(static_cast<long long>(extent) * (index + 1) / count);
}

bool finite_closed(double value, double lo, double hi) {
  return std::isfinite(value) && value >= lo && value <= hi;
}

bool same_rect(const RoiRect& a, const RoiRect& b) {
  return a.x == b.x && a.y == b.y && a.width == b.width &&
         a.height == b.height;
}

bool contains_rect(const RoiRect& outer, const RoiRect& inner) {
  return inner.x >= outer.x && inner.y >= outer.y &&
         inner.x + inner.width <= outer.x + outer.width &&
         inner.y + inner.height <= outer.y + outer.height;
}

bool overlaps(const RoiRect& a, const RoiRect& b) {
  return a.x < b.x + b.width && b.x < a.x + a.width &&
         a.y < b.y + b.height && b.y < a.y + a.height;
}

ShadingField rejected(std::string reason, const ShadingOptions& opts) {
  ShadingField field;
  field.rejection_reason = std::move(reason);
  field.options = opts;
  field.grid_cols = opts.grid_cols;
  field.grid_rows = opts.grid_rows;
  return field;
}

}  // namespace

bool shading_options_valid(const ShadingOptions& opts) {
  return opts.grid_cols >= 1 && opts.grid_rows >= 1 &&
         static_cast<long long>(opts.grid_cols) * opts.grid_rows <= 1000000LL &&
         finite_closed(opts.gate_center_frac, 0.0, 1.0) &&
         opts.gate_center_frac > 0.0 && opts.corner_block_px >= 2 &&
         opts.corner_inset_px >= 0 &&
         opts.corner_inset_px < std::numeric_limits<int>::max() &&
         finite_closed(opts.near_ceiling_level, 0.0, 1.0) &&
         opts.near_ceiling_level > 0.0 &&
         finite_closed(opts.near_ceiling_max, 0.0, 1.0) &&
         finite_closed(opts.min_center_signal, 0.0, 1.0) &&
         finite_closed(opts.max_negative_frac, 0.0, 1.0) &&
         finite_closed(opts.min_bin_coverage, 0.0, 1.0) &&
         opts.min_bin_coverage > 0.0 &&
         finite_closed(opts.asymmetry_policy, 0.0, 10.0);
}

std::optional<ShadingGeometry> make_shading_geometry(
    int width, int height, const ShadingOptions& opts) {
  // Use even mosaic dimensions and origins so every region contains the same
  // number of samples from all four CFA positions. Odd requests round inward;
  // the effective rectangles are published with the result.
  // Mirrored corner geometry is exact only on an even mosaic. Rejecting odd
  // dimensions is preferable to publishing an incorrect equal-radius result.
  if ((width & 1) != 0 || (height & 1) != 0) return std::nullopt;

  const int block = opts.corner_block_px & ~1;
  const int inset = (opts.corner_inset_px + 1) & ~1;
  if (block < 2 || inset < 0 ||
      2LL * (static_cast<long long>(inset) + block) > width ||
      2LL * (static_cast<long long>(inset) + block) > height) {
    return std::nullopt;
  }

  const auto gate =
      centered_cfa_balanced_roi(width, height, opts.gate_center_frac);
  if (!gate) return std::nullopt;

  const int center_x = ((width - block) / 2) & ~1;
  const int center_y = ((height - block) / 2) & ~1;
  const int right_x = (width - inset - block) & ~1;
  const int bottom_y = (height - inset - block) & ~1;

  const RoiRect center_requested{center_x, center_y, block, block};
  const std::array<RoiRect, 4> corner_requested = {
      RoiRect{inset, inset, block, block},
      RoiRect{right_x, inset, block, block},
      RoiRect{inset, bottom_y, block, block},
      RoiRect{right_x, bottom_y, block, block}};

  const auto center = cfa_balanced_roi(center_requested, width, height);
  if (!center || !same_rect(*center, center_requested)) {
    return std::nullopt;
  }

  ShadingGeometry geometry;
  geometry.gate = *gate;
  geometry.center = *center;
  for (int q = 0; q < 4; ++q) {
    const auto corner = cfa_balanced_roi(corner_requested[q], width, height);
    if (!corner || !same_rect(*corner, corner_requested[q])) {
      return std::nullopt;
    }
    geometry.corners[q] = *corner;
  }
  if (!contains_rect(geometry.gate, geometry.center)) return std::nullopt;
  for (const RoiRect& corner : geometry.corners) {
    if (overlaps(geometry.center, corner)) return std::nullopt;
  }
  geometry.valid = true;
  return geometry;
}

ShadingField measure_shading_field(const double* data, int width, int height,
                                   int row_stride_pixels,
                                   const ShadingOptions& opts,
                                   const std::array<double, 4>& ceiling) {
  if (data == nullptr || width < 2 || height < 2 || row_stride_pixels < width) {
    return rejected("invalid mosaic buffer", opts);
  }
  if (!shading_options_valid(opts)) {
    return rejected("invalid shading options", opts);
  }
  if (opts.grid_cols > plane_extent(width, 1) ||
      opts.grid_rows > plane_extent(height, 1) ||
      static_cast<long long>(opts.grid_cols) * opts.grid_rows > 1000000LL) {
    return rejected("grid does not fit the CFA planes", opts);
  }
  for (const double value : ceiling) {
    if (!std::isfinite(value) || value <= 0.0) {
      return rejected("invalid signal-referred ceiling", opts);
    }
  }

  ShadingField field;
  field.options = opts;
  field.grid_cols = opts.grid_cols;
  field.grid_rows = opts.grid_rows;
  field.signal_ceiling = ceiling;
  field.signal_ceiling_measured = true;

  const auto geometry = make_shading_geometry(width, height, opts);
  if (!geometry) {
    field.rejection_reason =
        "requested CFA-balanced blocks do not fit the image";
    return field;
  }

  field.geometry = *geometry;
  field.gates.min_bin_coverage = 1.0;
  bool aggregates_finite = true;

  const auto near_ceiling = measure_cfa_near_ceiling(
      data, width, height, row_stride_pixels, field.geometry.gate, ceiling,
      opts.near_ceiling_level);
  if (!near_ceiling) {
    field.rejection_reason = "near-ceiling measurement is undefined";
    return field;
  }
  field.gates.near_ceiling_frac_frame = near_ceiling->fraction_frame;
  field.gates.finite_frac_gate = near_ceiling->finite_fraction_gate;
  field.gates.finite_frac_frame = near_ceiling->finite_fraction_frame;
  field.gates.near_ceiling_frac_gate = near_ceiling->fraction_gate;

  for (int p = 0; p < 4; ++p) {
    const int dy = p / 2;
    const int dx = p % 2;
    const int plane_width = plane_extent(width, dx);
    const int plane_height = plane_extent(height, dy);
    if (plane_width < opts.grid_cols || plane_height < opts.grid_rows) {
      field.rejection_reason = "grid does not fit every CFA plane";
      return field;
    }

    const auto sample = [&](int plane_y, int plane_x) {
      const std::size_t row =
          static_cast<std::size_t>(2 * plane_y + dy) * row_stride_pixels;
      return data[row + static_cast<std::size_t>(2 * plane_x + dx)];
    };
    const auto region_samples = [&](const RoiRect& roi) {
      std::vector<double> values;
      values.reserve(static_cast<std::size_t>(roi.width / 2) *
                     static_cast<std::size_t>(roi.height / 2));
      for (int y = roi.y + dy; y < roi.y + roi.height; y += 2) {
        for (int x = roi.x + dx; x < roi.x + roi.width; x += 2) {
          const double value = data[static_cast<std::size_t>(y) *
                                        row_stride_pixels +
                                    static_cast<std::size_t>(x)];
          if (std::isfinite(value)) values.push_back(value);
        }
      }
      return values;
    };

    std::vector<double> center = region_samples(field.geometry.center);
    if (center.empty()) aggregates_finite = false;
    field.center_block_median[p] = median_of(center);
    field.gates.center_signal_frac[p] =
        field.center_block_median[p] / ceiling[p];

    for (int q = 0; q < 4; ++q) {
      std::vector<double> block = region_samples(field.geometry.corners[q]);
      if (block.empty()) aggregates_finite = false;
      field.blocks.corner_median[q][p] = median_of(block);
    }

    long long frame_finite = 0;
    long long frame_negative = 0;
    for (int py = 0; py < plane_height; ++py) {
      for (int px = 0; px < plane_width; ++px) {
        const double value = sample(py, px);
        if (!std::isfinite(value)) continue;
        ++frame_finite;
        if (value < 0.0) ++frame_negative;
      }
    }
    if (frame_finite == 0) aggregates_finite = false;
    field.gates.negative_frac[p] =
        frame_finite > 0 ? static_cast<double>(frame_negative) / frame_finite
                         : std::numeric_limits<double>::quiet_NaN();

    const std::size_t bin_count =
        static_cast<std::size_t>(opts.grid_cols) * opts.grid_rows;
    field.bin_median[p].assign(
        bin_count, std::numeric_limits<double>::quiet_NaN());
    std::vector<double> bin;
    for (int row = 0; row < opts.grid_rows; ++row) {
      int y0 = 0;
      int y1 = 0;
      bin_bounds(plane_height, opts.grid_rows, row, y0, y1);
      for (int col = 0; col < opts.grid_cols; ++col) {
        int x0 = 0;
        int x1 = 0;
        bin_bounds(plane_width, opts.grid_cols, col, x0, x1);
        const long long expected =
            static_cast<long long>(y1 - y0) * (x1 - x0);
        bin.clear();
        bin.reserve(static_cast<std::size_t>(expected));
        for (int py = y0; py < y1; ++py) {
          for (int px = x0; px < x1; ++px) {
            const double value = sample(py, px);
            if (std::isfinite(value)) bin.push_back(value);
          }
        }
        const double coverage =
            expected > 0 ? static_cast<double>(bin.size()) / expected : 0.0;
        field.gates.min_bin_coverage =
            std::min(field.gates.min_bin_coverage, coverage);
        const std::size_t index =
            static_cast<std::size_t>(row) * opts.grid_cols + col;
        if (bin.empty()) aggregates_finite = false;
        field.bin_median[p][index] = median_of(bin);
      }
    }
  }

  field.gates.measured = true;
  field.gates.finite_ok = aggregates_finite;
  field.gates.near_ceiling_ok = true;
  field.gates.screening_coverage_ok = true;
  field.gates.low_signal_ok = true;
  field.gates.negative_ok = true;
  for (int p = 0; p < 4; ++p) {
    field.gates.near_ceiling_ok &= flat_field_near_ceiling_fractions_pass(
        field.gates.near_ceiling_frac_frame[p],
        field.gates.near_ceiling_frac_gate[p], opts.near_ceiling_max);
    field.gates.screening_coverage_ok &= flat_field_finite_coverage_passes(
        field.gates.finite_frac_frame[p], field.gates.finite_frac_gate[p],
        kFlatFieldMinFiniteCoverage);
    field.gates.low_signal_ok &=
        field.gates.center_signal_frac[p] >= opts.min_center_signal;
    field.gates.negative_ok &=
        field.gates.negative_frac[p] <= opts.max_negative_frac;
  }
  field.gates.coverage_ok =
      field.gates.min_bin_coverage >= opts.min_bin_coverage;

  if (!field.gates.screening_coverage_ok) {
    field.rejection_reason = "screening finite coverage below policy";
    return field;
  }
  if (!field.gates.near_ceiling_ok || !field.gates.low_signal_ok ||
      !field.gates.negative_ok || !field.gates.coverage_ok ||
      !field.gates.finite_ok) {
    field.rejection_reason = "one or more quality gates failed";
    return field;
  }

  for (int p = 0; p < 4; ++p) {
    const double normalizer = field.center_block_median[p];
    if (!std::isfinite(normalizer) || normalizer <= 0.0) {
      field.rejection_reason = "center-block normalizer is not positive";
      return field;
    }
  }

  for (int p = 0; p < 4; ++p) {
    field.relative[p].resize(field.bin_median[p].size());
    for (std::size_t i = 0; i < field.bin_median[p].size(); ++i) {
      field.relative[p][i] =
          field.bin_median[p][i] / field.center_block_median[p];
    }
    for (int q = 0; q < 4; ++q) {
      field.blocks.corner_relative[q][p] =
          field.blocks.corner_median[q][p] / field.center_block_median[p];
    }
  }
  field.valid = true;
  return field;
}

ShadingChromatic chromatic_response(const ShadingField& field,
                                    const std::array<int, 4>& colors,
                                    std::string_view cdesc,
                                    const ShadingOptions& opts) {
  if (!finite_closed(opts.asymmetry_policy, 0.0, 10.0)) {
    return {};
  }

  int red = -1;
  int blue = -1;
  int green_a = -1;
  int green_b = -1;
  for (int p = 0; p < 4; ++p) {
    const int index = colors[p];
    if (index < 0 || static_cast<std::size_t>(index) >= cdesc.size()) return {};
    switch (cdesc[static_cast<std::size_t>(index)]) {
      case 'R':
        if (red >= 0) return {};
        red = p;
        break;
      case 'B':
        if (blue >= 0) return {};
        blue = p;
        break;
      case 'G':
        if (green_a < 0) {
          green_a = p;
        } else if (green_b < 0) {
          green_b = p;
        } else {
          return {};
        }
        break;
      default:
        return {};
    }
  }
  if (red < 0 || blue < 0 || green_a < 0 || green_b < 0) return {};

  ShadingChromatic out;
  out.layout_valid = true;
  out.red_position = red;
  out.green1_position = green_a;
  out.green2_position = green_b;
  out.blue_position = blue;
  if (!field.valid || field.grid_cols < 1 || field.grid_rows < 1) return out;

  const std::size_t bins =
      static_cast<std::size_t>(field.grid_cols) * field.grid_rows;
  if (field.relative[red].size() != bins ||
      field.relative[blue].size() != bins ||
      field.relative[green_a].size() != bins ||
      field.relative[green_b].size() != bins) {
    return out;
  }

  out.valid = true;
  out.complete = true;
  const double undefined = std::numeric_limits<double>::quiet_NaN();
  out.c_rg.resize(bins, undefined);
  out.c_bg.resize(bins, undefined);
  out.c_g1g2.resize(bins, undefined);
  for (std::size_t i = 0; i < bins; ++i) {
    const double r = field.relative[red][i];
    const double b = field.relative[blue][i];
    const double g1 = field.relative[green_a][i];
    const double g2 = field.relative[green_b][i];
    const double g = 0.5 * (g1 + g2);
    if (std::isfinite(r) && std::isfinite(b) && std::isfinite(g1) &&
        std::isfinite(g2) && g > 0.0 && g2 > 0.0) {
      out.c_rg[i] = r / g;
      out.c_bg[i] = b / g;
      out.c_g1g2[i] = g1 / g2;
    } else {
      out.complete = false;
      ++out.missing_bin_count;
    }
  }

  double lo = 0.0;
  double hi = 0.0;
  double sum = 0.0;
  int count = 0;
  for (int q = 0; q < 4; ++q) {
    const double green =
        0.5 * (field.blocks.corner_relative[q][green_a] +
               field.blocks.corner_relative[q][green_b]);
    if (!std::isfinite(green)) continue;
    if (count == 0) {
      lo = hi = green;
    } else {
      lo = std::min(lo, green);
      hi = std::max(hi, green);
    }
    sum += green;
    ++count;
  }
  if (count == 4 && sum > 0.0) {
    out.green_asymmetry = (hi - lo) / (sum / 4.0);
    if (std::isfinite(out.green_asymmetry)) {
      out.asymmetry_valid = true;
      out.asymmetry_exceeds_policy =
          out.green_asymmetry > opts.asymmetry_policy;
    } else {
      out.complete = false;
    }
  } else {
    out.complete = false;
  }
  return out;
}

ShadingPedestal measure_pedestal(const double* dark, int width, int height,
                                 int row_stride_pixels) {
  ShadingPedestal out;
  if (dark == nullptr || width < 2 || height < 2 || row_stride_pixels < width) {
    return out;
  }
  for (int p = 0; p < 4; ++p) {
    const int dy = p / 2;
    const int dx = p % 2;
    const int plane_width = plane_extent(width, dx);
    const int plane_height = plane_extent(height, dy);
    const std::size_t expected =
        static_cast<std::size_t>(plane_width) * plane_height;
    std::vector<double> samples;
    samples.reserve(expected);
    for (int py = 0; py < plane_height; ++py) {
      for (int px = 0; px < plane_width; ++px) {
        const double value =
            dark[static_cast<std::size_t>(2 * py + dy) * row_stride_pixels +
                 static_cast<std::size_t>(2 * px + dx)];
        if (std::isfinite(value)) samples.push_back(value);
      }
    }
    if (samples.empty()) return {};
    out.finite_fraction[p] = static_cast<double>(samples.size()) / expected;
    out.residual_dn[p] = median_of(samples);
    out.max_abs_residual_dn =
        std::max(out.max_abs_residual_dn, std::abs(out.residual_dn[p]));
  }
  out.full_finite_coverage = true;
  for (const double coverage : out.finite_fraction) {
    out.full_finite_coverage &= coverage == 1.0;
  }
  out.measured = true;
  return out;
}

ShadingComparison compare_shading_fields(const ShadingField& a,
                                         const ShadingField& b) {
  ShadingComparison out;
  if (!a.valid || !b.valid || !a.geometry.valid || !b.geometry.valid ||
      a.grid_cols != b.grid_cols || a.grid_rows != b.grid_rows ||
      !same_rect(a.geometry.gate, b.geometry.gate) ||
      !same_rect(a.geometry.center, b.geometry.center)) {
    return out;
  }
  for (int q = 0; q < 4; ++q) {
    if (!same_rect(a.geometry.corners[q], b.geometry.corners[q])) return out;
  }
  double sum_squares = 0.0;
  int count = 0;
  for (int q = 0; q < 4; ++q) {
    for (int p = 0; p < 4; ++p) {
      const double av = a.blocks.corner_relative[q][p];
      const double bv = b.blocks.corner_relative[q][p];
      if (!std::isfinite(av) || !std::isfinite(bv)) return {};
      const double delta_pp = 100.0 * std::abs(av - bv);
      out.max_corner_delta_pp = std::max(out.max_corner_delta_pp, delta_pp);
      sum_squares += delta_pp * delta_pp;
      ++count;
    }
  }
  if (count == 0) return out;
  out.rms_corner_delta_pp = std::sqrt(sum_squares / count);
  out.measured = true;
  return out;
}

}  // namespace camera_iq
