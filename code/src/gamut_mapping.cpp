#include "camera_iq/gamut_mapping.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace camera_iq {
namespace {

using Matrix3 = std::array<std::array<double, 3>, 3>;

// D65 matrices and transfer functions are transcribed from the rational
// non-normative sample conversion code in W3C CSS Color Module Level 4:
// https://www.w3.org/TR/css-color-4/#color-conversion-code
constexpr Matrix3 kSrgbToXyz = {{{506752.0 / 1228815.0,
                                  87881.0 / 245763.0,
                                  12673.0 / 70218.0},
                                 {87098.0 / 409605.0,
                                  175762.0 / 245763.0,
                                  12673.0 / 175545.0},
                                 {7918.0 / 409605.0,
                                  87881.0 / 737289.0,
                                  1001167.0 / 1053270.0}}};

constexpr Matrix3 kXyzToSrgb = {{{12831.0 / 3959.0, -329.0 / 214.0,
                                  -1974.0 / 3959.0},
                                 {-851781.0 / 878810.0,
                                  1648619.0 / 878810.0,
                                  36519.0 / 878810.0},
                                 {705.0 / 12673.0, -2585.0 / 12673.0,
                                  705.0 / 667.0}}};

constexpr Matrix3 kDisplayP3ToXyz = {{{608311.0 / 1250200.0,
                                       189793.0 / 714400.0,
                                       198249.0 / 1000160.0},
                                      {35783.0 / 156275.0,
                                       247089.0 / 357200.0,
                                       198249.0 / 2500400.0},
                                      {0.0, 32229.0 / 714400.0,
                                       5220557.0 / 5000800.0}}};

constexpr Matrix3 kXyzToDisplayP3 = {{{446124.0 / 178915.0,
                                       -333277.0 / 357830.0,
                                       -72051.0 / 178915.0},
                                      {-14852.0 / 17905.0,
                                       63121.0 / 35810.0, 423.0 / 17905.0},
                                      {11844.0 / 330415.0,
                                       -50337.0 / 660830.0,
                                       316169.0 / 330415.0}}};

constexpr Matrix3 kOklabLmsToXyz = {{{1.2268798758459243,
                                       -0.5578149944602171,
                                       0.2813910456659647},
                                      {-0.0405757452148008,
                                       1.1122868032803170,
                                       -0.0717110580655164},
                                      {-0.0763729366746601,
                                       -0.4214933324022432,
                                       1.5869240198367816}}};

constexpr Matrix3 kOklabToLms = {{{1.0, 0.3963377773761749,
                                    0.2158037573099136},
                                   {1.0, -0.1055613458156586,
                                    -0.0638541728258133},
                                   {1.0, -0.0894841775298119,
                                    -1.2914855480194092}}};

bool finite(double value) { return std::isfinite(value); }

bool finite(const EncodedRgb& rgb) {
  return finite(rgb.r) && finite(rgb.g) && finite(rgb.b);
}

bool finite(const LinearRgb& rgb) {
  return finite(rgb.r) && finite(rgb.g) && finite(rgb.b);
}

bool finite(const Xyz& xyz) {
  return finite(xyz.x) && finite(xyz.y) && finite(xyz.z);
}

bool finite(const Lab& lab) {
  return finite(lab.l) && finite(lab.a) && finite(lab.b);
}

const Matrix3& to_xyz_matrix(RgbColorSpace space) {
  switch (space) {
    case RgbColorSpace::Srgb:
      return kSrgbToXyz;
    case RgbColorSpace::DisplayP3:
      return kDisplayP3ToXyz;
  }
  throw std::runtime_error("gamut mapping: unsupported RGB color space");
}

const Matrix3& from_xyz_matrix(RgbColorSpace space) {
  switch (space) {
    case RgbColorSpace::Srgb:
      return kXyzToSrgb;
    case RgbColorSpace::DisplayP3:
      return kXyzToDisplayP3;
  }
  throw std::runtime_error("gamut mapping: unsupported RGB color space");
}

LinearRgb multiply(const Matrix3& matrix, const LinearRgb& rgb) {
  return {
      matrix[0][0] * rgb.r + matrix[0][1] * rgb.g +
          matrix[0][2] * rgb.b,
      matrix[1][0] * rgb.r + matrix[1][1] * rgb.g +
          matrix[1][2] * rgb.b,
      matrix[2][0] * rgb.r + matrix[2][1] * rgb.g +
          matrix[2][2] * rgb.b,
  };
}

Xyz multiply_xyz(const Matrix3& matrix, const LinearRgb& rgb) {
  const auto values = multiply(matrix, rgb);
  return {values.r, values.g, values.b};
}

LinearRgb multiply_rgb(const Matrix3& matrix, const Xyz& xyz) {
  return multiply(matrix, {xyz.x, xyz.y, xyz.z});
}

double decode_component(double value) {
  if (value <= 0.04045) return value / 12.92;
  return std::pow((value + 0.055) / 1.055, 2.4);
}

double encode_component(double value) {
  if (value <= 0.0031308) return 12.92 * value;
  return 1.055 * std::pow(value, 1.0 / 2.4) - 0.055;
}

void validate_unit(double value, const char* context) {
  if (!finite(value) || value < 0.0 || value > 1.0) {
    throw std::runtime_error(std::string(context) +
                             ": components must be finite and within [0,1]");
  }
}

void validate_boundary_options(const GamutBoundaryOptions& options) {
  if (!finite(options.maximum_chroma) || options.maximum_chroma <= 0.0 ||
      options.refinement_iterations == 0 ||
      !finite(options.chroma_tolerance) || options.chroma_tolerance <= 0.0 ||
      !finite(options.gamut_tolerance) || options.gamut_tolerance <= 0.0) {
    throw std::runtime_error("gamut boundary: invalid search options");
  }
}

Lab lab_at(double lightness, double chroma, double hue_radians) {
  return {lightness, chroma * std::cos(hue_radians),
          chroma * std::sin(hue_radians)};
}

double lab_chroma(const Lab& lab) { return std::hypot(lab.a, lab.b); }

double lab_hue(const Lab& lab) {
  if (lab.a == 0.0 && lab.b == 0.0) return 0.0;
  return std::atan2(lab.b, lab.a);
}

LinearRgb clamp_unit(const LinearRgb& rgb) {
  return {std::clamp(rgb.r, 0.0, 1.0), std::clamp(rgb.g, 0.0, 1.0),
          std::clamp(rgb.b, 0.0, 1.0)};
}

struct CubicPolynomial {
  // c[0] + c[1] x + c[2] x^2 + c[3] x^3
  std::array<double, 4> c{};
};

double evaluate(const CubicPolynomial& polynomial, double x) {
  return ((polynomial.c[3] * x + polynomial.c[2]) * x +
          polynomial.c[1]) *
             x +
         polynomial.c[0];
}

CubicPolynomial lab_inverse_polynomial(double offset, double slope,
                                       bool cubic_branch) {
  CubicPolynomial out;
  if (cubic_branch) {
    out.c = {offset * offset * offset,
             3.0 * offset * offset * slope,
             3.0 * offset * slope * slope, slope * slope * slope};
  } else {
    constexpr double delta = 6.0 / 29.0;
    constexpr double factor = 3.0 * delta * delta;
    out.c = {factor * (offset - 4.0 / 29.0), factor * slope, 0.0,
             0.0};
  }
  return out;
}

void add_scaled(CubicPolynomial& destination,
                const CubicPolynomial& source, double scale) {
  for (std::size_t i = 0; i < destination.c.size(); ++i) {
    destination.c[i] += scale * source.c[i];
  }
}

std::vector<double> derivative_critical_points(
    const CubicPolynomial& polynomial, double lower, double upper) {
  const double a = 3.0 * polynomial.c[3];
  const double b = 2.0 * polynomial.c[2];
  const double c = polynomial.c[1];
  const double scale = std::max({std::abs(a), std::abs(b), std::abs(c), 1.0});
  const double epsilon = 32.0 * std::numeric_limits<double>::epsilon() * scale;
  std::vector<double> roots;
  const auto add = [&](double root) {
    if (finite(root) && root > lower && root < upper) roots.push_back(root);
  };
  if (std::abs(a) <= epsilon) {
    if (std::abs(b) > epsilon) add(-c / b);
    return roots;
  }
  double discriminant = b * b - 4.0 * a * c;
  if (discriminant < -epsilon * scale) return roots;
  discriminant = std::max(0.0, discriminant);
  const double square_root = std::sqrt(discriminant);
  const double q = -0.5 * (b + std::copysign(square_root, b));
  if (q == 0.0) {
    add(-b / (2.0 * a));
  } else {
    add(q / a);
    add(c / q);
  }
  std::sort(roots.begin(), roots.end());
  roots.erase(std::unique(roots.begin(), roots.end()), roots.end());
  return roots;
}

void append_crossing_roots(const CubicPolynomial& channel,
                           double channel_boundary, double segment_lower,
                           double segment_upper, double chroma_tolerance,
                           std::vector<double>& roots) {
  CubicPolynomial shifted = channel;
  shifted.c[0] -= channel_boundary;
  std::vector<double> points = {segment_lower};
  const auto critical =
      derivative_critical_points(shifted, segment_lower, segment_upper);
  points.insert(points.end(), critical.begin(), critical.end());
  points.push_back(segment_upper);

  const double value_tolerance = 64.0 * std::numeric_limits<double>::epsilon();
  for (std::size_t i = 0; i + 1 < points.size(); ++i) {
    double lower = points[i];
    double upper = points[i + 1];
    double lower_value = evaluate(shifted, lower);
    double upper_value = evaluate(shifted, upper);
    if (std::abs(lower_value) <= value_tolerance) roots.push_back(lower);
    if (std::abs(upper_value) <= value_tolerance) roots.push_back(upper);
    if ((lower_value < 0.0) == (upper_value < 0.0)) continue;
    while (upper - lower > chroma_tolerance) {
      const double mid = 0.5 * (lower + upper);
      if (mid == lower || mid == upper) break;
      const double mid_value = evaluate(shifted, mid);
      if ((lower_value < 0.0) == (mid_value < 0.0)) {
        lower = mid;
        lower_value = mid_value;
      } else {
        upper = mid;
        upper_value = mid_value;
      }
    }
    roots.push_back(0.5 * (lower + upper));
  }
}

std::vector<double> channel_surface_crossings(
    double lightness, double hue_radians, RgbColorSpace space,
    const GamutBoundaryOptions& options) {
  constexpr double delta = 6.0 / 29.0;
  const double fy = (lightness + 16.0) / 116.0;
  const double x_slope = std::cos(hue_radians) / 500.0;
  const double z_slope = -std::sin(hue_radians) / 200.0;

  std::vector<double> partitions = {0.0, options.maximum_chroma};
  const auto add_breakpoint = [&](double slope) {
    if (slope == 0.0) return;
    const double crossing = (delta - fy) / slope;
    if (crossing > 0.0 && crossing < options.maximum_chroma) {
      partitions.push_back(crossing);
    }
  };
  add_breakpoint(x_slope);
  add_breakpoint(z_slope);
  std::sort(partitions.begin(), partitions.end());

  std::vector<double> roots = partitions;
  const Matrix3& matrix = from_xyz_matrix(space);
  const Xyz white{0.9504559270516717, 1.0, 1.0890577507598784};
  const double y = lab_to_xyz({lightness, 0.0, 0.0}, white).y;
  for (std::size_t segment = 0; segment + 1 < partitions.size(); ++segment) {
    const double lower = partitions[segment];
    const double upper = partitions[segment + 1];
    const double midpoint = 0.5 * (lower + upper);
    const auto x_base = lab_inverse_polynomial(
        fy, x_slope, fy + x_slope * midpoint > delta);
    const auto z_base = lab_inverse_polynomial(
        fy, z_slope, fy + z_slope * midpoint > delta);
    for (std::size_t channel = 0; channel < 3; ++channel) {
      CubicPolynomial polynomial;
      add_scaled(polynomial, x_base, matrix[channel][0] * white.x);
      polynomial.c[0] += matrix[channel][1] * y;
      add_scaled(polynomial, z_base, matrix[channel][2] * white.z);
      append_crossing_roots(polynomial, -options.gamut_tolerance, lower,
                            upper, options.chroma_tolerance, roots);
      append_crossing_roots(polynomial, 1.0 + options.gamut_tolerance, lower,
                            upper, options.chroma_tolerance, roots);
    }
  }
  std::sort(roots.begin(), roots.end());
  const double merge_tolerance = options.chroma_tolerance * 0.5;
  roots.erase(std::unique(roots.begin(), roots.end(),
                          [&](double first, double second) {
                            return std::abs(first - second) <= merge_tolerance;
                          }),
              roots.end());
  return roots;
}

Matrix3 multiply_matrices(const Matrix3& first, const Matrix3& second) {
  Matrix3 result{};
  for (std::size_t row = 0; row < 3; ++row) {
    for (std::size_t column = 0; column < 3; ++column) {
      for (std::size_t inner = 0; inner < 3; ++inner) {
        result[row][column] += first[row][inner] * second[inner][column];
      }
    }
  }
  return result;
}

std::vector<double> oklch_channel_surface_crossings(
    double lightness, double hue_radians, RgbColorSpace space,
    const GamutBoundaryOptions& options) {
  const double cosine = std::cos(hue_radians);
  const double sine = std::sin(hue_radians);
  const Matrix3 lms_to_rgb =
      multiply_matrices(from_xyz_matrix(space), kOklabLmsToXyz);
  std::array<CubicPolynomial, 3> lms_cubed;
  for (std::size_t lms = 0; lms < 3; ++lms) {
    const double offset = kOklabToLms[lms][0] * lightness;
    const double slope =
        kOklabToLms[lms][1] * cosine + kOklabToLms[lms][2] * sine;
    lms_cubed[lms] = lab_inverse_polynomial(offset, slope, true);
  }

  std::vector<double> roots = {0.0, options.maximum_chroma};
  for (std::size_t channel = 0; channel < 3; ++channel) {
    CubicPolynomial polynomial;
    for (std::size_t lms = 0; lms < 3; ++lms) {
      add_scaled(polynomial, lms_cubed[lms], lms_to_rgb[channel][lms]);
    }
    append_crossing_roots(polynomial, -options.gamut_tolerance, 0.0,
                          options.maximum_chroma,
                          options.chroma_tolerance, roots);
    append_crossing_roots(polynomial, 1.0 + options.gamut_tolerance, 0.0,
                          options.maximum_chroma,
                          options.chroma_tolerance, roots);
  }
  std::sort(roots.begin(), roots.end());
  const double merge_tolerance = options.chroma_tolerance * 0.5;
  roots.erase(std::unique(roots.begin(), roots.end(),
                          [&](double first, double second) {
                            return std::abs(first - second) <= merge_tolerance;
                          }),
              roots.end());
  return roots;
}

}  // namespace

