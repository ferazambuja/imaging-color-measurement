# Spectral group analysis: formulas and implementation

[Study](../studies/spectroradiometer-recovery.md) ·
[report](../reports/spectroradiometer-recovery.md) ·
[implementation](../code/src/spectro_analysis.cpp) ·
[closure](../code/src/spectro_colorimetry.cpp) ·
[sampled-spectrum tests](../code/tests/test_sampled_spectrum.cpp) ·
[measurement tests](../code/tests/test_spectro_measurement.cpp) ·
[analysis tests](../code/tests/test_spectro_analysis.cpp) ·
[closure tests](../code/tests/test_spectro_colorimetry.cpp)

Four modules are published here, and the split between them is the design:

| Module | Responsibility |
|---|---|
| [`sampled_spectrum`](../code/include/camera_iq/sampled_spectrum.hpp) | Unit-neutral level/shape separation, shared with other spectral work |
| [`spectro_measurement`](../code/include/camera_iq/spectro_measurement.hpp) | The typed reading and its group statistics |
| [`spectro_analysis`](../code/include/camera_iq/spectro_analysis.hpp) | Level, shape, and chromaticity as three separate outputs |
| [`spectro_colorimetry`](../code/include/camera_iq/spectro_colorimetry.hpp) | Same-record XYZ closure |

What is **not** here is equally deliberate: the MATLAB reader, the archive
ingest, the identity ledger, and the report writers. The published code begins
with typed in-memory measurements. How the archive was admitted is explained in
the [report](../reports/spectroradiometer-recovery.md); the measurement type
does not depend on the format the measurement arrived in.

## Level

For reading `r` on a uniform grid with step `Δλ`:

```text
level_r = Δλ · Σᵢ radiance_r[i]
```

Every retained sample, including both endpoints, receives the same weight. This
is the rectangular sum used by the archive, not trapezoidal integration.

The implementation uses compensated summation with exponent scaling, so a
finite, representable answer survives a wide numeric range. A non-positive
integral is rejected, because it cannot normalize a spectrum.

Across a group, level variation is the sample standard deviation with `n − 1`,
divided by the mean:

```text
CV = s / mean
```

The `n − 1` divisor is why a singleton has no value rather than zero: with one
reading the denominator is zero, and reporting `0` would describe perfect
agreement from a single measurement.

## Shape

Each spectrum is divided by its own integral before comparison, which removes
level completely:

```text
normalized_r[i]   = radiance_r[i] / level_r
shape_residual_r  = ‖normalized_r − mean_normalized‖₂ / ‖mean_normalized‖₂
```

The group reports the maximum residual over its readings. Mathematically, a
pure scale change leaves this at zero. The test uses a dyadic fixture for which
the finite-precision result is also exactly zero, then uses a fixed-integral
redistribution to prove the implementation does not simply return zero. Level
and shape are therefore separately responsive quantities rather than two names
for the same calculation.

## Chromaticity

From the recorded XYZ, not from a recomputation:

```text
x  = X / (X + Y + Z)
y  = Y / (X + Y + Z)
u′ = 4X / (X + 15Y + 3Z)
v′ = 9Y / (X + 15Y + 3Z)
```

The group reports the largest pairwise distance in `u′v′`, chosen because that
space is closer to perceptually uniform than `xy`, so a given numeric separation
means something more consistent across the diagram.

Recorded CCT and Duv are carried as metadata and never recomputed: the files do
not identify the locus and distance conventions that would make a recomputation
unambiguous.

## Same-record XYZ closure

Integrating the spectrum against a CIE observer with the same equal sample
weights should reproduce the recorded XYZ up to one proportional constant:

```text
XYZ_computed[c] = Δλ · Σᵢ radiance[i] · x̄_c[i]
```

One scale `k` is fitted across every reading and every channel simultaneously —
not per reading, and not per channel, because a per-channel scale would absorb
exactly the disagreement the check exists to detect. Signed relative residuals
are then reported.

The result labels its own scale `derived_from_recorded_xyz`. That label is load
bearing: the fitted value is close to a familiar constant, and the analysis
does not label it as one.

## Invalid inputs

| Condition | Behavior |
|---|---|
| Empty group or closure input | rejected |
| Grid shorter than two samples, non-uniform grid, or mismatched repeat axes | rejected |
| Radiance length mismatch or non-finite spectrum/XYZ sample | rejected |
| Spectrum with non-positive integral or non-representable normalization | rejected |
| XYZ that cannot form chromaticity | rejected |
| Observer/measurement axis mismatch or malformed observer table | rejected |
| Non-positive fitted closure scale or undefined relative residual | rejected |
| Non-representable mean, spread, integral, fit, or residual | rejected |

A rejection is a whole-group failure, not a partial result with some fields
filled in.

## What the tests pin

Four module-focused executables build every fixture in their own source files.
They do not read the private archive and do not test command-level or
archive-level behavior.

The central case is the separation property. A group holding one spectrum and
its exact double must show a level coefficient of variation of `√32 / 12` — the
`n − 1` value for `{8, 16}` — with shape residual and chromaticity separation
both exactly zero.

That case alone would pass on an implementation that always reports zero shape,
so a second case redistributes the spectrum while holding its integral fixed:
level variation must then be zero and shape residual must move. A deliberate
XYZ change separately requires nonzero chromaticity movement.

[`test_sampled_spectrum.cpp`](../code/tests/test_sampled_spectrum.cpp) covers
the rectangular integral, level/shape separation, a negative sample with a
positive total, subnormal preservation, cancellation, and its invalid-input cases.
[`test_spectro_measurement.cpp`](../code/tests/test_spectro_measurement.cpp)
covers pointwise `n−1` spread, high-offset accuracy, extreme finite range,
cancellation, permutation, and unrepresentable spread. The analysis test covers
the three-axis and singleton contracts plus high-range and subnormal cases.
[`test_spectro_colorimetry.cpp`](../code/tests/test_spectro_colorimetry.cpp)
recovers one global scale, proves that a channel-specific disagreement leaves a
residual, exercises high-range and cancellation paths, and checks the observer
table and closure rejections.

One note on the closure fixture, because it caught a mistake while being
written: the recorded XYZ must include the grid's sample width, since the
integral does. Omitting it makes the recovered scale differ from the intended
one by exactly the step — a fixture error that looks like an implementation bug.
