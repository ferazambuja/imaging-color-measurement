#pragma once

#include <array>
#include <limits>
#include <vector>

#include "camera_iq/camera_rgb.hpp"

namespace camera_iq {

struct Xyz {
  double x = 0;
  double y = 0;
  double z = 0;
};

struct Lab {
  double l = 0;
  double a = 0;
  double b = 0;
};

struct Ipt {
  double i = 0;
  double p = 0;
  double t = 0;
};

// OkLab relative to D65 with L in [0,1] for the SDR reference range.
// Finite extended values are supported for intermediate transforms.
struct Oklab {
  double l = 0;
  double a = 0;
  double b = 0;
};

struct Oklch {
  double l = 0;
  double c = 0;
  double h_degrees = 0;
  bool hue_defined = false;
};

enum class Cie94Application {
  GraphicArts,
  Textiles,
};

struct CcmFit {
  std::array<std::array<double, 3>, 3> matrix{};
  std::size_t patch_count = 0;
  double mean_delta_e_76 = 0;
  double max_delta_e_76 = 0;
  double rms_delta_e_76 = 0;
  double mean_delta_e_2000 = 0;
  double max_delta_e_2000 = 0;
  double rms_delta_e_2000 = 0;
};

struct CcmCrossValidation {
  std::size_t patch_count = 0;
  std::size_t fold_count = 0;
  double mean_delta_e_76 = 0;
  double max_delta_e_76 = 0;
  double rms_delta_e_76 = 0;
  double mean_delta_e_2000 = 0;
  double max_delta_e_2000 = 0;
  double rms_delta_e_2000 = 0;
};

struct CcmEvaluation {
  std::size_t patch_count = 0;
  double mean_delta_e_76 = 0;
  double max_delta_e_76 = 0;
  double rms_delta_e_76 = 0;
  double mean_delta_e_2000 = 0;
  double max_delta_e_2000 = 0;
  double rms_delta_e_2000 = 0;
};

struct CcmLightnessSelection {
  double max_lstar = 0;
  std::vector<std::size_t> kept_indices;
  std::vector<std::size_t> excluded_indices;
};

struct CcmDarkPatchDiagnostics {
  double max_lstar = 25.0;
  std::size_t patch_count = 0;
  std::size_t worst_patch_index = std::numeric_limits<std::size_t>::max();
  double mean_delta_e_76 = 0;
  double max_delta_e_76 = 0;
  double rms_delta_e_76 = 0;
  double mean_delta_e_2000 = 0;
  double max_delta_e_2000 = 0;
  double rms_delta_e_2000 = 0;
};

// XYZ and reference white must use the same scale (for example both Y=1 or
// both Y=100). Finite negative XYZ is supported as an extended mathematical
// domain for intermediate color transforms; the reference white is positive.
Lab xyz_to_lab(const Xyz& xyz, const Xyz& white);

double delta_e_76(const Lab& first, const Lab& second);

Xyz lab_to_xyz(const Lab& lab, const Xyz& white);

// IPT assumes D65-adapted relative XYZ with Ywhite=1. The signed 0.43
// response preserves finite negative intermediate values instead of silently
// clipping them.
Ipt xyz_d65_to_ipt(const Xyz& xyz);

// Dated W3C CSS Color 4 64-bit matrices, relative D65 XYZ (Ywhite=1).
// Sign-preserving cube roots retain the finite extended transform domain.
Oklab xyz_d65_to_oklab(const Xyz& xyz);
Xyz oklab_to_xyz_d65(const Oklab& oklab);
Oklch oklab_to_oklch(const Oklab& oklab);
Oklab oklch_to_oklab(const Oklch& oklch);
double delta_e_ok(const Oklab& first, const Oklab& second);

// CIE94 is directional because its chroma and hue weights use the reference
// color's chroma. Callers must name both the reference role and application.
double delta_e_94(const Lab& reference, const Lab& sample,
                  Cie94Application application);

// Separately named, symmetric historical variant. This is not the directional
// CIE94 reference-color contract; it uses sqrt(C1*C2) for S_C and S_H.
double delta_e_94_geometric_mean_chroma(const Lab& first, const Lab& second,
                                        Cie94Application application);

double delta_e_2000(const Lab& a, const Lab& b);

Xyz apply_ccm(const std::array<std::array<double, 3>, 3>& matrix,
              const CameraRgbPatch& rgb);

CcmFit fit_rgb_to_xyz_ccm(const std::vector<CameraRgbPatch>& camera_rgb,
                          const std::vector<Xyz>& target_xyz,
                          const Xyz& white_xyz);

CcmEvaluation evaluate_rgb_to_xyz_ccm(
    const std::array<std::array<double, 3>, 3>& matrix,
    const std::vector<CameraRgbPatch>& camera_rgb,
    const std::vector<Xyz>& target_xyz, const Xyz& white_xyz);

CcmCrossValidation cross_validate_rgb_to_xyz_ccm(
    const std::vector<CameraRgbPatch>& camera_rgb,
    const std::vector<Xyz>& target_xyz, const Xyz& white_xyz,
    std::size_t fold_count = 5);

CcmLightnessSelection select_reference_lightness(
    const std::vector<Xyz>& target_xyz, const Xyz& white_xyz,
    double exclude_below_lstar);

CcmDarkPatchDiagnostics diagnose_dark_patches(
    const std::vector<CameraRgbPatch>& camera_rgb,
    const std::vector<Xyz>& target_xyz, const Xyz& white_xyz,
    const std::array<std::array<double, 3>, 3>& matrix,
    double max_lstar = 25.0);

}  // namespace camera_iq
