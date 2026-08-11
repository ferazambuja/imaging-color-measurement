# Auditing a color-appearance equation before trusting it

[Report](../reports/cam16-equation-audit.md) ·
[Method and formulas](../methods/cam16-equation-audit.md) ·
[Data](../data/cam16-equation-audit.csv) ·
[Equation-audit code](../code/src/cam16_equation_audit.cpp) ·
[Browser calculator](https://ferazambuja.github.io/imaging/cam16-hellwig-comparator/) ·
[Standalone comparator](https://github.com/ferazambuja/cam16-hellwig-comparator)

A color-appearance model tries to predict perceptual attributes such as
brightness, chroma, and colorfulness under stated viewing conditions. Its
equations are often inherited through papers and standards, where a term can
look harmless in isolation while interacting differently with the rest of the
model. This study turns a bounded subset of published CAM16-related equations
into numerical experiments so that those interactions can be inspected rather
than assumed.

One term makes the case. Taken alone it reaches **2.595×**, while the complete
coupled expression spans **2.120–2.687×** — crossing that value from both
sides. The isolated factor is therefore neither a floor nor a ceiling on the
expression it belongs to.

## The question

What do the proposed brightness relation and the background-dependent chroma
terms actually predict, and which conclusions remain valid when an isolated
factor is put back into the coupled expression that contains it?

## What was calculated

The experiment has three parts:

1. Compare normalized CAM16 brightness, `sqrt(J/100)`, with the proposed linear
   relation, `J/100`, over `J = 0…100`.
2. Evaluate the isolated `N_cb^0.9` background factor relative to
   `Y_background = 20`, then evaluate the complete background-dependent chroma
   expression over `J = 10…90`.
3. Pin the corrected coefficient in the paper's colorfulness equation and
   retain all six published fit statistics, including the unfavorable one.

The inputs are deterministic equation values. No camera, display, printer, or
observer was measured in this study.

## What the audit found

The two brightness relations agree at black and white but assign different
midpoints. CAM16 reaches half normalized brightness at `J = 25`; the proposed
linear relation reaches it at `J = 50`.

The background experiment exposes the more important lesson. At
`Y_background = 0.1`, the isolated factor is **2.595×**, but the coupled
expression spans **2.120–2.687×** as reference lightness varies from
`J = 90` to `J = 10`. The isolated term sits inside that range. It is neither
a lower nor an upper bound on the complete expression under this sweep.

The paper's reported fits also resist a one-directional summary: brightness
improves from `R² = 0.86` to `0.95` and chroma from `0.87` to `0.96`, while
colorfulness declines from `0.81` to `0.71`.

![Three-panel CAM16 equation audit showing normalized brightness, background-dependent chroma terms, and published fit statistics](../figures/cam16-equation-audit.svg)

*Left: the square-root and linear brightness relations. Center: the isolated
background factor and the coupled range across `J = 10…90`. Right: the source
paper's published fits, including the colorfulness regression.*

## What this establishes—and what it does not

The audit establishes the numerical behavior of these declared equations and
shows why a component should not be interpreted as the complete model. It also
checks that the implementation carries the corrected coefficient `43` in the
paper's colorfulness relation.

The bounded audit itself is not a full forward transform, a standards-
conformance test, or an observer experiment. A separate
[Python companion](https://github.com/ferazambuja/cam16-hellwig-comparator)
evaluates both six-correlate forward formulations for caller-supplied XYZ and
viewing conditions; that makes the equations reusable but does not change what
the audit establishes. The published correlations are retained source-paper values;
the underlying observer datasets were not re-fitted here. A perceptual conclusion
would require suitable observer data, not a larger equation sweep or another
model output table.
