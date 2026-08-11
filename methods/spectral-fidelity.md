# Spectral-fidelity calculations

This companion explains the numerical core used by the spectral-sensitivity
study. The published code starts after archive ingest: wavelength axes have
already been aligned, sensitivity curves selected, chart reflectances paired,
and measured RGB prepared. It does not read RAW files, discover sessions, or
decide which historical records belong together.

[Study](../studies/spectral-sensitivity-and-color-fidelity.md) ·
[Report](../reports/spectral-sensitivity.md) ·
[Aggregate data](../data/spectral-color-fidelity.csv) ·
[Validation controls](../data/spectral-fidelity-controls.csv)

## Data flow

```text
aligned SSF + illuminant + reflectance + paired RGB
    └─ spectral_closure → white-card gate → one global scale → RMS/correlation

aligned SSF + CIE color-matching functions
    └─ spectral_quality → normalized subspace fit → Luther residual → QI

aligned SSF + illuminant + reflectance + CIE functions
    └─ spectral_smi → synthetic RGB/XYZ → fitted 3×3 → ΔE76/CIEDE2000 → SMI
```

These branches share spectral inputs but do not measure the same property.

## Physical closure

Implementation: [`spectral_closure.hpp`](../code/include/camera_iq/spectral_closure.hpp)
and [`spectral_closure.cpp`](../code/src/spectral_closure.cpp).

### Inputs

The module requires finite vectors on one strictly increasing, uniformly spaced
wavelength grid. Every sensitivity channel and illuminant has one value per
wavelength; every patch has one reflectance vector and one measured RGB triplet.
At least one patch and two wavelength samples are required.

Uniform spacing is an operating condition, not a cosmetic preference. The
implementation uses equal sample weights. The common step width would multiply
every prediction equally and is absorbed by the fitted exposure scale, but a
nonuniform grid would require explicit integration weights and is rejected.

### White-card pairing gate

For a flat perfect diffuser, the predicted channel response is

```text
Wc = Σλ Sc(λ) E(λ)
```

Measured and predicted `R/G` and `B/G` ratios are compared. Their largest
relative disagreement must not exceed the declared gate. Closure stops after a
failure because fitting the chart under an unconfirmed illuminant pairing would
produce a precise residual for the wrong experiment.

### One global exposure scale

For each patch and channel,

```text
Pp,c = Σλ Sc(λ) E(λ) Rp(λ)
```

The exposure scale minimizes squared error over the complete patch-by-channel
set:

```text
k = Σp,c Mp,c Pp,c / Σp,c Pp,c²
```

The implementation rejects a non-representable or zero-energy denominator.
It then reports, per channel,

```text
relative RMSc = sqrt(meanp((Mp,c − kPp,c)²)) / |meanp(Mp,c)|
```

and Pearson correlation across patches. The mean measured response must be
nonzero because the relative RMS would otherwise be undefined.

A per-channel scale is calculated for diagnosis only. The reported predictions
always use the one global `k`; three fitted scales would remove channel balance
from the residual.

## Luther-condition quality

Implementation: [`spectral_quality.hpp`](../code/include/camera_iq/spectral_quality.hpp)
and [`spectral_quality.cpp`](../code/src/spectral_quality.cpp).

Let `A` contain the sampled red, green, and blue sensitivities as columns, and
let `q` be one sampled CIE color-matching function. Each input column and target
is first divided by its L2 norm. The least-squares system is

```text
G = AᵀA
β = G⁻¹Aᵀq
r(q) = ||q − Aβ||₂ / ||q||₂
```

The code solves the 3×3 Gram system with Cramer’s rule and rejects a basis whose
determinant is too small relative to the product of its diagonal terms. At
least four wavelengths are required so the three-parameter fit is
overdetermined.

The combined residual and quality index are

```text
rcombined = sqrt((rx² + ry² + rz²) / 3)
QI = 1 − rcombined
```

Because the sensitivity columns are normalized, `rcombined` is invariant to
nonzero positive scaling of one or all channels. The invariant object is the
normalized subspace metric—not the camera’s physical amplitude response, SMI,
or closure residual.

## ISO 17321-style SMI

Implementation: [`spectral_smi.hpp`](../code/include/camera_iq/spectral_smi.hpp)
and [`spectral_smi.cpp`](../code/src/spectral_smi.cpp). It reuses the published
[`colorimetry`](color-correction-matrix.md) implementation for the 3×3 fit,
CIELAB conversion, and color differences.

### Spectral synthesis

Unlike closure, this branch accepts a nonuniform grid and uses trapezoidal
weights. For weight `wλ`, illuminant `E`, reflectance `R`, sensitivity `S`, and
CIE color-matching function `C`,

```text
camera channel = Σλ wλ E(λ) R(λ) S(λ)
reference XYZ  = Σλ wλ E(λ) R(λ) C(λ)
```

The perfect-diffuser reference is scaled to `Y = 100` and supplies the CIELAB
white. The camera and target patch sets then enter an unconstrained 3×3
least-squares fit. A second fit constrains camera white to map exactly to the
reference white, exposing how much that condition changes the result.

### Scores

The primary score is

```text
SMI = 100 − slope × mean ΔE76
```

with declared slope `5.5`. CIEDE2000 statistics are evaluated on the same fitted
patches but remain separate diagnostics. They cannot be substituted into the
SMI formula.

This is an ISO 17321-style implementation, not a bit-exact Annex-B
equivalence. Chart selection, illuminant, wavelength grid, matrix constraint,
and optimization convention all belong with the result.

## Invalid inputs

The public modules refuse conditions under which their result would be
undefined or misleading:

| Module | Rejected condition |
| --- | --- |
| Closure | fewer than two wavelengths; non-increasing or nonuniform grid; size mismatch; non-finite spectral, white, or measured values; invalid gate tolerance; no patches; failed numeric scale or relative residual |
| Luther quality | fewer than four wavelengths; non-increasing grid; size mismatch; non-finite or zero-norm vectors; rank-deficient sensitivity basis |
| SMI | fewer than two wavelengths; non-increasing or non-finite grid; size mismatch; non-finite spectral vectors; fewer than three test colors; non-positive or non-finite slope; non-positive reference or camera white response; singular fit |

The closure white-card mismatch is different: it returns an unresolved result
with no patch predictions because the failed pairing is a scientific outcome,
not malformed input.

## What the public tests establish

- [`test_spectral_closure.cpp`](../code/tests/test_spectral_closure.cpp) recovers
  an exact global scale, proves the white gate precedes the fit, forces a visible
  channel imbalance, and exercises the invalid-input cases.
- [`test_spectral_quality.cpp`](../code/tests/test_spectral_quality.cpp) uses one
  observer target outside the sensitivity span, pins the exact combined
  residual, checks scaling invariance to `1e-12`, and rejects invalid bases and
  vectors.
- [`test_spectral_smi.cpp`](../code/tests/test_spectral_smi.cpp) compares an
  exactly colorimetric synthetic camera with a wavelength-shifted counterexample,
  pins the score definition, checks white preservation, and exercises the
  declared rejections.

These are library-level tests on synthetic arrays. They establish what the
published numerical functions compute. They do not validate command-line
orchestration, private archive selection, monochromator behavior, or the
physical accuracy of a retained sensitivity curve.