Xyz d65_white_xyz() {
  return {0.9504559270516717, 1.0, 1.0890577507598784};
}

LinearRgb decode_rgb(const EncodedRgb& encoded) {
  validate_unit(encoded.r, "RGB decode");
  validate_unit(encoded.g, "RGB decode");
  validate_unit(encoded.b, "RGB decode");
  return {decode_component(encoded.r), decode_component(encoded.g),
          decode_component(encoded.b)};
}

EncodedRgb encode_rgb(const LinearRgb& linear) {
  validate_unit(linear.r, "RGB encode");
  validate_unit(linear.g, "RGB encode");
  validate_unit(linear.b, "RGB encode");
  return {encode_component(linear.r), encode_component(linear.g),
          encode_component(linear.b)};
}

Xyz linear_rgb_to_xyz(const LinearRgb& rgb, RgbColorSpace space) {
  if (!finite(rgb)) {
    throw std::runtime_error("RGB to XYZ: components must be finite");
  }
  return multiply_xyz(to_xyz_matrix(space), rgb);
}

LinearRgb xyz_to_linear_rgb(const Xyz& xyz, RgbColorSpace space) {
  if (!finite(xyz)) {
    throw std::runtime_error("XYZ to RGB: components must be finite");
  }
  return multiply_rgb(from_xyz_matrix(space), xyz);
}

