# Scientific reports

Each report gives the full account behind a study: what the ideal controlled
experiment would have retained, what the archive actually kept, how the
measurement was designed around the difference, the detailed results, and the
acquisition that would resolve what remains open.

The [studies](../studies/README.md) answer the question. The reports show the
work, including the parts that did not resolve.

- [Slanted-edge SFR across aperture and field](sfr-mtf.md) — two capture systems
  sharing a 50 mm lens model; center sweep, field margins, advisory cross-check,
  and the three unranked explanations for the divergence.
- [CFA flat-field response](flat-field-response.md) — headroom screening,
  center-normalized CFA maps, equal-radius corner asymmetry, bounded dark
  control, and the source/camera attribution limit.
- [Linear RGB-to-XYZ color correction](ccm-fit.md) — five-fold held-out
  evaluation, the dark-patch exclusion experiment, and the reference limit.
- [Getting linear camera RGB out of a chart capture](patch-extraction.md) —
  black subtraction, flat correction, white balance, and why RGB agreement and
  geometric agreement are separate checks.
- [Where the target XYZ came from](reference-provenance.md) — compatible
  spectral reference, measured illuminant, and what per-unit identity would add.
- [Spectral sensitivity and camera color fidelity](spectral-sensitivity.md) —
  five sensitivity sets, three distinct fidelity questions, four physical
  closure paths, and the unresolved cross-rig endpoint.
- [Recovering and characterizing an archived spectroradiometer set](spectroradiometer-recovery.md)
  — content identity versus record-based grouping, three variation axes,
  same-record XYZ closure, and an independent read.
- [Comparing four Display-P3 to sRGB gamut-mapping methods](gamut-mapping.md)
  — controlled coordinate and algorithm comparisons, analytic first-exit
  boundaries, the P3-yellow counterexample, and the device/observer boundary.
- [CAM16 equation audit](cam16-equation-audit.md) — normalized brightness,
  isolated and coupled background behavior, the corrected colorfulness
  coefficient, a linked standalone two-model comparator, and the limit imposed
  by the absence of observer data.
- [Spectral measurement and reference-data cross-check](spectral-measurement-crosscheck.md)
  — repeated spectra on unlike grids, residual localization, contradictory
  observer metadata, and the acquisition-control limit.
