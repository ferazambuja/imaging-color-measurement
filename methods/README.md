# Methods and formulas

These companions carry the numerical detail: accepted inputs, formulas, the
sequence a calculation actually follows, what each reported quantity means, and
what the implementation rejects rather than guessing.

Each links to the published reference code that implements it. The code is the
implementation used to produce the results, not a restatement written for
display.

- [Slanted-edge SFR](slanted-edge-sfr.md) — CFA input, edge fitting,
  oversampled ESF binning, the two-sided support rule, the LSF/window/DFT
  sequence, adjacent-difference correction, and invalid-input handling.
- [CFA flat-field response](flat-field-response.md) — signal-referred headroom,
  CFA-balanced geometry, median spatial maps, chromatic ratios, equal-radius
  asymmetry, pair comparison, and invalid-input handling.
- [Color-correction matrix](color-correction-matrix.md) — normal equations
  for the 3x3 fit, CIELAB conversion, CIEDE2000, deterministic folds, lightness
  selection, and invalid-input handling.
- [Spectral fidelity](spectral-fidelity.md) — white-gated physical closure,
  the normalized Luther-condition subspace residual, ISO 17321-style SMI,
  operating conditions, and invalid-input handling.
- [Spectral group analysis](spectral-group-analysis.md) — level, normalized
  shape, and chromaticity as separate axes; the n-1 singleton contract; and the
  single-scale XYZ closure.
- [Gamut mapping](gamut-mapping.md) — RGB/XYZ transforms, analytic CIELAB and
  OkLCh first-exit boundaries, four mapping intents, typed diagnostics, and
  invalid-input handling.
- [CAM16 equation audit](cam16-equation-audit.md) — normalized brightness,
  isolated and coupled background factors, the corrected colorfulness
  relation, operating domains, and invalid-input handling.
- [Spectral comparison](spectral-comparison.md) — native and common-grid
  normalization, directional relative L2, per-band localization, diagnostic
  exclusions, offset sensitivity, and invalid-input handling.
