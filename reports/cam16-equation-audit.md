# CAM16 equation audit

[Study](../studies/color-model-equation-audit.md) ·
[Method and formulas](../methods/cam16-equation-audit.md) ·
[Data](../data/cam16-equation-audit.csv) ·
[Equation-audit code](../code/src/cam16_equation_audit.cpp) ·
[Browser calculator](https://ferazambuja.github.io/imaging/cam16-hellwig-comparator/) ·
[Standalone comparator](https://github.com/ferazambuja/cam16-hellwig-comparator)

## Why audit an appearance-model equation?

Color appearance models connect colorimetry with perceptual attributes under
specified viewing conditions. Because their equations are coupled, changing
one term can have a consequence that is obscured when the term is quoted by
itself. The source paper makes the same point about its own proposal: “changes
made to one part of a color appearance model can have unexpected repercussions
in other parts.” The practical question is therefore not only whether an
equation was transcribed correctly, but also whether the interpretation
survives when the surrounding terms are restored.

## What the source paper argues

Hellwig and Fairchild revisit how CIECAM02 and CAM16 relate brightness to
lightness, and trace the nonlinearity between them to a transcription rather
than to a measurement: “the nonlinear relationship between lightness, J, and
brightness is an artifact of how the Hunt model was transcribed to CIECAM97s.”

They state the consequence as a thought experiment. Asked to pick the gray
card halfway between black and white by lightness, and then again by
brightness, an observer picks the same card. CAM16 cannot: “the card that
CAM16 predicts to have middle lightness will always be lighter than the card
that CAM16 predicts to be middle brightness.”

Replacing that nonlinearity forces a reevaluation of the chroma, colorfulness,
and saturation equations, which is where the background-dependence question
below comes from. The paper also identifies a limit case in the current
formulation: below a background luminance factor of 20, “the chroma of all
colors increases, approaching infinity as `Y_B` approaches zero,” a behavior it
attributes to the `N_cb` term.

That is the setting for this audit. It reproduces a deliberately small subset
of those deterministic consequences, includes the proposal's worse reported
fit to the LUTCHI colorfulness data, and says where a calculation about
equations stops and a statement about observers would begin.

## Normalized brightness

Within one fixed viewing-condition contract, normalized CAM16 brightness is:

```text
Q / Q_white = sqrt(J / 100)
```

The proposed relation is linear:

```text
Q / Q_white = J / 100
```

Both map `J = 0` to black and `J = 100` to white. Inside that interval they
differ: at `J = 25`, CAM16 gives `0.5` while the linear relation gives `0.25`;
at `J = 50`, they give approximately `0.707` and `0.5`. The calculation shows
the consequence of the two definitions. It does not decide which better
predicts an observer.

## Isolated and coupled background behavior

Holding every other term fixed, the `N_cb^0.9` contribution relative to
`Y_background = 20` reduces to:

```text
isolated factor = (20 / Y_background)^0.18
```

| Relative background | Isolated factor |
|---:|---:|
| 20 | 1.000 |
| 5 | 1.283 |
| 1 | 1.715 |
| 0.1 | 2.595 |

That is only one factor inside CAM16 chroma. Restoring the other
background-dependent terms gives:

```text
C(Y_background) / C(20) =
    (n_ref / n)^0.18
  × [(1.64 - 0.29^n) / (1.64 - 0.29^n_ref)]^0.73
  × (J_ref / 100)^[(z(n) - z(n_ref)) / (2 z(n_ref))]

n = Y_background / 100
z(n) = 1.48 + sqrt(n)
```

The implementation holds the adapted responses fixed and sweeps reference
lightness from `J = 10` through `90`.

| Relative background | Isolated factor | Coupled-expression range |
|---:|---:|---:|
| 5 | 1.283 | 1.112–1.263 |
| 1 | 1.715 | 1.416–1.725 |
| 0.1 | 2.595 | 2.120–2.687 |

At `Y_background = 5`, the coupled result stays below the isolated factor. At
`1` and `0.1`, it crosses that factor as lightness changes. The direction and
size of the difference therefore depend on both background and reference
lightness. Quoting `2.595×` alone would conceal that interaction.

## Corrected coefficient and retained tradeoff

Equation 23 was corrected after first online publication. The paper prints the
notice directly beneath the equation: “[Correction added on 22nd April 2022,
after first online publication. Equation (23) correction has been updated.]”
Two forms are therefore in circulation, and a reproduction agrees only when it
uses the same one. This implementation pins the corrected form, whose leading
coefficient is `43`:

```text
M = 43 N_c e_t sqrt(a² + b²)
```

For `N_c = e_t = 1` and a 3-4-5 opponent vector, direct substitution gives
`43 × 5 = 215`. The test suite asserts that value, so a silent revert to an
uncorrected coefficient fails the build rather than shifting every published
colorfulness figure at once.

The paper reports the following squared correlations:

| Dataset / correlate | CAM16 | Proposed relation | Reported in |
|---|---:|---:|---|
| LUTCHI brightness | 0.86 | 0.95 | Figure 2 |
| Munsell chroma | 0.87 | 0.96 | Figure 6 |
| LUTCHI colorfulness | 0.81 | 0.71 | Figure 7 |

The last row matters: the proposal improves the reported brightness and chroma
fits but worsens colorfulness on the listed dataset. The paper does not treat
that as a defect to be hidden. It argues the proposed colorfulness must scale
with the achromatic white signal `A_W` so that colorfulness and brightness stay
in proportion as scene luminance changes—the condition for saturation to remain
invariant to luminance level—and concludes on that basis that “the worse
performance on the LUTCHI colorfulness data by the proposed colorfulness
formulas is permissible.”

These are values reported by the paper, not correlations independently
reproduced from observer records in this portfolio.

![Three-panel CAM16 equation audit showing normalized brightness, background-dependent chroma terms, and published fit statistics](../figures/cam16-equation-audit.svg)

*The straight brightness line is the proposed relation; the curved line is
CAM16. In the center panel, the gap and crossing between the isolated term and
the coupled range are the finding—not plotting uncertainty. The right panel
preserves all three reported comparisons.*

## What this calculation cannot answer

The CSV is fully regenerable from the published equation module. The tests pin
the curve sizes, representative points, coupled endpoints, corrected
coefficient, and domain rejections.

The separate [Python companion](https://github.com/ferazambuja/cam16-hellwig-comparator)
implements both six-correlate forward paths for declared XYZ and viewing
conditions, making the formulation comparison usable outside this report's
fixed grid. That software extension does not add perceptual validation. Observer
validation would still require suitable observer data and a study designed for
that question; extending either numerical grid would not cross that boundary.

Neither this audit nor the companion maps results into CAM16-UCS. That follows
the paper's own limit on how far its proposal has been carried: “The uniform
color space CAM16-UCS was not considered in this article and certainly needs to
be revised and refit to experimental data given the changes proposed here.”

## Source

Luke Hellwig and Mark D. Fairchild, “Brightness, Lightness, Colorfulness, and
Chroma in CIECAM02 and CAM16,” *Color Research & Application* 47 (2022),
1083–1095, [doi:10.1002/col.22792](https://doi.org/10.1002/col.22792).

Equation 23 carries a correction added 22 April 2022, after first online
publication. The corrected form is the one implemented here; quotations above
are from the corrected article.