bool is_in_unit_gamut(const LinearRgb& rgb, double tolerance) {
  if (!finite(tolerance) || tolerance < 0.0) {
    throw std::runtime_error("gamut predicate: tolerance must be finite and non-negative");
  }
  if (!finite(rgb)) return false;
  return rgb.r >= -tolerance && rgb.r <= 1.0 + tolerance &&
         rgb.g >= -tolerance && rgb.g <= 1.0 + tolerance &&
         rgb.b >= -tolerance && rgb.b <= 1.0 + tolerance;
}

GamutBoundaryResult find_gamut_boundary(
    double lightness, double hue_radians, RgbColorSpace space,
    const GamutBoundaryOptions& options) {
  validate_boundary_options(options);
  constexpr double endpoint_tolerance = 1e-10;
  if (!finite(lightness) || !finite(hue_radians) ||
      lightness < -endpoint_tolerance ||
      lightness > 100.0 + endpoint_tolerance) {
    throw std::runtime_error(
        "gamut boundary: lightness must be in [0,100] and hue must be finite");
  }
  lightness = std::clamp(lightness, 0.0, 100.0);

  const auto in_gamut = [&](double chroma) {
    const Xyz xyz = lab_to_xyz(lab_at(lightness, chroma, hue_radians),
                               d65_white_xyz());
    return is_in_unit_gamut(xyz_to_linear_rgb(xyz, space),
                            options.gamut_tolerance);
  };
  if (!in_gamut(0.0)) {
    throw std::runtime_error(
        "gamut boundary: neutral point is outside the RGB gamut");
  }

  const auto crossings =
      channel_surface_crossings(lightness, hue_radians, space, options);
  double previous_in_gamut_probe = 0.0;
  std::size_t segments_examined = 0;
  for (std::size_t i = 0; i + 1 < crossings.size(); ++i) {
    if (crossings[i + 1] <= crossings[i]) continue;
    ++segments_examined;
    const double probe = 0.5 * (crossings[i] + crossings[i + 1]);
    if (in_gamut(probe)) {
      previous_in_gamut_probe = probe;
      continue;
    }

    double low = previous_in_gamut_probe;
    double high = probe;
    std::size_t refinement_iterations = 0;
    while (refinement_iterations < options.refinement_iterations &&
           high - low > options.chroma_tolerance) {
      const double mid = 0.5 * (low + high);
      if (mid == low || mid == high) break;
      if (in_gamut(mid)) {
        low = mid;
      } else {
        high = mid;
      }
      ++refinement_iterations;
    }
    const bool converged = high - low <= options.chroma_tolerance ||
                           std::nextafter(low, high) == high;
    return {low, low, high, high - low, segments_examined,
            refinement_iterations, converged};
  }
  throw std::runtime_error(
      "gamut boundary: no boundary found before maximum_chroma");
}

