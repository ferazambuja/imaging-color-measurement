# Linear RGB-to-XYZ color correction: fit and held-out evaluation

[Study](../studies/colorchecker-ccm.md) ·
[patch extraction](patch-extraction.md) ·
[reference provenance](reference-provenance.md) ·
[method and formulas](../methods/color-correction-matrix.md) ·
[validation summary](../data/ccm-validation-summary.csv)

## The experiment this would ideally have been

The controlled version measures the specific physical chart that was
photographed, on a well-characterized spectrophotometer, at the time of capture. It
captures the chart under several illuminants and apertures, repeats each capture
so session variation becomes a measured spread, and validates the fitted matrix
against a second chart the fit never saw.

What survived is one capture session: RAW chart frames, flat-field frames, and
dark frames at one aperture under one illuminant, with the illuminant spectrum
recorded in the measurement sidecars. No per-unit measurement of the
photographed chart was retained.

That shapes what the fit can answer. It is enough to ask whether a linear model
generalizes beyond the patches it was fitted on — and to answer it — without
being enough to call the result a chart calibration.

## Method

Camera RGB comes from the patch-extraction path documented
[separately](patch-extraction.md): black subtraction, bilinear demosaic,
same-aperture flat correction, white balance, and rectangular sampling of patch
interiors. Target XYZ comes from the spectral reference described in the
[reference report](reference-provenance.md), rendered under the measured
illuminant.

The matrix is a least-squares solution: for camera RGB `r_i` and target `t_i`,
each output-channel row of `M` solves the normal equations, giving `XYZ = M·RGB`.
Error is converted to CIELAB against the declared white and reported as
CIEDE2000.

**Five-fold held-out design.** Patches are assigned to five deterministic
partitions. Each fold trains on four fifths and evaluates on the remaining
fifth, so every patch is evaluated exactly once by a matrix that never saw it.
The partitions are deterministic, not random: re-running the evaluation gives a
bit-identical result, which matters because a shifting fold assignment would
make the reported error irreproducible.

## Results

From [`ccm-validation-summary.csv`](../data/ccm-validation-summary.csv), mean
CIEDE2000:

| Evaluation | Patches | Fit / training | Five-fold held out |
|---|---|---|---|
| All-patch baseline | 140 | 4.099 | **4.134** |
| Kept set, `L* ≥ 25` | 112 | 3.170 | 3.221 |
| All patches under the kept-set fit | 140 | 4.126 | — |
| Excluded dark patches under that fit | 28 | 7.952 | — |

**Held-out exceeds training by 0.035.** Under these deterministic partitions,
the training score is representative of patches withheld from each fit. The
close scores argue against patch-fold overfit within this calculation; they do
not reproduce the fit on another physical chart.

**4.134 is the reported result.** It belongs to the combined
camera–capture–reference–model path. The archive contains no observer experiment
or declared viewing condition, so the report does not translate this
CIEDE2000 value into a universal visibility or just-noticeable-difference result.

## The dark-patch experiment

Excluding patches below `L* = 25` and refitting lowers held-out error to
**3.221**, 22% below the baseline. The two evaluations that follow are why that
number is not reported as the result:

- Under the restricted fit, all 140 patches change only from **4.134** to
  **4.126**.
- The 28 excluded patches measure **7.952**.

The lower kept-set score did not transfer to the complete chart; it came from
leaving the hardest subset out of that average while the subset remained poorly
predicted. Recording the exclusion as a flare-handling choice, rather than as a
better all-patch model, is the reading these evaluations support.

Plausible contributors include flare in the bright-surround capture and
mismatch between the compatible reference and the photographed chart unit.
They are not separable here, and the archive does not exclude other
capture-path effects.

## Limitations

**Fold partitions are not physical replication.** All five folds draw from one
capture of one chart. The 0.035 gap establishes that the model is not memorizing
patches. It says nothing about a second chart, illuminant, or session.

**The reference is compatible, not per-unit.** See the
[reference report](reference-provenance.md). The compatible reference differs
from manufacturer nominal values by mean ΔE76 1.34 across the 140 patches —
a different metric under different weighting, so not an amount to subtract
from the CIEDE2000 result. The disagreement prevents camera-only attribution;
it does not quantify the reference contribution.

**One capture condition.** One aperture, one illuminant, one session. The
correction flat is measured rather than assumed, but cannot separate the
illumination source from the lens and sensor, making this same-aperture
correction rather than component calibration.

**Numerical scope.** The 3×3 solve uses normal equations with pivoted
elimination and rejects a rank-deficient design. The published fit completed
without that rejection, but no condition number was retained, so this report does
not report a quantified numerical-stability margin. A QR or SVD solution would
be the preferred extension for a conditioning study.

## What would resolve it

A per-unit spectral measurement of the photographed chart, with its own
uncertainty retained, would remove the product-reference substitution and make
that contribution estimable. A second chart, illuminant, and capture session
would extend the evaluation from patch partitions to physical replication.
