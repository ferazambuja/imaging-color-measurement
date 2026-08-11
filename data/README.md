# Public data

These files are the public aggregate inputs and outputs used by the portfolio
figures and reports. They do not contain original RAW captures, source
filesystem paths, or instrument serials.

## Camera measurements

- [`sfr-aperture-summary.csv`](sfr-aperture-summary.csv)
- [`flat-field-response.csv`](flat-field-response.csv)
- [`flat-field-screening.csv`](flat-field-screening.csv)
- [`ccm-patch-rgb.csv`](ccm-patch-rgb.csv)
- [`ccm-validation-summary.csv`](ccm-validation-summary.csv)
- [`patch-rgb-crosscheck.csv`](patch-rgb-crosscheck.csv)
- [`patch-grid-validation.csv`](patch-grid-validation.csv)

The flat-field tables use neutral public sample identifiers. All non-label
fields are preserved from the reviewed aggregates. The CCM patch table adds
only the explicit `R_DN,G_DN,B_DN` header to the retained 140 numeric rows.
The two patch-validation tables preserve the channel-agreement and geometry
decisions cited by the extraction report without publishing capture names or
coordinates.

The compatible reflectance workbook, measured illuminant spectrum, and derived
140-patch target XYZ are not redistributed. The public patch RGB and validation
summary therefore support inspection of the inputs and results, but not an
independent end-to-end reproduction of the archive CCM fit.

## Spectral and instrument measurements

- [`spectral-color-fidelity.csv`](spectral-color-fidelity.csv)
- [`spectral-fidelity-controls.csv`](spectral-fidelity-controls.csv)
- [`spectroradiometer-group-summary.csv`](spectroradiometer-group-summary.csv)
- [`hid-spectral-comparison.csv`](hid-spectral-comparison.csv)
- [`hid-spectral-comparison.json`](hid-spectral-comparison.json)
- [`spectral-reference-audit.json`](spectral-reference-audit.json)
- [`spectral-reference-repeat.csv`](spectral-reference-repeat.csv)
- [`spectroradiometer-validation.csv`](spectroradiometer-validation.csv)

In `spectral-color-fidelity.csv`, `smi_cc18` and
`mean_ciede2000_cc18` describe the same declared CIE D55 calculation over the
18 chromatic ColorChecker patches, but they remain different metrics and are
not compared numerically. The closure columns describe a separate physical
cross-check: one global exposure scale was fitted for each of four camera
datasets, then relative RMS and the minimum channel correlation were evaluated
over 140 matched patches. The Phase One row has no paired closure capture, so
those cells are intentionally empty rather than inferred.

`spectral-fidelity-controls.csv` records the checks used for the mixed-source
comparison: the retained same-camera curve agreement, the
four-camera Luther residuals computed from both toolkit and legacy sensitivity
curves, and the two retained Phase One runs. Blank cells mean that a metric is
not part of that control, not that it was measured as zero. These sanitized
aggregates were calculated from retained original measurements that are not
part of this repository. They document the controls used for the published
comparisons; original capture and instrument-file ingestion is not included.

The calculations and limits are explained in the
[spectral-sensitivity study](../studies/spectral-sensitivity-and-color-fidelity.md).
The HID, reference-audit, and paired-chart tables are explained in the
[spectral measurement cross-check](../studies/spectral-measurement-crosscheck.md).

## Deterministic color studies

- [`gamut-synthetic-input.csv`](gamut-synthetic-input.csv)
- [`gamut-cielab-radial.csv`](gamut-cielab-radial.csv)
- [`gamut-oklch-radial.csv`](gamut-oklch-radial.csv)
- [`gamut-css-local-minde.csv`](gamut-css-local-minde.csv)
- [`gamut-soft-compression.csv`](gamut-soft-compression.csv)
- [`cam16-equation-audit.csv`](cam16-equation-audit.csv)
- [`cie94-historical-24patch.csv`](cie94-historical-24patch.csv)

The gamut and CAM16 tables are deterministic inputs or outputs, not camera,
display, printer, or observer measurements. The historical CIE94 table retains
rounded Lab pairs from an earlier color-management course exercise; it is used
to test plausible conventions, not to reconstruct missing third-party settings
as known facts.
