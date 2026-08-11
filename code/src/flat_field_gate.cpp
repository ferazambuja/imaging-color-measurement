#include "camera_iq/flat_field_gate.hpp"

#include <array>
#include <cmath>
#include <cstddef>
#include <limits>

namespace camera_iq {
namespace {

bool same_rect(const RoiRect& a, const RoiRect& b) {
  return a.x == b.x && a.y == b.y && a.width == b.width &&
         a.height == b.height;
}

}  // namespace

std::optional<CfaNearCeilingMeasurement> measure_cfa_near_ceiling(
    const double* data, int width, int height, int row_stride_pixels,
    const RoiRect& gate, const std::array<double, 4>& signal_ceiling,
    double near_ceiling_level) {
  if (data == nullptr || width < 2 || height < 2 ||
      row_stride_pixels < width || !std::isfinite(near_ceiling_level) ||
      near_ceiling_level <= 0.0 || near_ceiling_level > 1.0) {
    return std::nullopt;
  }
  for (double ceiling : signal_ceiling) {
    if (!std::isfinite(ceiling) || ceiling <= 0.0) return std::nullopt;
  }

  // `shading` rejects an odd mosaic outright rather than publish a trimmed
  // frame, and screening must agree with it. Trimming would leave the last row
  // or column unscreened while apply_flat_field() still corrects those pixels.
  if ((width & 1) != 0 || (height & 1) != 0)
    return std::nullopt;

  const auto frame =
      cfa_balanced_roi(RoiRect{0, 0, width, height}, width, height);
  const auto balanced_gate = cfa_balanced_roi(gate, width, height);
  // cfa_balanced_roi() clips to the frame. Equality therefore proves both
  // containment and CFA balance; a second containment branch is unreachable.
  if (!frame || !balanced_gate || !same_rect(*balanced_gate, gate)) {
    return std::nullopt;
  }

  std::array<std::size_t, 4> frame_finite{};
  std::array<std::size_t, 4> frame_near{};
  std::array<std::size_t, 4> gate_finite{};
  std::array<std::size_t, 4> gate_near{};
  std::array<std::size_t, 4> frame_total{};
  std::array<std::size_t, 4> gate_total{};
  for (int y = frame->y; y < frame->y + frame->height; ++y) {
    const bool row_in_gate = y >= gate.y && y < gate.y + gate.height;
    const std::size_t row = static_cast<std::size_t>(y) *
                            static_cast<std::size_t>(row_stride_pixels);
    for (int x = frame->x; x < frame->x + frame->width; ++x) {
      const std::size_t position =
          static_cast<std::size_t>((y & 1) * 2 + (x & 1));
      const double value = data[row + static_cast<std::size_t>(x)];
      const bool in_gate = row_in_gate && x >= gate.x &&
                           x < gate.x + gate.width;
      // Counted before the finiteness test so coverage has a denominator that
      // does not shrink with the very samples it is measuring.
      ++frame_total[position];
      if (in_gate)
        ++gate_total[position];
      if (!std::isfinite(value))
        continue;
      ++frame_finite[position];
      if (in_gate) ++gate_finite[position];
      if (value >= near_ceiling_level * signal_ceiling[position]) {
        ++frame_near[position];
        if (in_gate) ++gate_near[position];
      }
    }
  }

  CfaNearCeilingMeasurement out;
  out.frame = *frame;
  out.gate = gate;
  const double undefined = std::numeric_limits<double>::quiet_NaN();
  for (std::size_t p = 0; p < 4; ++p) {
    out.fraction_frame[p] =
        frame_finite[p] > 0
            ? static_cast<double>(frame_near[p]) /
                  static_cast<double>(frame_finite[p])
            : undefined;
    out.fraction_gate[p] =
        gate_finite[p] > 0
            ? static_cast<double>(gate_near[p]) /
                  static_cast<double>(gate_finite[p])
            : undefined;
    out.finite_fraction_frame[p] = frame_total[p] > 0
                                       ? static_cast<double>(frame_finite[p]) /
                                             static_cast<double>(frame_total[p])
                                       : undefined;
    out.finite_fraction_gate[p] = gate_total[p] > 0
                                      ? static_cast<double>(gate_finite[p]) /
                                            static_cast<double>(gate_total[p])
                                      : undefined;
  }
  return out;
}

}  // namespace camera_iq
