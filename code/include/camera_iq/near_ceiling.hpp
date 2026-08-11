#pragma once

namespace camera_iq {

// Shared default for signal-referred headroom screening. Commands may expose a
// runtime override, but every result must serialize the effective value. This
// is a declared project analysis choice, not a camera-industry standard.
inline constexpr double kDefaultNearCeilingLevel = 0.98;

} // namespace camera_iq
