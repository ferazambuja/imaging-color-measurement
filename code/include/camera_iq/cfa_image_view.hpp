#pragma once

#include <array>
#include <span>
#include <string_view>

namespace camera_iq {

// Non-owning view of an active-area, black-subtracted 2x2 CFA mosaic. The
// white and black levels retain the sensor-DN context needed to detect
// near-saturation without exposing a RAW decoder or archive format.
struct CfaImageView {
  int width = 0;
  int height = 0;
  int row_stride_pixels = 0;
  std::array<int, 4> color_at_position{0, 1, 2, 3};
  std::string_view cdesc;
  std::span<const double> samples;
  double white_level = 0.0;
  std::array<double, 4> black_per_channel{0, 0, 0, 0};
};

}  // namespace camera_iq
