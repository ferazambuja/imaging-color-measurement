#pragma once

#include <string>
#include <vector>

#include "camera_iq/cfa_image_view.hpp"
#include "camera_iq/roi.hpp"

namespace camera_iq {

struct SfrOptions {
  double bin_spacing_px = 0.25;
  double min_edge_angle_deg = 2.0;
  double max_edge_angle_deg = 10.0;
  double min_contrast_dn = 20.0;
  double near_saturation_fraction = 0.98;
  int min_roi_dimension_px = 24;
  // Applied both to green samples in each scan line and to the number of
  // recovered line centroids used for the edge fit.
  int min_line_samples = 8;
};

struct SfrResult {
  bool accepted = false;
  std::string rejection_reason;
  std::string channel = "green-linear";
  std::string orientation;
  RoiRect roi;
  int green_sample_count = 0;
  double saturated_fraction = 0.0;
  double contrast_dn = 0.0;
  double edge_angle_deg = 0.0;
  double mtf50_cy_per_px = 0.0;
  double mtf50p_cy_per_px = 0.0;
  double mtf_at_nyquist = 0.0;
  double r1090_px = 0.0;
  double oversample = 4.0;
  std::vector<double> mtf_frequency_cy_per_px;
  std::vector<double> mtf;
};

std::vector<double> dft_magnitude(const std::vector<double>& signal);
double adjacent_difference_response(double frequency_cy_per_px,
                                    double sample_spacing_px);

SfrResult analyze_green_sfr(const CfaImageView& image, const RoiRect& requested,
                            const SfrOptions& options = {});

}  // namespace camera_iq
