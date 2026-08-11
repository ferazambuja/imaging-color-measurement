#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include "camera_iq/sampled_spectrum.hpp"

namespace camera_iq {

enum class SpectralOffsetSeries {
  Reference,
  Candidate,
};

struct SpectralComparisonOptions {
  std::vector<double> common_wavelength_nm;
  std::vector<double> excluded_wavelength_nm;
  double offset_min_nm = 0.0;
  double offset_max_nm = 0.0;
  double offset_step_nm = 0.0;
  SpectralOffsetSeries offset_series = SpectralOffsetSeries::Candidate;
};

struct SpectralComparisonBand {
  double wavelength_nm = 0.0;
  double signed_residual = 0.0;
  double squared_residual_fraction = 0.0;
};

struct SpectralExclusionResult {
  std::vector<double> excluded_wavelength_nm;
  std::size_t retained_sample_count = 0;
  double directional_relative_l2 = 0.0;
};

struct SpectralL2Objective {
  double residual_l2_norm = 0.0;
  double reference_l2_norm = 0.0;
  double directional_relative_l2 = 0.0;
};

struct SpectralOffsetResult {
  double wavelength_offset_nm = 0.0;
  SpectralL2Objective objective;
};

struct SpectralComparison {
  std::string normalization = "common_grid_equal_weight_integral";
  std::string interpolation = "linear";
  std::string relative_l2_denominator = "reference_l2_norm";
  std::string offset_convention =
      "candidate_nominal_wavelength_plus_offset_is_actual_wavelength";
  std::string offset_objective_scope =
      "per_offset_equal_weight_integral_normalization_on_fixed_common_grid";
  SampledSpectrumGroupAnalysis reference_group;
  SampledSpectrumGroupAnalysis candidate_group;
  std::vector<double> common_wavelength_nm;
  std::vector<double> reference_on_common_grid;
  std::vector<double> candidate_on_common_grid;
  double directional_relative_l2 = 0.0;
  std::vector<SpectralComparisonBand> bands;
  std::vector<SpectralExclusionResult> exclusion_results;
  std::size_t offset_common_grid_sample_count = 0;
  std::optional<SpectralL2Objective> zero_offset_objective;
  std::vector<SpectralOffsetResult> offset_sensitivity;
  double best_wavelength_offset_nm = 0.0;
  double best_offset_directional_relative_l2 = 0.0;
  std::vector<SpectralComparisonBand> best_offset_bands;
};

SpectralComparison compare_spectral_groups(
    const std::vector<SampledSpectrum>& reference,
    const std::vector<SampledSpectrum>& candidate,
    const SpectralComparisonOptions& options);

}  // namespace camera_iq
