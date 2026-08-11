#include "camera_iq/roi.hpp"

#include <algorithm>
#include <cmath>
#include <sstream>

namespace camera_iq {

std::optional<RoiRect> parse_roi_spec(std::string_view spec) {
  std::istringstream in{std::string(spec)};
  RoiRect roi;
  char c1 = 0, c2 = 0, c3 = 0;
  if (!(in >> roi.x >> c1 >> roi.y >> c2 >> roi.width >> c3 >>
        roi.height)) {
    return std::nullopt;
  }
  if (c1 != ',' || c2 != ',' || c3 != ',') return std::nullopt;
  in >> std::ws;
  if (!in.eof()) return std::nullopt;
  if (roi.x < 0 || roi.y < 0 || roi.width <= 0 || roi.height <= 0) {
    return std::nullopt;
  }
  return roi;
}

std::optional<RoiRect> cfa_balanced_roi(const RoiRect& requested,
                                        int image_width, int image_height) {
  if (image_width <= 0 || image_height <= 0 || requested.width <= 0 ||
      requested.height <= 0) {
    return std::nullopt;
  }

  const long long req_x1 =
      static_cast<long long>(requested.x) + requested.width;
  const long long req_y1 =
      static_cast<long long>(requested.y) + requested.height;

  int x0 = std::clamp(requested.x, 0, image_width);
  int y0 = std::clamp(requested.y, 0, image_height);
  int x1 = static_cast<int>(
      std::clamp(req_x1, 0LL, static_cast<long long>(image_width)));
  int y1 = static_cast<int>(
      std::clamp(req_y1, 0LL, static_cast<long long>(image_height)));

  if (x0 & 1) ++x0;
  if (y0 & 1) ++y0;
  if (x1 & 1) --x1;
  if (y1 & 1) --y1;

  if (x1 - x0 < 2 || y1 - y0 < 2) return std::nullopt;
  return RoiRect{x0, y0, x1 - x0, y1 - y0};
}

std::optional<RoiRect> centered_cfa_balanced_roi(int image_width,
                                                 int image_height,
                                                 double linear_fraction) {
  if (image_width < 2 || image_height < 2 ||
      !std::isfinite(linear_fraction) || linear_fraction <= 0.0 ||
      linear_fraction > 1.0) {
    return std::nullopt;
  }

  int width =
      static_cast<int>(std::floor(image_width * linear_fraction)) & ~1;
  int height =
      static_cast<int>(std::floor(image_height * linear_fraction)) & ~1;
  if (width < 2 || height < 2) return std::nullopt;

  const int x = ((image_width - width) / 2) & ~1;
  const int y = ((image_height - height) / 2) & ~1;
  const RoiRect requested{x, y, width, height};
  const auto balanced =
      cfa_balanced_roi(requested, image_width, image_height);
  if (!balanced || balanced->x != requested.x || balanced->y != requested.y ||
      balanced->width != requested.width ||
      balanced->height != requested.height) {
    return std::nullopt;
  }
  return balanced;
}


}  // namespace camera_iq
