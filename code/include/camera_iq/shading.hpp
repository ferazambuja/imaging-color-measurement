#pragma once

#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "camera_iq/flat_field_gate.hpp"
#include "camera_iq/roi.hpp"

namespace camera_iq {

// Geometry for a flat-field response measurement. Three regions with three
// jobs; they are deliberately different sizes and must never be interchanged.
//
//   gate region    (gate_center_frac)  near-ceiling gate only
//   center block   (corner_block_px)   normalizer and low-signal anchor
//   bin grid       (grid_cols/rows)    the response maps
//
// The gate region is larger than the block it protects: loss of center
// headroom can bias reported falloff toward less falloff, so the gate has to
// see more of the frame than the region being normalized by.
struct ShadingOptions {
  int grid_cols = 16;
  int grid_rows = 12;
  // Linear fraction of each CFA plane, centered. Gates only — never the
  // normalizer.
  double gate_center_frac = kFlatFieldGateCenterFraction;
  // Center/corner block edge in mosaic pixels; each CFA plane sees half of it.
  int corner_block_px = 400;
  // Corner-block inset from the frame edge, in mosaic pixels.
  int corner_inset_px = 120;

  // Declared analysis policies, not camera-industry standards. The report
  // publishes the measured diagnostics beside every pass/fail verdict.
  double near_ceiling_level = kFlatFieldNearCeilingLevel;
  double near_ceiling_max = kFlatFieldMaxNearCeilingFraction;
  double min_center_signal = 0.05;
  double max_negative_frac = 0.01;
  double min_bin_coverage = 0.90;
  // Declared diagnostic threshold for departure from a centered radial scalar
  // model. It never grants or refuses attribution to any physical component.
  double asymmetry_policy = 0.05;
};

// Gate diagnostics. A post-measurement rejection carries the numbers that
// explain it. An earlier rejection leaves `measured` false so serializers do
// not turn initializer values into fabricated measurements.
//
// The near-ceiling fraction is reported twice on purpose. Both regions are
// gated per CFA position: the centered form catches a concentrated bright
// region, while the frame form catches a peripheral or broadly elevated field.
struct ShadingGates {
  bool measured = false;
  std::array<double, 4> near_ceiling_frac_gate{0, 0, 0, 0};
  std::array<double, 4> near_ceiling_frac_frame{0, 0, 0, 0};
  // Coverage the two fractions above were measured over. A near-ceiling
  // fraction is a ratio over finite samples, so it is only meaningful when
  // enough samples were finite to take it.
  std::array<double, 4> finite_frac_gate{0, 0, 0, 0};
  std::array<double, 4> finite_frac_frame{0, 0, 0, 0};
  std::array<double, 4> negative_frac{0, 0, 0, 0};
  // Center-block median as a fraction of that plane's signal-referred ceiling.
  std::array<double, 4> center_signal_frac{0, 0, 0, 0};
  // Worst per-bin fraction of expected samples that were finite and usable.
  double min_bin_coverage = 0.0;