GamutBoundaryResult find_oklch_gamut_boundary(
    double lightness, double hue_radians, RgbColorSpace space,
    const GamutBoundaryOptions& options) {
  validate_boundary_options(options);
  constexpr double endpoint_tolerance = 1e-12;
  if (!finite(lightness) || !finite(hue_radians) ||
      lightness < -endpoint_tolerance ||
      lightness > 1.0 + endpoint_tolerance) {
    throw std::runtime_error(
        "OkLCh gamut boundary: lightness must be in [0,1] and hue must be finite");
  }
  lightness = std::clamp(lightness, 0.0, 1.0);
  const auto in_gamut = [&](double chroma) {
    const Oklab oklab{lightness, chroma * std::cos(hue_radians),
                      chroma * std::sin(hue_radians)};
    return is_in_unit_gamut(
        xyz_to_linear_rgb(oklab_to_xyz_d65(oklab), space),
        options.gamut_tolerance);
  };
  if (!in_gamut(0.0)) {
    throw std::runtime_error(
        "OkLCh gamut boundary: neutral point is outside the RGB gamut");
  }

  const auto crossings =
      oklch_channel_surface_crossings(lightness, hue_radians, space, options);
  double previous_in_gamut_probe = 0.0;
  std::size_t segments_examined = 0;
  for (std::size_t i = 0; i + 1 < crossings.size(); ++i) {
    if (crossings[i + 1] <= crossings[i]) continue;
    ++segments_examined;
    const double probe = 0.5 * (crossings[i] + crossings[i + 1]);
    if (in_gamut(probe)) {
      previous_in_gamut_probe = probe;
      continue;
    }
    double low = previous_in_gamut_probe;
    double high = probe;
    std::size_t refinement_iterations = 0;
    while (refinement_iterations < options.refinement_iterations &&
           high - low > options.chroma_tolerance) {
      const double mid = 0.5 * (low + high);
      if (mid == low || mid == high) break;
      if (in_gamut(mid)) {
        low = mid;
      } else {
        high = mid;
      }
      ++refinement_iterations;
    }
    const bool converged = high - low <= options.chroma_tolerance ||
                           std::nextafter(low, high) == high;
    return {low, low, high, high - low, segments_examined,
            refinement_iterations, converged};
  }
  throw std::runtime_error(
      "OkLCh gamut boundary: no boundary found before maximum_chroma");
}

