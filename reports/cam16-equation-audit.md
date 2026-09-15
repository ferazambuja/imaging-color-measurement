# CAM16 background coupling and Hellwig–Fairchild fit tradeoffs

[Study](../studies/color-model-equation-audit.md) ·
[Method and formulas](../methods/cam16-equation-audit.md) ·
[Data](../data/cam16-equation-audit.csv) ·
[Equation-audit code](../code/src/cam16_equation_audit.cpp) ·
[Browser calculator](https://ferazambuja.github.io/imaging/cam16-hellwig-comparator/) ·
[Standalone comparator](https://github.com/ferazambuja/cam16-hellwig-comparator)

## Coupled terms change the interpretation

Color appearance models connect colorimetry with perceptual attributes under
specified viewing conditions. Because their equations are coupled, changing
one term can have a consequence that is obscured when the term is quoted by
itself. The source paper likewise warns that a local change can affect other
parts of a color appearance model. The practical question is how the isolated
term behaves when the surrounding terms are restored.

## What the source paper argues

Hellwig and Fairchild revisit how CIECAM02 and CAM16 relate brightness to
lightness, and trace the nonlinearity between them to a transcription rather
than to a measurement in the development from the Hunt model to CIECAM97s.

They state the consequence as a thought experiment. Asked to pick the gray
card halfway between black and white by lightness, and then again by
brightness, an observer picks the same card. CAM16 instead predicts different
cards, with its middle-lightness card lighter than its middle-brightness card.

Replacing that nonlinearity forces a reevaluation of the chroma, colorfulness,
and saturation equations, which is where the background-dependence question
below comes from. The paper also identifies a limit case in the current
formulation: below a background luminance factor of 20, chroma rises for every
color and tends toward infinity as `Y_B` approaches zero. It attributes that
behavior to the `N_cb` term.

This report reproduces selected consequences of those equations, uses the
corrected colorfulness coefficient, and compares the paper's reported fits for
brightness, chroma, and colorfulness.

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
the consequence of the two definitions.

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
lightness; the isolated `2.595×` value does not describe the coupled response.

## Corrected coefficient and mixed fit results

Equation 23 was corrected on 22 April 2022, after first online publication.
The authors' downloadable early copy still shows `47`, while the corrected
article and the Colour implementation use `43`. This implementation uses the
corrected form:

```text
M = 43 N_c e_t sqrt(a² + b²)
```

For `N_c = e_t = 1` and a 3-4-5 opponent vector, direct substitution gives
`43 × 5 = 215`, providing a compact numerical check of the corrected form.

The paper reports the following coefficients of determination:

| Dataset / correlate | CAM16 | Proposed relation | Reported in |
|---|---:|---:|---|
| LUTCHI brightness | 0.86 | 0.95 | Figure 2 |
| Munsell chroma | 0.87 | 0.96 | Figure 6 |
| LUTCHI colorfulness | 0.81 | 0.71 | Figure 7 |

The proposal's reported coefficients are higher for brightness and chroma but
lower for colorfulness on the listed datasets. The authors argue that the
colorfulness tradeoff preserves proportionality with brightness as scene
luminance changes, so saturation remains invariant to luminance level. They
also leave the relation between colorfulness and adapting luminance open for
further study.

![Three-panel CAM16 equation audit showing normalized brightness, background-dependent chroma terms, and published fit statistics](../figures/cam16-equation-audit.svg)

*The straight brightness line is the proposed relation; the curved line is
CAM16. The center panel compares the isolated term with the coupled range. The
right panel shows all three fit statistics reported in the paper.*

## Scope of the result

This calculation includes no observer data. Determining which formulation
better predicts appearance requires measurements designed for that question.

Neither this audit nor the
[standalone comparator](https://github.com/ferazambuja/cam16-hellwig-comparator)
maps results into CAM16-UCS. That follows the paper's own limit on how far its
proposal has been carried: CAM16-UCS was outside its scope and would need to be
revised and fitted again before these changes could be used there.

## Source

Luke Hellwig and Mark D. Fairchild, “Brightness, Lightness, Colorfulness, and
Chroma in CIECAM02 and CAM16,” *Color Research & Application* 47 (2022),
1083–1095, [doi:10.1002/col.22792](https://doi.org/10.1002/col.22792).

Equation 23 carries a correction added 22 April 2022, after first online
publication. The corrected form is the one implemented here.
