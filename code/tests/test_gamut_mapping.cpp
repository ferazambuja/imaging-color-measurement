// Public tests for the Display-P3 to sRGB gamut-mapping core.
//
// The fixtures are synthetic and deterministic. They exercise the numerical
// contracts that the portfolio discusses without requiring a display profile,
// image archive, or observer experiment.

#include "camera_iq/gamut_mapping.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <string>

namespace {

using camera_iq::EncodedRgb;
using camera_iq::GamutMapBranch;
using camera_iq::GamutMapIntent;
using camera_iq::GamutMapOptions;
using camera_iq::GamutMappingCoordinateSpace;
using camera_iq::GamutMappingResult;
using camera_iq::Lab;
using camera_iq::LinearRgb;
using camera_iq::RgbColorSpace;

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

template <typename Operation>
bool throws(Operation operation) {
  try {
    operation();
  } catch (const std::exception&) {
    return true;
  }
  return false;
}

double hue_radians(const Lab& lab) { return std::atan2(lab.b, lab.a); }

GamutMapOptions options(GamutMapIntent intent) {
  GamutMapOptions out;
  out.source = RgbColorSpace::DisplayP3;
  out.destination = RgbColorSpace::Srgb;
  out.intent = intent;
  return out;
}

void test_transforms_and_common_gamut_identity() {
  const auto half = camera_iq::decode_rgb({0.5, 0.5, 0.5});
  check_near(half.r, 0.21404114048223255, 1e-15,
             "the shared RGB transfer curve decodes 0.5");
  check_near(camera_iq::encode_rgb(half).r, 0.5, 1e-15,
             "the transfer curve round-trips 0.5");
  check_near(camera_iq::decode_rgb({0.04045, 0, 0}).r, 0.04045 / 12.92,
             1e-15, "the encoded breakpoint uses the linear branch");

  struct Primary {
    RgbColorSpace space;
    LinearRgb rgb;
    std::array<double, 3> xyz;
  };
  const std::array<Primary, 6> primaries{{
      {RgbColorSpace::Srgb, {1, 0, 0},
       {0.41239079926595951, 0.21263900587151036, 0.019330818715591849}},
      {RgbColorSpace::Srgb, {0, 1, 0},
       {0.35758433938387796, 0.71516867876775593, 0.11919477979462599}},
      {RgbColorSpace::Srgb, {0, 0, 1},
       {0.18048078840183429, 0.072192315360733715, 0.95053215224966058}},
      {RgbColorSpace::DisplayP3, {1, 0, 0},
       {0.48657094864821615, 0.22897456406974881, 0.0}},
      {RgbColorSpace::DisplayP3, {0, 1, 0},
       {0.26566769316909306, 0.69173852183650642, 0.045113381858902638}},
      {RgbColorSpace::DisplayP3, {0, 0, 1},
       {0.1982172852343625, 0.079286914093745001, 1.043944368900976}},
  }};
  for (const auto& primary : primaries) {
    const auto xyz = camera_iq::linear_rgb_to_xyz(primary.rgb, primary.space);
    check_near(xyz.x, primary.xyz[0], 1e-15, "primary matrix X");
    check_near(xyz.y, primary.xyz[1], 1e-15, "primary matrix Y");
    check_near(xyz.z, primary.xyz[2], 1e-15, "primary matrix Z");
    const auto back = camera_iq::xyz_to_linear_rgb(xyz, primary.space);
    check_near(back.r, primary.rgb.r, 2e-15, "primary matrix round-trip R");
    check_near(back.g, primary.rgb.g, 2e-15, "primary matrix round-trip G");
    check_near(back.b, primary.rgb.b, 2e-15, "primary matrix round-trip B");
  }

  // Encoded sRGB red expressed in Display-P3. The hard intents must recognize
  // it as common-gamut content and preserve the colorimetric result.
  const EncodedRgb common_red{0.91748755732516563, 0.20028680774084706,
                              0.13856059121111408};
  for (const auto intent : {GamutMapIntent::BoundaryProjection,
                            GamutMapIntent::OklchBoundaryProjection,
                            GamutMapIntent::CssColor4LocalMinde}) {
    const auto mapped =
        camera_iq::map_encoded_rgb_to_gamut(common_red, options(intent));
    check(mapped.input_in_destination && !mapped.modified,
          "hard intent preserves a common-gamut input");
    check_near(mapped.output_encoded.r, 1.0, 2e-9,
               "common-gamut identity reaches sRGB red R");
    check_near(mapped.output_encoded.g, 0.0, 2e-9,
               "common-gamut identity reaches sRGB red G");
    check_near(mapped.output_encoded.b, 0.0, 2e-9,
               "common-gamut identity reaches sRGB red B");
  }
}

void test_coordinate_and_algorithm_separation() {
  const auto radial = camera_iq::map_encoded_rgb_to_gamut(
      {1, 1, 0}, options(GamutMapIntent::BoundaryProjection));
  const auto oklch = camera_iq::map_encoded_rgb_to_gamut(
      {1, 1, 0}, options(GamutMapIntent::OklchBoundaryProjection));
  const auto local = camera_iq::map_encoded_rgb_to_gamut(
      {1, 1, 0}, options(GamutMapIntent::CssColor4LocalMinde));

  check(radial.branch == GamutMapBranch::FixedLhRadialBoundaryClip,
        "CIELAB radial branch is explicit");
  check(oklch.branch == GamutMapBranch::FixedOklchRadialBoundaryClip,
        "OkLCh radial branch is explicit");
  check(local.mapping_coordinate_space ==
            GamutMappingCoordinateSpace::OklabD65,
        "Local MINDE records its coordinate system");
  check(radial.output_in_destination && oklch.output_in_destination &&
            local.output_in_destination,
        "all three hard intents produce destination-gamut output");

  check_near(radial.output_lab.l, radial.input_lab.l, 1e-12,
             "CIELAB radial mapping preserves Lstar");
  check_near(hue_radians(radial.output_lab), hue_radians(radial.input_lab),
             1e-12, "CIELAB radial mapping preserves Lab hue");
  check_near(oklch.output_oklch.l, oklch.input_oklch.l, 1e-12,
             "OkLCh radial mapping preserves OkLab lightness");
  check_near(oklch.output_oklch.h_degrees, oklch.input_oklch.h_degrees,
             1e-10, "OkLCh radial mapping preserves OkLCh hue");

  check(radial.output_mapping_chroma < 23.0,
        "CIELAB radial mapping exposes the yellow overcompression case");
  check(oklch.output_mapping_chroma > 0.20 &&
            oklch.output_mapping_chroma < oklch.input_mapping_chroma,
        "OkLCh radial mapping retains more yellow chroma while staying bounded");
  check(local.local_minde.applicable && local.local_minde.iterations > 0,
        "Local MINDE records an active binary search");
  check_near(local.local_minde.jnd, 0.02, 0.0,
             "Local MINDE records the algorithm threshold");
  check_near(local.local_minde.epsilon, 0.0001, 0.0,
             "Local MINDE records the search epsilon");
  check(local.local_minde.final_delta_e_ok < 0.02,
        "Local MINDE terminates below its declared local difference threshold");
  check_near(local.output_encoded.r, 0.9962332729609733, 5e-4,
             "Local MINDE P3-yellow R agrees with the independent oracle");
  check_near(local.output_encoded.g, 0.9990138958496102, 5e-4,
             "Local MINDE P3-yellow G agrees with the independent oracle");
  check_near(local.output_encoded.b, 0.0, 5e-4,
             "Local MINDE P3-yellow B agrees with the independent oracle");
}

void test_first_exit_boundary_and_soft_knee() {
  const auto white = camera_iq::d65_white_xyz();
  const Lab p3_red = camera_iq::xyz_to_lab(
      camera_iq::linear_rgb_to_xyz({1, 0, 0}, RgbColorSpace::DisplayP3),
      white);
  const double hue = hue_radians(p3_red);
  const auto boundary = camera_iq::find_gamut_boundary(
      p3_red.l, hue, RgbColorSpace::Srgb);
  check(boundary.converged && boundary.lower_chroma == boundary.chroma,
        "the boundary returns the conservative side of a converged bracket");
  check(boundary.bracket_width <= 1e-10,
        "the boundary bracket meets the declared tolerance");
  check_near(boundary.chroma, 93.86561347147861, 2e-9,
             "the P3-red ray pins the first sRGB exit");

  // This legal Display-P3 ray briefly exits and re-enters sRGB. Selecting the
  // first transition, rather than the largest later in-gamut chroma, is the
  // contract that makes the solver suitable for radial mapping.
  constexpr double kLightness = 96.23855934179699;
  constexpr double kHue = 1.8012425907027048;
  constexpr double kChroma = 57.69;
  const Lab narrow{kLightness, kChroma * std::cos(kHue),
                   kChroma * std::sin(kHue)};
  check(camera_iq::is_in_unit_gamut(
            camera_iq::xyz_to_linear_rgb(
                camera_iq::lab_to_xyz(narrow, white), RgbColorSpace::DisplayP3),
            1e-12),
        "the narrow-exit fixture is a legal Display-P3 color");
  check(!camera_iq::is_in_unit_gamut(
            camera_iq::xyz_to_linear_rgb(
                camera_iq::lab_to_xyz(narrow, white), RgbColorSpace::Srgb),
            1e-12),
        "the narrow-exit fixture lies outside sRGB");
  const auto narrow_boundary = camera_iq::find_gamut_boundary(
      kLightness, kHue, RgbColorSpace::Srgb);
  check(narrow_boundary.chroma > 57.5 && narrow_boundary.chroma < kChroma,
        "the solver finds the narrow first exit before re-entry");

  auto soft = options(GamutMapIntent::SoftChromaCompression);
  soft.knee_fraction = 0.75;
  const double destination = camera_iq::gamut_boundary_chroma(
      p3_red.l, hue, RgbColorSpace::Srgb);
  const double knee = soft.knee_fraction * destination;
  const Lab inside_core{p3_red.l, (knee - 1e-5) * std::cos(hue),
                        (knee - 1e-5) * std::sin(hue)};
  const auto protected_color =
      camera_iq::map_d65_lab_to_gamut(inside_core, soft);
  check(protected_color.branch == GamutMapBranch::ProtectedCoreIdentity,
        "soft compression preserves the declared core");
  check_near(protected_color.output_chroma, knee - 1e-5, 1e-10,
             "soft compression is identity below the knee");

  soft.knee_fraction = 1.0;
  check(throws([&] {
          (void)camera_iq::map_encoded_rgb_to_gamut({1, 0, 0}, soft);
        }),
        "a knee with no compression span is refused");
}

void test_adversarial_contract() {
  const auto radial_options = options(GamutMapIntent::BoundaryProjection);
  const auto soft_options = options(GamutMapIntent::SoftChromaCompression);
  const auto oklch_options = options(GamutMapIntent::OklchBoundaryProjection);
  const auto local_options = options(GamutMapIntent::CssColor4LocalMinde);

  bool no_throw = true;
  bool finite = true;
  bool in_gamut = true;
  bool cielab_contract = true;
  bool oklch_contract = true;
  bool hard_identity = true;
  double maximum_lab_hue_shift = 0.0;
  std::size_t count = 0;

  const auto visit = [&](const EncodedRgb& sample) {
    ++count;
    try {
      const auto radial =
          camera_iq::map_encoded_rgb_to_gamut(sample, radial_options);
      const auto soft =
          camera_iq::map_encoded_rgb_to_gamut(sample, soft_options);
      const auto oklch =
          camera_iq::map_encoded_rgb_to_gamut(sample, oklch_options);
      const auto local =
          camera_iq::map_encoded_rgb_to_gamut(sample, local_options);
      const std::array<GamutMappingResult, 4> results{radial, soft, oklch,
                                                        local};
      for (const auto& result : results) {
        finite = finite && std::isfinite(result.output_encoded.r) &&
                 std::isfinite(result.output_encoded.g) &&
                 std::isfinite(result.output_encoded.b);
        in_gamut = in_gamut && result.output_in_destination &&
                   camera_iq::is_in_unit_gamut(
                       result.destination_linear_after, 1e-12);
        if (result.mapping_coordinate_space ==
            GamutMappingCoordinateSpace::CielabD65) {
          cielab_contract =
              cielab_contract &&
              result.output_chroma <= result.input_chroma + 1e-10 &&
              std::abs(result.output_lab.l - result.input_lab.l) <= 1e-12;
          if (result.input_chroma > 1e-6 && result.output_chroma > 1e-6) {
            const double delta = std::abs(std::atan2(
                std::sin(hue_radians(result.output_lab) -
                         hue_radians(result.input_lab)),
                std::cos(hue_radians(result.output_lab) -
                         hue_radians(result.input_lab))));
            maximum_lab_hue_shift = std::max(maximum_lab_hue_shift, delta);
            cielab_contract = cielab_contract && delta <= 2e-7;
          }
        }
      }
      oklch_contract =
          oklch_contract &&
          oklch.output_mapping_chroma <= oklch.input_mapping_chroma + 1e-12 &&
          std::abs(oklch.output_oklch.l - oklch.input_oklch.l) <= 1e-12 &&
          (!oklch.input_oklch.hue_defined ||
           std::abs(oklch.output_oklch.h_degrees -
                    oklch.input_oklch.h_degrees) <= 1e-8);
      if (radial.input_in_destination) {
        hard_identity = hard_identity && !radial.modified;
      }
      if (oklch.input_in_destination) {
        hard_identity = hard_identity && !oklch.modified;
      }
      if (local.input_in_destination) {
        hard_identity = hard_identity && !local.modified;
      }
    } catch (const std::exception&) {
      no_throw = false;
    }
  };

  const std::array<double, 9> boundary_values{
      0.0,
      1e-12,
      std::nextafter(0.04045, 0.0),
      0.04045,
      std::nextafter(0.04045, 1.0),
      0.25,
      0.5,
      std::nextafter(1.0, 0.0),
      1.0,
  };
  for (double r : boundary_values) {
    for (double g : boundary_values) {
      for (double b : boundary_values) visit({r, g, b});
    }
  }

  std::uint64_t state = 0xd1b54a32d192ed03ULL;
  const auto next_unit = [&]() {
    state = state * 6364136223846793005ULL + 1442695040888963407ULL;
    return static_cast<double>(state >> 11) * 0x1.0p-53;
  };
  for (int i = 0; i < 2000; ++i) {
    visit({next_unit(), next_unit(), next_unit()});
  }
  for (int i = 0; i < 500; ++i) {
    const double center = (static_cast<double>(i) + 0.5) / 500.0;
    const double offset = static_cast<double>((i % 5) - 2) * 1e-10;
    visit({std::clamp(center + offset, 0.0, 1.0), center,
           std::clamp(center - offset, 0.0, 1.0)});
  }

  check(count == 3229, "the deterministic adversarial set has 3,229 inputs");
  check(no_throw, "every legal adversarial input maps without an exception");
  check(finite, "every adversarial output is finite");
  check(in_gamut, "every adversarial output is independently in sRGB");
  check(cielab_contract && maximum_lab_hue_shift <= 2e-7,
        "the scoped CIELAB radial contracts hold");
  check(oklch_contract, "the scoped OkLCh radial contracts hold");
  check(hard_identity, "all hard intents preserve destination-gamut inputs");
}

void test_refusals() {
  const auto radial = options(GamutMapIntent::BoundaryProjection);
  check(throws([&] {
          (void)camera_iq::map_encoded_rgb_to_gamut(
              {std::numeric_limits<double>::quiet_NaN(), 0, 0}, radial);
        }),
        "a non-finite encoded input is refused");
  check(throws([&] {
          (void)camera_iq::map_encoded_rgb_to_gamut({1.01, 0, 0}, radial);
        }),
        "an encoded input outside [0,1] is refused");
  check(throws([&] {
          (void)camera_iq::map_d65_lab_to_gamut(
              {50, std::numeric_limits<double>::infinity(), 0}, radial);
        }),
        "a non-finite Lab input is refused");

  auto invalid_boundary = radial;
  invalid_boundary.boundary.gamut_tolerance = 0.0;
  check(throws([&] {
          (void)camera_iq::map_encoded_rgb_to_gamut({1, 0, 0},
                                                    invalid_boundary);
        }),
        "a zero gamut tolerance is refused");

  camera_iq::GamutBoundaryOptions exhausted;
  exhausted.refinement_iterations = 1;
  exhausted.chroma_tolerance = 1e-12;
  const auto white = camera_iq::d65_white_xyz();
  const Lab p3_red = camera_iq::xyz_to_lab(
      camera_iq::linear_rgb_to_xyz({1, 0, 0}, RgbColorSpace::DisplayP3),
      white);
  check(throws([&] {
          (void)camera_iq::gamut_boundary_chroma(
              p3_red.l, hue_radians(p3_red), RgbColorSpace::Srgb, exhausted);
        }),
        "an unconverged boundary search is refused");
}

}  // namespace

int main() {
  test_transforms_and_common_gamut_identity();
  test_coordinate_and_algorithm_separation();
  test_first_exit_boundary_and_soft_knee();
  test_adversarial_contract();
  test_refusals();
  if (failures != 0) {
    std::cerr << failures << " gamut-mapping test(s) failed\n";
    return EXIT_FAILURE;
  }
  std::cout << "gamut-mapping tests passed\n";
  return EXIT_SUCCESS;
}
