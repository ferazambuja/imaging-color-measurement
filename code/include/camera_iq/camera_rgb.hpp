#pragma once

namespace camera_iq {

// Linear camera RGB for one chart patch. The values remain in the caller's
// declared scale; this type does not imply colorimetry or normalization.
struct CameraRgbPatch {
  double r = 0;
  double g = 0;
  double b = 0;
};

}  // namespace camera_iq
