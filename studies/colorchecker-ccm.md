# How do you know a color matrix is not just fitted to its own data?

A camera's RAW RGB values are not colorimetry. Two cameras photographing the
same chart under the same light record different numbers, and neither set is
CIE XYZ, because each sensor's spectral sensitivities differ from the human
observer's. The usual bridge is a color-correction matrix: a linear 3×3 fit
from one camera's RGB into XYZ for a declared capture condition.

The difficulty is that such a matrix is easy to make look good. Fit it on 140
patches, report the error on those same 140 patches, and you have measured how
well the fit reproduces its own training data — which is not the question anyone
is asking.

This study fits the matrix and then reports error on patches the fit never saw.

Held-out error came to **4.134** mean CIEDE2000 against **4.099** on the
training patches — a gap of 0.035, so these five patch partitions show little
overfitting. The more useful result is what the design caught:
restricting the fit to lighter patches produced a headline that looked **22%
better** while all-patch error stayed at **4.126** and the excluded dark patches
remained poorly predicted at **7.952**.

## What was done

Each of 140 chart patches is sampled from the RAW frame after black
subtraction and demosaic, corrected by a measured flat field, and white
balanced. The reference side comes from the chart's spectral reflectances
rendered under a measured illuminant, giving the XYZ each patch should produce.

![Reduced crop of the ColorChecker-SG patch grid used for the
capture](../figures/context/colorchecker-sg-patch-grid.jpg)

*A reduced crop of the captured chart, showing the patch grid the 140 samples
are drawn from. Illustration only — the crop omits the full target and is not a
calibration reference or an analysis input.*

The fit is repeated over **five deterministic partitions** of the patch set. Each
time, four fifths of the patches train the matrix and the remaining fifth is
evaluated. Every patch is held out exactly once, and the reported error comes
only from patches the matrix had not seen.

Error is reported as **CIEDE2000**, a perceptual color-difference measure where
lower is closer.

![Fit and held-out CIEDE2000 for the all-patch and kept-set
evaluations](../figures/ccm-validation.svg)

*Lower is better. The first two groups compare training error with five-fold
held-out error, for all 140 patches and for the 112-patch `L* ≥ 25` kept set.
The last two bars evaluate the kept-set fit on all 140 patches and on the 28
excluded dark patches.*

## The result

| Evaluation | Patches | Training | Held out |
|---|---|---|---|
| All-patch baseline | 140 | 4.099 | **4.134** |
| Kept set, `L* ≥ 25` | 112 | 3.170 | 3.221 |
| All patches, under the kept-set fit | 140 | 4.126 | — |
| Excluded dark patches | 28 | 7.952 | — |

Held-out error exceeded training error by **0.035**. That small gap is the
answer to the opening question: across these deterministic patch partitions,
the training score is representative of the patches withheld from each fit.
It argues against patch-fold overfit within this calculation. It does not show
that the matrix will transfer to another chart, illuminant, or capture session.

The reported **4.134** is the residual for this complete
camera–capture–reference–model path. No observer experiment or declared viewing
condition was part of the archive, so the value is not converted into a count
of just-noticeable differences. It also cannot be assigned to the camera
alone: model limitation, capture flare, and reference mismatch remain coupled.

## The result the design was built to catch

Restricting the fit to patches above `L* = 25` dropped held-out error from
**4.134** to **3.221** — a 22% improvement, and a much better-looking headline.

It is not an all-patch improvement. Under that same restricted fit:

- all 140 patches changed only from **4.134** to **4.126**;
- the 28 excluded dark patches measured **7.952**.

The lower kept-set score did not transfer to the complete chart. It came from
removing the hardest subset from that average while the subset remained poorly
predicted. The **4.134** figure is therefore the result this study quotes.

Plausible contributors include flare in the bright-surround capture and
mismatch between the compatible reference and the photographed chart unit.
These data cannot establish their relative contributions or exclude other
capture-path effects.

## What this does not establish

**The held-out design tests patch partitions, not physical generalization.**
Every fold comes from one capture of one chart. The 0.035 gap shows the matrix
is not memorizing individual patches; it does not show the matrix transfers to a
second chart, a second illuminant, or a second session.

**The reference is compatible, not per-unit.** The target XYZ comes from a
spectral reference associated with the same chart product, not from a
measurement of the specific physical chart that was photographed. That
compatible reference differs from manufacturer nominal values by **mean ΔE76
1.34** across the 140 patches. That is a different metric under different
weighting conventions, so it is not an amount to subtract from 4.134. The
disagreement shows that reference choice is unresolved; it does not determine
whether or how much of 4.134 came from reference mismatch.

**The scope is one capture condition.** One aperture, one illuminant, one
session. The flat used for the field correction is measured rather than assumed,
but it cannot separate the illumination source from the lens and sensor, so this
is same-aperture correction rather than a component calibration.

The most direct next experiment is a per-unit spectral measurement of the chart
that was actually photographed, with its measurement uncertainty retained.
That removes the product-reference substitution and makes its contribution
estimable; nothing in these archived data can do that retrospectively.

---

**Detail:** [CCM report](../reports/ccm-fit.md) ·
[patch extraction](../reports/patch-extraction.md) ·
[reference provenance](../reports/reference-provenance.md) ·
[method and formulas](../methods/color-correction-matrix.md) ·
[validation summary](../data/ccm-validation-summary.csv) ·
[patch RGB](../data/ccm-patch-rgb.csv) ·
[reference implementation](../code/src/colorimetry.cpp)
