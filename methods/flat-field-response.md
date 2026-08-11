# CFA flat-field response method and implementation

[Methods](README.md) · [study](../studies/cfa-flat-field-response.md) ·
[scientific report](../reports/flat-field-response.md) ·
[public header](../code/include/camera_iq/shading.hpp) ·
[implementation](../code/src/shading.cpp) ·
[tests](../code/tests/test_flat_field.cpp)

## Scope

The published C++20 module measures a flat field after RAW decoding and black
subtraction. Its input is a pointer to a signed CFA mosaic plus image geometry,
declared options, and one signal-referred ceiling for each of the four mosaic
positions.

RAW decoding, archive discovery, metadata parsing, private sample labels, and
report serialization are deliberately outside this repository. The numerical
estimator here is the same implementation used by the archive adapter; it is
not a simplified display reimplementation.

## Data flow

```text
black-subtracted CFA mosaic + four signal ceilings + options
  -> validate policy and even CFA geometry
  -> measure full-frame and centered-gate headroom per CFA position
  -> measure center and four equal-radius corner blocks
  -> take per-position medians over a spatial grid
  -> normalize each CFA plane by its own center median
  -> derive green, R/G, B/G, and G1/G2 response maps
  -> compute four-corner green asymmetry
  -> optionally compare two fields or measure a dark pedestal
```

The separation between the archive adapter and this core is visible in the API:
the core does not accept a filename, dataset identifier, camera clock, or RAW
metadata object.

## Signal headroom

The input mosaic is already black-subtracted, so raw white code is the wrong
denominator. For CFA position `p`:

```text
Smax[p] = white[p] - black[p]
T[p] = near_ceiling_level × Smax[p]
```

For region `r`, the near-ceiling fraction and finite coverage are:

```text
Nnear[p,r] / Nfinite[p,r]
Nfinite[p,r] / Nexpected[p,r]
```

The first ratio is undefined when there are no finite samples. It is never
replaced with zero. Headroom and coverage are separate conditions because a
low near-ceiling fraction measured over one surviving sample does not describe
a usable field.

`measure_cfa_near_ceiling()` accepts a screening rectangle only when it equals
its clipped, CFA-balanced form. That one equality proves both containment and
complete 2 × 2 CFA geometry; the former separate containment branch was
unreachable and is not part of the published design.

## Geometry

Three spatial regions have different jobs:

- the **center gate** screens local headroom;
- the **center block** supplies the normalization denominator; and
- the **four corner blocks** supply the equal-radius asymmetry statistic.

`make_shading_geometry()` resolves the exact rectangles once. Odd block sizes
and insets are rounded inward to even values, but an odd active image is
rejected: exact mirrored corners are unavailable on that geometry. A clipped
block, a center outside the gate, or overlap between the center and a corner is
also rejected rather than silently becoming a different experiment.

## Binned spatial response

Each CFA position is sampled independently. Within each grid cell, the module
uses the upper median so an isolated defective pixel cannot move an entire
cell. For CFA plane `p` and bin `i`:

```text
M[p,i] = median of finite samples in bin i
R[p,i] = M[p,i] / median[p, center block]
```

The center block normalizes to one as a region. Individual center samples and
grid cells that overlap it are not forced to one.

A map is accepted only when every plane has a positive finite center median,
enough finite samples in every bin, acceptable negative-sample fraction, and
valid aggregate values. Earlier gate failures preserve the diagnostics already
measured but do not emit plausible response vectors.

## Chromatic response

The CFA layout is supplied explicitly. For each bin:

```text
G(i)       = [R[G1,i] + R[G2,i]] / 2
C_RG(i)    = R[R,i]  / G(i)
C_BG(i)    = R[B,i]  / G(i)
C_G1G2(i)  = R[G1,i] / R[G2,i]
```

These are ratios of independently center-normalized spatial fields, not raw
R/G and B/G values. A constant G1/G2 gain difference cancels; a spatially
varying mismatch does not.

