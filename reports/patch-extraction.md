# Getting linear camera RGB out of a chart capture

[Study](../studies/colorchecker-ccm.md) ·
[CCM report](ccm-fit.md) ·
[reference provenance](reference-provenance.md) ·
[patch RGB](../data/ccm-patch-rgb.csv)

Before a color matrix can be fitted, each chart patch needs one linear RGB
triple that represents the patch and nothing else. Every step between the sensor
and that triple can distort it, so each is stated here rather than assumed.

## From mosaic to patch value

**Black subtraction.** The sensor's per-channel pedestal is removed once, so
every later value is signal above black rather than raw code. Subtracting a
pedestal twice is silent and produces a plausible wrong answer, which is why the
operation happens at a single declared point.

**Demosaic.** A bilinear reconstruction gives a full RGB image. Sampling
rectangles stay inside the patch interiors to reduce border and interpolation
effects. This archive does not contain an alternate-demosaic experiment, so the
report does not quantify how much the demosaic choice changes the patch means.

**Flat-field correction.** A measured flat frame, captured at the same aperture,
divides out the field response across the chart area. That frame is screened
before use: a flat whose own bright region sits near the sensor ceiling would
carry a flattened, understated field and quietly distort every patch it
corrects. The selected frame measures 0% near ceiling in both the whole frame
and the centered region.

The flat cannot separate the illumination source from the lens and the sensor,
so this is same-aperture correction, not shading calibration. Its own
characterization is a separate study; the figures used here are published in
[`flat-field-screening.csv`](../data/flat-field-screening.csv) and
[`flat-field-response.csv`](../data/flat-field-response.csv).

**White balance.** Channel gains set the neutral axis so the neutral patches sit
on it. This is a declared step in the pipeline, not a fit parameter.

**Sampling.** Each patch is sampled as a rectangle inside the patch interior,
away from borders and printing artifacts. The resulting triples are published in
[`ccm-patch-rgb.csv`](../data/ccm-patch-rgb.csv) — 140 rows of linear
`R_DN, G_DN, B_DN`, which is the exact input the matrix is fitted on.

## Two agreements that are not the same agreement

Patch values were cross-checked against an independent reference tool's averages
for the same capture. Channel correlations exceeded **0.99999998**, with direct
RMSE of **0.352 / 0.041 / 0.381 DN** for R/G/B. Direct and flipped chart
orientations separated clearly, so the patch ordering is not ambiguous.
The channel-level values are published in
[`patch-rgb-crosscheck.csv`](../data/patch-rgb-crosscheck.csv).

A second, cheaper localization route was tested and rejected: generating the
patch grid from four detected chart corners rather than using manually checked
rectangles. Its RGB correlations were still above **0.999**, which looks like
success — but its generated centers missed the checked positions by up to
**16.449 px** against a declared 5 px limit.
The geometry decision and its independent RGB gate are recorded in
[`patch-grid-validation.csv`](../data/patch-grid-validation.csv).

Those two results say different things, and the distinction is the point.
High RGB correlation means the sampled values track each other. It does not mean
the sampler is looking at the right place: on a chart of large flat patches, a
center can drift well inside a neighboring region and still correlate. Geometry
has to be checked as geometry. The corner-seeded grid was therefore not used for
the reported result, and the manually checked rectangles remain the coordinate
source.

## Limitations

Patch sampling inherits every limitation of the capture: one aperture, one
illuminant, one session. Interior rectangles avoid border effects but do not
measure patch uniformity. The reference-tool comparison establishes agreement
between two samplers of the same frame; it is not an independent measurement of
the chart.
