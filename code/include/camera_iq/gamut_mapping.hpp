#pragma once

#include <cstddef>

#include "camera_iq/colorimetry.hpp"

namespace camera_iq {

enum class RgbColorSpace { Srgb, DisplayP3 };

struct EncodedRgb {
  double r = 0;
  double g = 0;
  double b = 0;
};

struct LinearRgb {
  double r = 0;
  double g = 0;
  double b = 0;
};

enum class GamutMapIntent {
  BoundaryProjection,
  SoftChromaCompression,
  OklchBoundaryProjection,
  CssColor4LocalMinde,
};

enum class GamutMappingCoordinateSpace { CielabD65, OklabD65 };

enum class GamutMapBranch {
  IdentityNoMappingRequired,
  IdentityNoGamutContraction,
  ProtectedCoreIdentity,
  FixedLhRadialBoundaryClip,
  FixedOklchRadialBoundaryClip,
  SoftChromaCompression,
  CssColor4LocalMindeInitialClip,
  CssColor4LocalMindeBinarySearch,
};

struct GamutBoundaryOptions {
  double maximum_chroma = 512.0;
  std::size_t refinement_iterations = 48;
  double chroma_tolerance = 1e-10;
  double gamut_tolerance = 1e-12;
};

struct GamutMapOptions {
  RgbColorSpace source = RgbColorSpace::DisplayP3;
  RgbColorSpace destination = RgbColorSpace::Srgb;
  GamutMapIntent intent = GamutMapIntent::BoundaryProjection;
  // Soft compression preserves the connected core below this fraction of the
  // destination boundary. It intentionally moves the in-gamut shell above it.
  double knee_fraction = 0.75;
  GamutBoundaryOptions boundary;
  GamutBoundaryOptions oklch_boundary = {2.0, 48, 1e-12, 1e-12};
};

struct GamutBoundaryResult {
  double chroma = 0;
  double lower_chroma = 0;
  double upper_chroma = 0;
  double bracket_width = 0;
  std::size_t segments_examined = 0;
  std::size_t refinement_iterations = 0;
  bool converged = false;
};

struct CssColor4LocalMindeDiagnostics {
  bool applicable = false;
  double jnd = 0.02;
  double epsilon = 0.0001;
  std::size_t iterations = 0;
  double final_delta_e_ok = 0;
  bool returned_clipped_color = false;
};

struct GamutMappingResult {
  Lab input_lab;
  Lab output_lab;
  Xyz input_xyz;
  Xyz output_xyz;
  LinearRgb destination_linear_before;
  LinearRgb destination_linear_after;
  EncodedRgb output_encoded;
  GamutMappingCoordinateSpace mapping_coordinate_space =
      GamutMappingCoordinateSpace::CielabD65;
  Oklab input_oklab;
  Oklab output_oklab;
  Oklch input_oklch;
  Oklch output_oklch;
  bool input_in_destination = false;
  bool modified = false;
  bool output_in_destination = false;
  GamutMapBranch branch = GamutMapBranch::IdentityNoMappingRequired;
  double input_chroma = 0;
  double output_chroma = 0;
  double input_mapping_chroma = 0;
  double output_mapping_chroma = 0;
  bool boundary_diagnostics_applicable = true;
  GamutBoundaryResult source_boundary;
  GamutBoundaryResult destination_boundary;
  double source_boundary_chroma = 0;
  double destination_boundary_chroma = 0;
  double knee_chroma = 0;
  CssColor4LocalMindeDiagnostics local_minde;
};

Xyz d65_white_xyz();

LinearRgb decode_rgb(const EncodedRgb& encoded);
EncodedRgb encode_rgb(const LinearRgb& linear);

Xyz linear_rgb_to_xyz(const LinearRgb& rgb, RgbColorSpace space);
LinearRgb xyz_to_linear_rgb(const Xyz& xyz, RgbColorSpace space);

bool is_in_unit_gamut(const LinearRgb& rgb, double tolerance = 0.0);

// Returns the first exit from the neutral-connected interval. At fixed L* and
// Lab hue, Lab->linear RGB is piecewise cubic in chroma. The solver enumerates
// every channel crossing of the tolerated 0/1 surfaces, inspects the resulting
// monotone intervals, and refines the first in->out transition. This is
// deliberately not the largest in-gamut chroma: a ray can later re-enter.
GamutBoundaryResult find_gamut_boundary(
    double lightness, double hue_radians, RgbColorSpace space,
    const GamutBoundaryOptions& options = GamutBoundaryOptions{});

GamutBoundaryResult find_oklch_gamut_boundary(
    double lightness, double hue_radians, RgbColorSpace space,
    const GamutBoundaryOptions& options =
        GamutBoundaryOptions{2.0, 48, 1e-12, 1e-12});

double gamut_boundary_chroma(
    double lightness, double hue_radians, RgbColorSpace space,
    const GamutBoundaryOptions& options = GamutBoundaryOptions{});

// The input is CIELAB relative to D65 with a normalized Ywhite=1 reference.
// The explicit name prevents generic D50 Lab values from being accepted under
// a plausible but wrong white-point assumption.
GamutMappingResult map_d65_lab_to_gamut(const Lab& input,
                                        const GamutMapOptions& options);

GamutMappingResult map_encoded_rgb_to_gamut(
    const EncodedRgb& input, const GamutMapOptions& options);

}  // namespace camera_iq
