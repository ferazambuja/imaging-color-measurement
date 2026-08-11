#pragma once

#include <vector>

namespace camera_iq {

struct Cam16BrightnessAuditPoint {
  double j = 0;
  double cam16_q_over_q_white = 0;
  double hellwig_2022_q_over_q_white = 0;
};

struct Cam16BackgroundAuditPoint {
  double y_background = 0;
  double isolated_ncb_chroma_factor = 0;
};

struct Cam16CoupledChromaAuditPoint {
  double y_background = 0;
  double reference_j = 0;
  double relative_chroma = 0;
};

struct Hellwig2022PublishedPerformance {
  double brightness_cam16_r_squared = 0;
  double brightness_proposed_r_squared = 0;
  double chroma_cam16_r_squared = 0;
  double chroma_proposed_r_squared = 0;
  double colorfulness_cam16_r_squared = 0;
  double colorfulness_proposed_r_squared = 0;
};

struct Cam16EquationAuditReport {
  std::vector<Cam16BrightnessAuditPoint> brightness_curve;
  std::vector<Cam16BackgroundAuditPoint> background_curve;
  std::vector<Cam16CoupledChromaAuditPoint> coupled_chroma_curve;
  Hellwig2022PublishedPerformance performance;
};

// Normalized within one fixed viewing-condition contract: Q/Q_white.
double cam16_normalized_brightness(double j);
double hellwig_2022_normalized_brightness(double j);

// Isolates only N_cb^0.9 relative to Y_b=20 with all other CAM16 terms held
// fixed. It is deliberately not named or reported as full CAM16 chroma.
double cam16_isolated_ncb_chroma_factor(double y_background,
                                        double reference_y_background = 20.0);

// Reproduces the complete CAM16 chroma expression's background dependence
// while adapted responses are held fixed, isolating the terms varied in the
// paper's Figure 3. reference_j is the stimulus lightness at
// reference_y_background. This is not a general CAM16 forward transform.
double cam16_relative_chroma_fixed_adapted_response(
    double y_background, double reference_j,
    double reference_y_background = 20.0);

// Corrected 22 April 2022 form of Hellwig & Fairchild Equation 23.
double hellwig_2022_colorfulness(double n_c, double eccentricity,
                                 double opponent_a, double opponent_b);

Cam16EquationAuditReport build_cam16_equation_audit();

}  // namespace camera_iq