  bool near_ceiling_ok = false;
  // Screening-region finite coverage is distinct from near-ceiling headroom
  // and from the per-map-bin coverage verdict below.
  bool screening_coverage_ok = false;
  bool low_signal_ok = false;
  bool negative_ok = false;
  bool coverage_ok = false;
  // True when every reported aggregate is finite. Individual missing samples
  // are controlled independently by `min_bin_coverage`.
  bool finite_ok = false;
};

// Per-CFA-position binned response over a black-subtracted active Bayer mosaic.
//
// Planes are indexed by 2x2 mosaic position (0,0)(0,1)(1,0)(1,1). The caller
// supplies the mapping from those positions to channel letters. Each plane is
// measured independently; the planes are never summed.
//
// `bin_median` holds the per-bin sample median in black-subtracted DN,
// row-major over `grid_rows` x `grid_cols`. `relative` is `bin_median` divided
// by that plane's own `center_block_median`. The center-block median therefore
// normalizes to 1.0; individual center samples or grid bins need not.
//
// `valid` is false when the measurement cannot be normalized — the center-block
// median is a denominator on every map and scalar, so a non-positive one
// rejects rather than dividing. A rejected measurement keeps every diagnostic
// it computed (`bin_median`, `center_block_median`) and leaves `relative`
// empty, mirroring how the SFR command preserves diagnostics on a rejected
// center.

// Corner/center block scalars and the corner-field asymmetry statistic.
//
// Corners are indexed TL, TR, BL, BR and are `corner_block_px` blocks inset by
// `corner_inset_px`. They are deliberately not the grid's corner bins: a 16x12
// bin is centered at 1/32 of the frame width and never reaches the corner. The
// grid drives the maps; these blocks drive the scalars.
//
//   A = ( max_q G(q) - min_q G(q) ) / mean_q G(q)
//
// over the four corner blocks of the green relative-response map, where G is
// the mean of the two green planes. The four blocks sit at equal radius, so a
// centered radially symmetric field drives A to zero analytically — and to
// within a small discretization residual in a sampled synthetic fixture. That
// residual depends on the field, CFA sampling, block geometry, and estimator;
// it is not a universal sampling floor. A value above the policy is a
// diagnostic departure from this scalar model; it does not identify whether
// source, lens, sensor, alignment, or another capture term caused it.
//
// Equal radius is what licenses the inference. An off-axis maximum alone does
// not: a centered radial response may peak on an annulus.
// Four corner-block scalar sets used by the corner-field asymmetry statistic.
// The separate center normalizer is ShadingField::center_block_median.
struct ShadingBlocks {
  std::array<std::array<double, 4>, 4> corner_median{};
  std::array<std::array<double, 4>, 4> corner_relative{};
};

// Effective mosaic-space regions after inward CFA balancing. Invalid or
// impossible geometry is rejected rather than clipped into a different test.
struct ShadingGeometry {
  bool valid = false;
  RoiRect gate;
  RoiRect center;
  std::array<RoiRect, 4> corners{};  // TL, TR, BL, BR
};

// Validates the complete numerical policy before any pixels are read. Exposed
// so archive adapters and the decoder-independent estimator cannot drift onto
// separate interpretations of the same options.
bool shading_options_valid(const ShadingOptions& opts);

// Resolves the exact effective regions used by the estimator. The rectangles
// are CFA-balanced and remain equal-radius at the four corner sites; invalid
// or odd mosaic geometry is refused rather than silently reshaped.
std::optional<ShadingGeometry> make_shading_geometry(
    int width, int height, const ShadingOptions& opts);

struct ShadingField {
  bool valid = false;
  std::string rejection_reason;
  // Complete effective policy used for this result, including rejections.
  ShadingOptions options;
  bool signal_ceiling_measured = false;
  std::array<double, 4> signal_ceiling{0, 0, 0, 0};
  int grid_cols = 0;
  int grid_rows = 0;
  std::array<std::vector<double>, 4> bin_median;
  std::array<std::vector<double>, 4> relative;
  std::array<double, 4> center_block_median{0, 0, 0, 0};
  ShadingGates gates;
  ShadingBlocks blocks;
  ShadingGeometry geometry;
};

// Measures the binned per-CFA-position response of a flat-field capture.
//
// `data` points at the active-area origin and holds signed black-subtracted
// residuals; this function never subtracts black again. `row_stride_pixels` may
// exceed `width` when walking a cropped view of a larger buffer.
//
// Bins use the median rather than the mean so isolated defective pixels cannot
// move a bin. Returns an empty field if the geometry or options are invalid.
//
// `ceiling` is the signal-referred ceiling per CFA position — `white_level -
// black[p]`, not `white_level`. Samples here are already black-subtracted
// residuals, so a gate written against the raw white level is dimensionally
// wrong. When gate evaluation is reached, `ShadingField::gates` reports every
// fraction even if a quality gate rejects. Earlier failures leave
// `ShadingField::gates.measured` false.
ShadingField measure_shading_field(const double *data, int width, int height,
                                   int row_stride_pixels,
                                   const ShadingOptions &opts,
                                   const std::array<double, 4> &ceiling);

// Center-normalized chromatic response of the measured capture-system field.
//
//   C_RG(bin) = [ R(bin) / R_center ] / [ G(bin) / G_center ]
//   C_BG(bin) = [ B(bin) / B_center ] / [ G(bin) / G_center ]
//
// These are ratios of independently center-normalized response maps, not raw
// R/G and B/G ratios. `G` is the arithmetic mean of the two green planes'
// relative response.
//
// `c_g1g2` applies the same construction to the two greens alone. It reads 1.0
// only when the two independently normalized green planes have the same spatial
// response; a uniform gain difference cancels and is not diagnosed by this map.
//
// A small corner-to-corner spread in these maps does not establish camera-only
// color shading: a spatially varying source spectrum produces the same
// signature. Interpretation belongs to the full capture system, including
// source, optics, sensor/CFA, alignment, and other uncontrolled capture terms.
struct ShadingChromatic {
  // `layout_valid` retains CFA-position provenance even when the field itself
  // is rejected and therefore has no chromatic maps.
  bool layout_valid = false;
  bool valid = false;  // false when maps cannot be derived
  bool complete = false;  // false when one or more ratio bins are undefined
  std::size_t missing_bin_count = 0;
  int red_position = -1;
  int green1_position = -1;
  int green2_position = -1;
  int blue_position = -1;
  std::vector<double> c_rg;
  std::vector<double> c_bg;
  std::vector<double> c_g1g2;
  // Corner-field asymmetry of the green relative response over the four corner
  // blocks. See ShadingBlocks for the definition and what it does and does not
  // license. Lives here rather than on ShadingField because "green" is a CFA
  // fact: greens sit on the anti-diagonal for RGGB but not for GRBG.
  bool asymmetry_valid = false;
  double green_asymmetry = 0.0;
  bool asymmetry_exceeds_policy = false;
};

// Derives chromatic maps from a measured field. `color_at_position` maps the
// mosaic positions (0,0)(0,1)(1,0)(1,1) to indices in `cdesc`. Exactly one R,
// one B, and two G positions are required; any other layout returns
// `valid=false`.
ShadingChromatic chromatic_response(const ShadingField& field,
                                    const std::array<int, 4>& color_at_position,
                                    std::string_view cdesc,
                                    const ShadingOptions& opts = {});

// Pedestal check against a dark frame. The dark bounds the result and is never
// subtracted from, added to, or otherwise
// applied to the samples, which are already black-subtracted residuals.
//
// A pedestal error does not cancel from center-normalized ratios: it biases
// lower-signal corners differently from the center and can affect R/B against
// G unequally. That is why the pairing result remains explicit.
//
// `exposure_metadata_matches` records whether the dark's aperture, shutter and
// ISO agree with the shading frame. A filename alone does not establish a
// matching exposure; a mismatch downgrades the result rather than being ignored.
struct ShadingPedestal {
  bool measured = false;
  bool compatible = false;
  bool make_model_metadata_matches = false;
  // Physical body identity is proven only when both maker-reported serials are
  // present and equal. If both serials are absent, `body_serials_consistent`
  // remains true so the bounded dark-control check can proceed, while
  // `body_serials_match` stays false and therefore does not fabricate sample
  // identity. A one-sided or unequal serial is a blocking conflict.
  bool body_serials_present = false;
  bool body_serials_match = false;
  bool body_serials_consistent = false;
  std::string dark_file;
  std::array<double, 4> residual_dn{0, 0, 0, 0};
  double max_abs_residual_dn = 0.0;
  std::array<double, 4> finite_fraction{0, 0, 0, 0};
  bool full_finite_coverage = false;
  bool spatial_checked = false;
  std::array<double, 4> center_residual_dn{0, 0, 0, 0};
  std::array<std::array<double, 4>, 4> corner_residual_dn{};
  double max_abs_spatial_residual_dn = 0.0;
  bool exposure_metadata_present = false;
  bool exposure_metadata_matches = false;
  bool within_tolerance = false;
  bool verified = false;
};

// Repeatability / exposure-invariance comparison between two measured fields.
//
// Defined per corner site and per plane as the absolute difference in relative
// response, in percentage points; both the maximum and the RMS across the 16
// corner-by-CFA measurements are reported, because one frame pair supports a
// check and not a proof. A purely multiplicative field is exposure-invariant
// once each frame is normalized by its own center, so a non-zero delta is
// shows that the field is not a fixed multiplicative map.
struct ShadingComparison {
  bool measured = false;
  double max_corner_delta_pp = 0.0;
  double rms_corner_delta_pp = 0.0;
};

ShadingComparison compare_shading_fields(const ShadingField& a,
                                         const ShadingField& b);

// Measures the pedestal residual of a dark frame: the per-CFA-position median
// of its black-subtracted samples, which is zero when the metadata black level
// is right. `dark` must be the same CFA geometry as the shading frame.
ShadingPedestal measure_pedestal(const double* dark, int width, int height,
                                 int row_stride_pixels);

}  // namespace camera_iq
