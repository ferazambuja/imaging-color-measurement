# Where the target XYZ came from

[Study](../studies/colorchecker-ccm.md) ·
[CCM report](ccm-fit.md) ·
[patch extraction](patch-extraction.md)

A color-correction matrix is fitted against target values that state what each
patch *should* produce. Those targets are not measured by the camera; they come
from the chart's spectral reflectances rendered under the capture illuminant.
The reference therefore limits how the fitted matrix can be interpreted, which
makes its provenance part of the result rather than a footnote.

## What was used

The target XYZ came from a spectral reference associated with the same
ColorChecker SG product as the photographed chart, rendered under an explicitly
measured sphere spectrum. The illuminant is taken from the measurement
sidecars retained with the session — not inferred from capture metadata or
from the date.

That gives a physically specified 140-patch target: reflectance times
illuminant, integrated against the CIE observer.

## What was not established

**Identity with the photographed chart unit was not established.** The reference
describes the chart *product*; no per-unit spectral measurement of the specific
physical chart that was photographed was retained. Charts of the same product
vary between units and change with age and handling.

The reference was verified against manufacturer nominal values across all 140
patches and differs from them by **mean ΔE76 1.34**.

That figure needs care. It is a ΔE76 measurement, under a different weighting
convention from the CIEDE2000 used for the camera result, so it cannot be
subtracted from that result or treated as an error budget for it. What it
establishes is narrower and still useful: two plausible descriptions of the
chart product disagree, so the reported camera result cannot be assigned to the
camera alone. These data do not establish whether or how much of the reported
difference came from reference mismatch.

## What the comparison supports

With a compatible reference, the study can measure how well a linear model maps
this capture's RGB to a declared target, and whether its training score remains
representative across patch partitions. Both are properties of the complete
measured and modeled path, not of the camera in isolation.

What it cannot support is a per-unit chart calibration, or the conclusion that the
residual is attributable to the camera alone.

## What a per-unit measurement would add

Measuring the photographed chart directly on a well-characterized spectrophotometer,
with calibration and measurement uncertainty retained, would replace the
product-level substitution with a per-unit reference. It would make reference
mismatch estimable rather than silently coupled to the residual. Remaining
difference would still include model, capture, and measurement contributions.