Completeness applies to every accepted chromatic document. If any required
denominator is zero or non-finite, the map remains a valid CFA-layout result
but is marked incomplete, reports the number of missing bins, and stores NaN
for each undefined ratio. It does not emit zero or infinity.

## Equal-radius asymmetry

For corner `q`, green response is the average of the two center-normalized
green planes:

```text
G(q) = [R_G1(q) + R_G2(q)] / 2
A = [max_q G(q) - min_q G(q)] / mean_q G(q)
```

The four blocks are separate measurements. Keeping them separate retains the
direction of a gradient; taking a radial average first would erase the pattern
being tested.

For the synthetic centered-quadratic fixture in the public test, sampled
asymmetry must remain below `1e-3` under that fixture's CFA phase, block
geometry, and median estimator. This is a fixture-specific discretization
bound, not a universal guarantee. The asymmetric fixture must exceed the
declared `0.05` policy and retain the expected left/right direction.

## Pair comparison and pedestal

Two accepted fields can be compared only when their grid and effective
geometry match. For corner `q` and plane `p`:

```text
d(q,p) = 100 × |R1(q,p) - R2(q,p)| percentage points
maximum = max d(q,p)
RMS = sqrt[mean d(q,p)^2]
```

Multiplicative exposure scale cancels under separate center normalization. An
additive pedestal does not, which is why the public test requires a synthetic
offset to remain measurable in the comparison.

`measure_pedestal()` reports the median black-subtracted residual and finite
coverage for each CFA position. Camera/exposure identity and archive pairing
belong to the private adapter, not to this numerical function. In the reports,
the pedestal check is required for detailed/response outputs; screening
inventories may stop after admission because they do not publish a response
map.

## Invalid inputs

The core rejects:

- null or undersized buffers and invalid row strides;
- non-finite or non-positive signal ceilings;
- invalid policy ranges or impractically large grids;
- odd active-image dimensions or regions that cannot preserve CFA balance;
- inadequate screening or map-bin coverage;
- near-ceiling, low-signal, or excessive-negative-sample conditions;
- a non-positive center denominator;
- invalid CFA color layout; and
- pair comparisons whose effective geometries differ.

An undefined field is not converted into a zero map, and a rejected field
cannot become a chromatic result simply because partially populated vectors
happen to exist.

## Tests

The synthetic public test separates several properties that the archive cannot
test independently:

- exact 1% headroom and 90% coverage boundaries, with rejection immediately
  beyond their declared operating conditions;
- odd-image, odd-origin, and clipped-gate geometry rejections;
- exact recovery of asymmetric 4 × 4 spatial and chromatic maps;
- explicit incompleteness for one zero chromatic denominator;
- a centered radial field below the fixture-specific `1e-3` asymmetry bound;
- a directional field above the `0.05` policy;
- cancellation of multiplicative exposure scale and survival of an additive
  offset; and
- finite per-position pedestal measurement.

These are library-level numerical tests. They establish the estimator's
calculation and invalid-input handling, not archive ingestion, camera metadata
identity, sphere uniformity, or the physical cause of the measured field. The
public aggregate tables supply the reviewed archive result; the private source
captures are not required to run these unit tests.

## Published source

- shared near-ceiling policy: [`near_ceiling.hpp`](../code/include/camera_iq/near_ceiling.hpp)
- CFA headroom gate: [`flat_field_gate.hpp`](../code/include/camera_iq/flat_field_gate.hpp),
  [`flat_field_gate.cpp`](../code/src/flat_field_gate.cpp)
- ROI geometry: [`roi.hpp`](../code/include/camera_iq/roi.hpp),
  [`roi.cpp`](../code/src/roi.cpp)
- response types and formulas: [`shading.hpp`](../code/include/camera_iq/shading.hpp)
- numerical implementation: [`shading.cpp`](../code/src/shading.cpp)
- synthetic tests: [`test_flat_field.cpp`](../code/tests/test_flat_field.cpp)
