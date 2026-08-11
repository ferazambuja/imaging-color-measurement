# How far can an incomplete spectral archive be trusted?

[Report](../reports/spectral-measurement-crosscheck.md) ·
[Method and formulas](../methods/spectral-comparison.md) ·
[Comparison data](../data/hid-spectral-comparison.json) ·
[Reference audit](../data/spectral-reference-audit.json) ·
[Reference code](../code/src/spectral_compare.cpp)

Two spectral series can disagree for several reasons: the source may have
changed, the instruments may sample wavelength differently, or metadata may
send later colorimetry through the wrong observer model. The retained archive
contains enough information to locate these disagreements, but not enough to
assign their physical cause. That difference—localization without
attribution—is the point of this study.

Two retained series differ by **4.327%** directional relative L2, and the
disagreement is concentrated rather than uniform: two bands, at 530 and 540
nm, carry **75.9%** of the squared residual. Excluding them takes the comparison
to **2.276%**. That locates the problem without naming its cause.

## The ideal experiment and the surviving records

The strongest experiment would interleave both instruments on the same
monitored source, with geometry, settings, calibration state, wavelength
accuracy, and bandpass recorded. Those controls were not retained. What
survived were two eight-reading HID-lamp series on different wavelength grids,
four exports of one 24-patch reflectance measurement, and a separate candidate
pair of chart measurements with incomplete acquisition metadata.

Rather than treating that as either a controlled comparison or useless data, the
analysis asks narrower questions the records can answer:

- Is the cross-series difference larger than the variation within each series?
- Which wavelengths carry that difference?
- Which explicit observer reproduces the colorimetry embedded in a
  self-contradictory export?
- Are four files independent measurements or one measurement serialized four
  ways?

## What the spectra establish

After both HID series are resampled and normalized on the shared 380–730 nm,
10 nm grid, their directional relative-L2 difference is **4.327%**. The
largest within-series normalized-shape residuals are only **0.307%** and
**0.207%**, so the measured repeat spread does not explain the cross-series
difference.

The disagreement is localized. The 530 and 540 nm bands carry **75.9% of the
squared residual**; excluding those two diagnostic bands reduces the
comparison to **2.276%**. This identifies where a resolving experiment should
look. It does not identify which instrument—or the source—was responsible.

A fitted wavelength offset reduces the fixed-support objective from
**4.327416%** to **3.084143%** at **−0.95 nm**, a **28.7% reduction**. The
offset was selected and scored on the same spectra, so it demonstrates
sensitivity to relative wavelength registration, not a measured calibration
error.

![Repeated spectral series, residual localization, observer metadata comparison, and paired-chart variation](../figures/spectral-measurement-crosscheck.svg)

*The technical plots carry the numerical comparison. The highlighted 530 and 540 nm bars
localize the original disagreement; the three right-hand panels answer
separate metadata and interchange questions and are not combined into one
accuracy score.*

## What the metadata establishes

One reflectance export declares both a 2-degree and a 10-degree observer.
Recalculation under D65 gives **0.0119 mean ΔE76** against the embedded Lab
values with the CIE 1964 10-degree observer, compared with **3.909** under the
CIE 1931 2-degree alternative. A second application's embedded XYZ separately
agrees with its declared 2-degree calculation to **0.0469% mean relative L2**.
Observer choice therefore has to be tested against each declared output rather
than inferred from a filename or one conflicting field.

All four exports contain the same 24 spectra by stable sample identity. They
demonstrate interchange, not four independent measurements. The candidate
chart pair shows **0.851 mean** and **1.952 maximum ΔE76** under its declared
D55/2-degree calculation, but the missing acquisition conditions limit that to
observed paired-series variation—not instrument repeatability or accuracy.

## What would resolve the open question

The retained measurements show where the HID disagreement is concentrated and
which metadata interpretation is consistent. It cannot separate source change,
wavelength registration, spectral bandwidth, geometry, or calibration state.
A new interleaved acquisition on a monitored source, with a characterized
higher-resolution spectral reference and recorded geometry and settings, would
be needed to assign a cause.
