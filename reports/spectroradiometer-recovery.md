# Recovering and characterizing an archived spectroradiometer set

[Study](../studies/spectroradiometer-recovery.md) ·
[method and formulas](../methods/spectral-group-analysis.md) ·
[group summary](../data/spectroradiometer-group-summary.csv) ·
[validation summary](../data/spectroradiometer-validation.csv)

## The experiment this would ideally have been

A controlled repeatability study fixes and records geometry, integration time,
and instrument configuration; monitors the source independently or measures a
stable reference channel; and interleaves repeat readings to expose warm-up and
time-order effects. Those controls make competing source, setup, and instrument
explanations testable instead of assuming that one caused the difference.

What survived is the measurement data itself: spectra, recorded XYZ, radiance,
and acquisition fields, in files whose names counted acquisitions rather than
describing scenes, several stored more than once under different names. The
setup records did not survive.

That gap is the whole reason this report stops where it does. The stored
differences can be quantified; the archive does not retain what is needed to
assign them a physical cause.

## Admitting the archive

Two operations, deliberately kept apart:

**Content identity.** Exact byte comparison separated 89 distinct retained
readings from 45 byte-identical aliases. An alias is the same measurement under
a second name; it is retained as provenance and analyzed once, because counting
it twice would inflate a group's apparent repeat count.

**Grouping.** Which distinct readings belong to the same target comes from the
retained grouping record. It is not inferred from spectral similarity — that
would assume the conclusion, since two readings of one stable source and two
readings of two similar sources look alike. The record yields 40 groups: 37 with
repeated readings, 3 singletons.

A ledger mismatch, an ambiguous source, a non-uniform grid, a shape mismatch, or
a non-finite derived value is a rejection rather than a partial group.

## Method

Each group is characterized on three axes, defined in the
[method companion](../methods/spectral-group-analysis.md): the equal-weight
spectral integral for level, the relative L2 residual between self-normalized
spectra for shape, and the largest pairwise Δu′v′ from recorded XYZ for
chromaticity.

Repeat statistics use the sample standard deviation with `n − 1`. Singletons
report absent variation fields rather than zeros: one measurement establishes a
level and a shape but not a spread, and a zero would read as perfect agreement.

## Results

Across the 37 repeated groups:

| Axis | Median | Maximum |
|---|---|---|
| Spectral-integral coefficient of variation | 7.167679309662159% | 41.647103399837285% |
| Maximum normalized-shape relative L2 residual | 0.5177845902558952% | 1.075914397243751% |
| Maximum pairwise Δu′v′ from recorded XYZ | 0.0007029769933166811 | 0.002851948638865613 |

`ramp_patch_05` carries the level maximum. `ramp_patch_01` carries both the
shape maximum and the chromaticity maximum. The level maximum therefore occurs
in a different group from the other two, and the three values do not describe
one measurement condition.

## Same-record XYZ closure

Integrating each retained spectrum against a public CIE observer with equal
sample weights reproduces the XYZ recorded in the same file under one
archive-derived proportional scale of `683.0167582353332`, with maximum absolute
relative residual `1.5543122344752192e-13%` and RMS relative residual
`5.004889505182855e-14%`.

This is closure between two fields of the same file. It establishes numerical
consistency between the spectral and colorimetric records under one fitted
scale. It is not an instrument-accuracy test, and the fitted scale is derived
from the archive — the analysis does not identify it as a standard
luminous-efficacy constant or infer undocumented instrument-software behavior
from its magnitude.

## Independent read

A MATLAB R2026a implementation read all 89 retained readings: 89 source-file
identity comparisons, 178 exact binary64 vector-hash comparisons — two per
reading — and 623 numeric-field comparisons at `1e-12`
absolute-or-relative tolerance. The largest absolute difference was
`4.547473508864641e-12 K`, on recorded CCT.

That establishes agreement between the two readers on the retained vectors and
numeric fields. It does not independently reproduce the grouping record or the
derived group statistics, and it is not a test of instrument accuracy.

## Limitations

The retained records do not establish whether the observed differences represent
physical change, acquisition variation, or measurement uncertainty. Source
output, geometry, acquisition settings, re-aiming, and instrument behavior are
not separable, so the result is labeled **within-group observed variation** —
not source drift, instrument noise, or repeatability.

Recorded CCT and Duv pass through as metadata. The source files do not identify
the locus and distance conventions needed to recompute them unambiguously, so
they are carried rather than recalculated.

Three singletons remain in the output with empty variation fields.

## What would resolve it

A repeat acquisition should record the conditions this one lost: documented
geometry, integration time, and instrument configuration; an independently
monitored source or stable reference channel; and interleaved repeats. Those
controls would make source, setup, and instrument explanations testable. Until
then, 41.6% is a quantified within-group difference, not an attributed drift or
noise mechanism.
