# Color-correction matrix: formulas and implementation

[Study](../studies/colorchecker-ccm.md) ·
[CCM report](../reports/ccm-fit.md) ·
[header](../code/include/camera_iq/colorimetry.hpp) ·
[implementation](../code/src/colorimetry.cpp) ·
[tests](../code/tests/test_colorimetry.cpp)

The code linked here is the implementation that produced the published results.
It begins where the measurement ends: with corrected chart RGB and target XYZ
already in hand. Patch extraction, chart localization, reference rendering, and
the spectrum readers are not part of this module — the
[patch-extraction](../reports/patch-extraction.md) and
[reference](../reports/reference-provenance.md) reports explain how its two
inputs were obtained.

## The fit

For patch `i`, let `r_i = (R, G, B)` be linear camera RGB and `t_i = (X, Y, Z)`
the target. The model is

```text
t_i ≈ M · r_i        with M a 3×3 matrix
```

Each output channel is independent, so `M` is three separate least-squares
problems sharing one design matrix. Stacking the patch RGB row vectors into `R`
and one target channel into `t`, the row `m` of `M` for that channel solves the
normal equations:

```text
(Rᵀ R) m = Rᵀ t
```

`Rᵀ R` is 3×3 regardless of patch count, which is what makes this cheap: 140
patches contribute to the accumulation, not to the size of the system.

**Conditioning.** The implementation solves those normal equations by Gaussian
elimination with partial pivoting, and rejects a design whose pivot collapses —
a rank-deficient set, such as patches that all lie on the neutral axis, has no
unique solution and returning one anyway would be worse than failing. Forming
`RᵀR` squares the condition number, which is a real cost in general. The archive
fit completed without the singular-pivot rejection, but no condition number was
retained, so this portfolio does not report a quantified stability margin. A QR
or SVD solve is the appropriate extension for that question.

## Measuring the error

Predicted and target XYZ are converted to CIELAB against the declared reference
white:

```text
f(u)  = u^(1/3)                       if u > (6/29)³
      = u / (3·(6/29)²) + 4/29        otherwise

L* = 116·f(Y/Yn) − 16
a* = 500·(f(X/Xn) − f(Y/Yn))
b* = 200·(f(Y/Yn) − f(Z/Zn))
```

The linear segment near zero gives the transform a finite slope instead of the
cube root's divergent slope at zero, which matters for dark patches.

Two difference metrics are reported:

- **ΔE76** — plain Euclidean distance in CIELAB. Simple, and useful precisely
  because it has no weighting to misapply.
- **CIEDE2000** — adds lightness, chroma, and hue weighting plus a hue-rotation
  term for the blue region. This is the headline metric.

They are **not interchangeable and not convertible**. A ΔE76 figure from one
source cannot be subtracted from a CIEDE2000 figure from another; the study and
reports are explicit about this where both appear.

## Held-out evaluation

`cross_validate_rgb_to_xyz_ccm` assigns patch index `i` to partition `i mod k`,
fits on `k−1` partitions, and evaluates on the held-out one, repeating so every
patch is evaluated exactly once by a matrix that never saw it. The split is
deterministic but not a substitute for a separately captured chart.

Determinism is the design decision worth naming. A random partition would give a
different number on every run, and the reported error would not be reproducible
from the published inputs. The tests assert that repeating the evaluation
returns a bit-identical result.

What this establishes is bounded: it detects a model memorizing individual
patches. It does not establish physical generalization, because every fold is
drawn from the same capture of the same chart.

## Lightness selection and dark-patch diagnostics

`select_reference_lightness` partitions patches by target `L*`, and
`diagnose_dark_patches` evaluates a matrix on the excluded subset. Together
these make a restricted fit auditable: it is only possible to see that a
better-looking headline came from patch selection if the excluded subset is
still evaluated and reported.

## Invalid inputs

The public API rejects rather than returning a plausible default:

| Condition | Behavior |
|---|---|
| Patch and target counts disagree | rejected |
| Rank-deficient design | rejected |
| Non-finite input sample | rejected |
| Fewer than two folds | rejected |

## What the tests pin

[`test_colorimetry.cpp`](../code/tests/test_colorimetry.cpp) runs on synthetic
fixtures and published reference tables only.

The central case generates targets from a known 3×3 matrix and requires the
solver to return that matrix to `1e-10`, with color residual below `1e-9` —
data produced by an exact linear map must leave nothing material behind at the
declared tolerance.

The held-out case is the one that would be easy to write vacuously. Linear data
cannot distinguish training from held-out error, so the fixture bends the
targets with a quadratic term no 3×3 can represent, and the test requires the
held-out-minus-training gap to exceed `1.0` CIEDE2000. This is a property of the
synthetic fixture, not a tolerance applied to the archive result.

The remaining cases cover CIEDE2000 against the supplementary reference pairs
from [Sharma, Wu, and Dalal (2005)](https://doi.org/10.1002/col.20070) at `1e-4`,
including the neutral-chroma edge case and its symmetry; the CIELAB round trip;
`L* = 100` at the reference white; deterministic fold assignment; the
kept/excluded partition; and the four listed rejection cases.