double gamut_boundary_chroma(double lightness, double hue_radians,
                             RgbColorSpace space,
                             const GamutBoundaryOptions& options) {
  const auto result =
      find_gamut_boundary(lightness, hue_radians, space, options);
  if (!result.converged) {
    throw std::runtime_error(
        "gamut boundary: refinement did not meet chroma_tolerance");
  }
  return result.chroma;
}

namespace {

struct LocalClip {
  LinearRgb linear;
  Xyz xyz;
  Oklab oklab;
  double delta_e_ok = 0;
};

GamutMappingResult map_d65_xyz_with_oklch(const Xyz& input_xyz,
                                           const GamutMapOptions& options) {
  if (!finite(input_xyz)) {
    throw std::runtime_error("OkLCh gamut mapping: input XYZ must be finite");
  }
  validate_boundary_options(options.oklch_boundary);

  GamutMappingResult result;
  result.mapping_coordinate_space = GamutMappingCoordinateSpace::OklabD65;
  result.input_xyz = input_xyz;
  result.input_lab = xyz_to_lab(input_xyz, d65_white_xyz());
  result.input_oklab = xyz_d65_to_oklab(input_xyz);
  result.input_oklch = oklab_to_oklch(result.input_oklab);
  result.input_chroma = lab_chroma(result.input_lab);
  result.input_mapping_chroma = result.input_oklch.c;

  const LinearRgb source_linear =
      xyz_to_linear_rgb(input_xyz, options.source);
  if (!is_in_unit_gamut(source_linear,
                        options.oklch_boundary.gamut_tolerance)) {
    throw std::runtime_error(
        "OkLCh gamut mapping: input is outside the declared source gamut");
  }
  result.destination_linear_before =
      xyz_to_linear_rgb(input_xyz, options.destination);
  result.input_in_destination = is_in_unit_gamut(
      result.destination_linear_before,
      options.oklch_boundary.gamut_tolerance);

  const double hue_radians =
      result.input_oklch.hue_defined
          ? result.input_oklch.h_degrees *
                3.141592653589793238462643383279502884 / 180.0
          : 0.0;
  Oklab output_oklab = result.input_oklab;

  if (options.intent == GamutMapIntent::OklchBoundaryProjection) {
    result.boundary_diagnostics_applicable = result.input_oklch.hue_defined;
    if (result.input_oklch.hue_defined) {
      result.source_boundary = find_oklch_gamut_boundary(
          result.input_oklch.l, hue_radians, options.source,
          options.oklch_boundary);
      result.destination_boundary = find_oklch_gamut_boundary(
          result.input_oklch.l, hue_radians, options.destination,
          options.oklch_boundary);
      if (!result.source_boundary.converged ||
          !result.destination_boundary.converged) {
        throw std::runtime_error(
            "OkLCh gamut mapping: boundary refinement did not converge");
      }
      result.source_boundary_chroma = result.source_boundary.chroma;
      result.destination_boundary_chroma = result.destination_boundary.chroma;
      if (!result.input_in_destination) {
        const double output_chroma = std::min(
            result.input_oklch.c, result.destination_boundary_chroma);
        result.branch = GamutMapBranch::FixedOklchRadialBoundaryClip;
        output_oklab = {result.input_oklch.l,
                        output_chroma * std::cos(hue_radians),
                        output_chroma * std::sin(hue_radians)};
      }
    } else if (!result.input_in_destination) {
      throw std::runtime_error(
          "OkLCh gamut mapping: missing hue cannot define an out-of-gamut ray");
    }
  } else if (options.intent == GamutMapIntent::CssColor4LocalMinde) {
    result.boundary_diagnostics_applicable = false;
    result.local_minde.applicable = true;
    constexpr double jnd = 0.02;
    constexpr double epsilon = 0.0001;
    result.local_minde.jnd = jnd;
    result.local_minde.epsilon = epsilon;

    const auto clip = [&](const Oklab& current) {
      LocalClip clipped;
      const Xyz current_xyz = oklab_to_xyz_d65(current);
      clipped.linear = clamp_unit(
          xyz_to_linear_rgb(current_xyz, options.destination));
      clipped.xyz = linear_rgb_to_xyz(clipped.linear, options.destination);
      clipped.oklab = xyz_d65_to_oklab(clipped.xyz);
      clipped.delta_e_ok = delta_e_ok(clipped.oklab, current);
      return clipped;
    };

    if (!result.input_in_destination) {
      if (result.input_oklch.l >= 1.0) {
        output_oklab = {1.0, 0.0, 0.0};
        result.branch = GamutMapBranch::CssColor4LocalMindeInitialClip;
        result.local_minde.returned_clipped_color = true;
      } else if (result.input_oklch.l <= 0.0) {
        output_oklab = {0.0, 0.0, 0.0};
        result.branch = GamutMapBranch::CssColor4LocalMindeInitialClip;
        result.local_minde.returned_clipped_color = true;
      } else {
        Oklab current = result.input_oklab;
        LocalClip clipped = clip(current);
        if (clipped.delta_e_ok < jnd) {
          output_oklab = clipped.oklab;
          result.branch = GamutMapBranch::CssColor4LocalMindeInitialClip;
          result.local_minde.returned_clipped_color = true;
          result.local_minde.final_delta_e_ok = clipped.delta_e_ok;
        } else {
          double minimum = 0.0;
          double maximum = result.input_oklch.c;
          bool minimum_in_gamut = true;
          while (maximum - minimum > epsilon) {
            ++result.local_minde.iterations;
            const double chroma = 0.5 * (minimum + maximum);
            current = {result.input_oklch.l, chroma * std::cos(hue_radians),
                       chroma * std::sin(hue_radians)};
            if (minimum_in_gamut &&
                is_in_unit_gamut(
                    xyz_to_linear_rgb(oklab_to_xyz_d65(current),
                                      options.destination),
                    options.oklch_boundary.gamut_tolerance)) {
              minimum = chroma;
              continue;
            }
            clipped = clip(current);
            if (clipped.delta_e_ok < jnd) {
              if (jnd - clipped.delta_e_ok < epsilon) break;
              minimum_in_gamut = false;
              minimum = chroma;
            } else {
              maximum = chroma;
            }
          }
          output_oklab = clipped.oklab;
          result.branch = GamutMapBranch::CssColor4LocalMindeBinarySearch;
          result.local_minde.returned_clipped_color = true;
          result.local_minde.final_delta_e_ok = clipped.delta_e_ok;
        }
      }
    }
  } else {
    throw std::runtime_error(
        "OkLCh gamut mapping: unsupported rendering intent");
  }

  result.output_oklab = output_oklab;
  result.output_oklch = oklab_to_oklch(output_oklab);
  result.output_xyz = oklab_to_xyz_d65(output_oklab);
  result.output_lab = xyz_to_lab(result.output_xyz, d65_white_xyz());
  result.output_chroma = lab_chroma(result.output_lab);
  result.output_mapping_chroma = result.output_oklch.c;
  result.destination_linear_after =
      xyz_to_linear_rgb(result.output_xyz, options.destination);
  result.output_in_destination = is_in_unit_gamut(
      result.destination_linear_after,
      options.oklch_boundary.gamut_tolerance);
  if (!result.output_in_destination) {
    throw std::runtime_error(
        "OkLCh gamut mapping: mapped result is outside the destination gamut");
  }
  result.modified =
      std::abs(result.output_lab.l - result.input_lab.l) > 1e-12 ||
      std::abs(result.output_lab.a - result.input_lab.a) > 1e-12 ||
      std::abs(result.output_lab.b - result.input_lab.b) > 1e-12;
  result.output_encoded =
      encode_rgb(clamp_unit(result.destination_linear_after));
  return result;
}

}  // namespace

GamutMappingResult map_d65_lab_to_gamut(const Lab& input,
                                        const GamutMapOptions& options) {
  if (!finite(input)) {
    throw std::runtime_error("gamut mapping: input Lab must be finite");
  }
  constexpr double endpoint_tolerance = 1e-10;
  if (input.l < -endpoint_tolerance ||
      input.l > 100.0 + endpoint_tolerance) {
    throw std::runtime_error(
        "gamut mapping: input Lab lightness must be in [0,100]");
  }
  Lab normalized_input = input;
  normalized_input.l = std::clamp(normalized_input.l, 0.0, 100.0);

  if (options.intent == GamutMapIntent::OklchBoundaryProjection ||
      options.intent == GamutMapIntent::CssColor4LocalMinde) {
    return map_d65_xyz_with_oklch(
        lab_to_xyz(normalized_input, d65_white_xyz()), options);
  }
  if (!finite(options.knee_fraction) || options.knee_fraction < 0.0 ||
      options.knee_fraction >= 1.0) {
    throw std::runtime_error("gamut mapping: knee_fraction must be in [0,1)");
  }
  validate_boundary_options(options.boundary);

  GamutMappingResult result;
  result.input_lab = normalized_input;
  result.input_xyz = lab_to_xyz(normalized_input, d65_white_xyz());
  result.input_oklab = xyz_d65_to_oklab(result.input_xyz);
  result.input_oklch = oklab_to_oklch(result.input_oklab);
  const LinearRgb source_linear =
      xyz_to_linear_rgb(result.input_xyz, options.source);
  if (!is_in_unit_gamut(source_linear, options.boundary.gamut_tolerance)) {
    throw std::runtime_error(
        "gamut mapping: input Lab is outside the declared source gamut");
  }

  result.destination_linear_before =
      xyz_to_linear_rgb(result.input_xyz, options.destination);
  result.input_in_destination = is_in_unit_gamut(
      result.destination_linear_before, options.boundary.gamut_tolerance);
  result.input_chroma = lab_chroma(normalized_input);
  result.input_mapping_chroma = result.input_chroma;
  constexpr double hue_defined_chroma = 1e-12;
  result.boundary_diagnostics_applicable =
      result.input_chroma > hue_defined_chroma;
  const double hue = result.boundary_diagnostics_applicable
                         ? lab_hue(normalized_input)
                         : 0.0;
  if (result.boundary_diagnostics_applicable) {
    result.source_boundary = find_gamut_boundary(
        normalized_input.l, hue, options.source, options.boundary);
    result.destination_boundary = find_gamut_boundary(
        normalized_input.l, hue, options.destination, options.boundary);
    if (!result.source_boundary.converged ||
        !result.destination_boundary.converged) {
      throw std::runtime_error(
          "gamut mapping: boundary refinement did not meet chroma_tolerance");
    }
    result.source_boundary_chroma = result.source_boundary.chroma;
    result.destination_boundary_chroma = result.destination_boundary.chroma;
  } else if (!result.input_in_destination) {
    throw std::runtime_error(
        "gamut mapping: missing hue cannot define an out-of-gamut ray");
  }

  double output_chroma = result.input_chroma;
  if (options.intent == GamutMapIntent::BoundaryProjection) {
    if (!result.input_in_destination) {
      output_chroma =
          std::min(result.input_chroma, result.destination_boundary_chroma);
      result.branch = GamutMapBranch::FixedLhRadialBoundaryClip;
    }
  } else if (options.intent == GamutMapIntent::SoftChromaCompression) {
    const bool source_boundary_is_wider =
        result.boundary_diagnostics_applicable &&
        result.source_boundary.lower_chroma >
        result.destination_boundary.upper_chroma;
    const bool compression_active =
        options.source != options.destination &&
        (source_boundary_is_wider || !result.input_in_destination);
    if (compression_active) {
      result.knee_chroma =
          options.knee_fraction * result.destination_boundary_chroma;
      if (result.input_chroma > result.knee_chroma) {
        result.branch = GamutMapBranch::SoftChromaCompression;
        const double destination_span =
            result.destination_boundary_chroma - result.knee_chroma;
        if (destination_span == 0.0) {
          // A representable knee_fraction below one can still round K to D.
          // In that case the only representable continuous limit is D, not
          // the neutral axis. RGB-component tolerances do not belong in this
          // CIELAB-chroma calculation.
          output_chroma = result.destination_boundary_chroma;
        } else {
          const double excess = result.input_chroma - result.knee_chroma;
          // Experimental protected-core compressor. It is C0-continuous with
          // unit slope at the knee, strictly monotone, and approaches the
          // connected destination boundary without crossing it.
          output_chroma = result.knee_chroma +
                          destination_span * excess /
                              (destination_span + excess);
        }
      } else {
        result.branch = GamutMapBranch::ProtectedCoreIdentity;
      }
    } else {
      result.branch = GamutMapBranch::IdentityNoGamutContraction;
    }
  } else {
    throw std::runtime_error("gamut mapping: unsupported rendering intent");
  }

  result.output_lab = lab_at(normalized_input.l, output_chroma, hue);
  if (result.input_chroma == 0.0) {
    result.output_lab.a = 0.0;
    result.output_lab.b = 0.0;
  }
  result.output_xyz = lab_to_xyz(result.output_lab, d65_white_xyz());
  result.destination_linear_after =
      xyz_to_linear_rgb(result.output_xyz, options.destination);
  if (!is_in_unit_gamut(result.destination_linear_after,
                        options.boundary.gamut_tolerance)) {
    throw std::runtime_error(
        "gamut mapping: mapped result is outside the destination gamut");
  }
  // Keep the declared mapping coordinate as the solver/compressor result.
  // Recomputing hypot(a,b) after the polar-to-Cartesian reconstruction can
  // move an exact radial-boundary value by one ULP and make derived diagnostics
  // such as boundary utilization configuration-dependent.
  result.output_chroma = output_chroma;
  result.output_oklab = xyz_d65_to_oklab(result.output_xyz);
  result.output_oklch = oklab_to_oklch(result.output_oklab);
  result.output_mapping_chroma = output_chroma;
  result.modified =
      std::abs(result.output_lab.l - result.input_lab.l) > 1e-12 ||
      std::abs(result.output_lab.a - result.input_lab.a) > 1e-12 ||
      std::abs(result.output_lab.b - result.input_lab.b) > 1e-12;
  result.output_in_destination = is_in_unit_gamut(
      result.destination_linear_after, options.boundary.gamut_tolerance);
  result.output_encoded =
      encode_rgb(clamp_unit(result.destination_linear_after));
  return result;
}

GamutMappingResult map_encoded_rgb_to_gamut(
    const EncodedRgb& input, const GamutMapOptions& options) {
  if (!finite(input)) {
    throw std::runtime_error("gamut mapping: encoded input must be finite");
  }
  const LinearRgb source_linear = decode_rgb(input);
  const Xyz source_xyz = linear_rgb_to_xyz(source_linear, options.source);
  if (options.intent == GamutMapIntent::OklchBoundaryProjection ||
      options.intent == GamutMapIntent::CssColor4LocalMinde) {
    return map_d65_xyz_with_oklch(source_xyz, options);
  }
  return map_d65_lab_to_gamut(xyz_to_lab(source_xyz, d65_white_xyz()),
                              options);
}

}  // namespace camera_iq
