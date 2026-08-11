# Display-P3 to sRGB gamut mapping

[Studies](README.md) · [scientific report](../reports/gamut-mapping.md) ·
[method and formulas](../methods/gamut-mapping.md) ·
[reference code](../code/src/gamut_mapping.cpp)

No method wins outright, and that is the result. Moving the same radial rule
from CIELAB to OkLCh cut the grid maximum from **23.928** to **9.956** while
raising the mean from **2.857** to **2.947**. P3 yellow improved more sharply,
to **5.523**, but the worst case moved to red: a strong local repair was not a
universal improvement.

## The question

A Display-P3 color can lie outside sRGB. Showing it on an sRGB display then
requires a decision: clip it, reduce its chroma, or move it by some other rule.
That choice can preserve one property while damaging another, so this study
asks two controlled questions:

1. What changes when the same radial rule moves from CIELAB to OkLCh?
2. What changes when the coordinate space stays OkLCh but the rule changes
   from radial clipping to Local MINDE?

The comparison matters because “in gamut” is only a validity condition. It does
not say whether the mapped color retained useful distinctions, avoided a severe
local error, or kept a stable hue coordinate.

## The experiment

The input is a deterministic five-level cube in encoded Display-P3:
`5 × 5 × 5 = 125` colors. Each color is converted through D65 XYZ and mapped
to ideal sRGB by four declared methods:

- **CIELAB radial:** preserve L\* and Lab hue, reduce C\* to the first
  neutral-connected sRGB boundary.
- **OkLCh radial:** keep the radial rule but preserve OkLab lightness and OkLCh
  hue instead.
- **Local MINDE:** use the binary-search algorithm in the 28 July 2026 CSS
  Color 4 Candidate Recommendation Draft.
- **CIELAB soft knee:** preserve a core below 75% of the radial boundary and
  compress the shoulder smoothly; this is an experimental baseline, not a
  standard rendering intent.

Ninety-four of the 125 colors begin outside sRGB. The grid is deliberately a
stress set, not an estimate of how frequently photographs contain these colors.

![Display-P3 to sRGB gamut-mapping comparison](../figures/gamut-mapping.svg)

*Left: each line joins an input color to its CIELAB-radial result, so a longer
line means more chroma was removed. Upper right: displacement distributions for
the CIELAB radial and soft-knee methods. Lower right: modified-color count,
grid-mean CIEDE2000, and the 90th-percentile absolute IPT hue-coordinate change
among modified colors with defined input and output hue. These are numerical
diagnostics, not observer preference scores.*

## What changed

CIEDE2000 is used here as a numerical distance between the original and mapped
color: lower means less displacement under that calculation, not automatically
better appearance. “IPT hue p90” is the 90th percentile of the absolute IPT
hue-coordinate change among modified colors whose input and output hue are
defined. The two columns use different populations on purpose: the mean covers
all 125 grid colors, while the hue percentile covers only the colors a method
actually moved.

| Method | Modified | Mean CIEDE2000 | Maximum | Worst grid color | IPT hue p90 |
|---|---:|---:|---:|---|---:|
| CIELAB radial | 94 / 125 | 2.857 | 23.928 | P3 yellow | 2.781° |
| OkLCh radial | 94 / 125 | 2.947 | 9.956 | P3 red | 3.368° |
| Local MINDE | 94 / 125 | 2.323 | 7.602 | P3 red | 4.806° |
| CIELAB soft knee | 108 / 125 | 4.195 | 24.026 | P3 yellow | 5.720° |

### Coordinate choice fixed one severe failure, not the whole grid

For Display-P3 yellow, CIELAB radial mapping reduced Lab chroma from `127.63`
to `22.74` and produced a CIEDE2000 displacement of `23.928`. Using the same
radial rule in OkLCh retained `0.211` chroma instead of `0.058` and lowered
that color's displacement to `5.523`.

That is the strongest local improvement in the study, but the grid mean rose
from `2.857` to `2.947`, the worst point moved from yellow to red, and the IPT
hue tail widened from `2.781°` to `3.368°`. The result therefore shows a
targeted correction, not an improvement for every color.

### Algorithm choice lowered displacement and widened the hue tail

With OkLCh held fixed, Local MINDE lowered the grid mean from `2.947` to
`2.323` and the maximum from `9.956` to `7.602`. Its 90th-percentile IPT
hue-coordinate change increased from `3.368°` to `4.806°`, however, and the
count above `3°` rose from 10 to 23.

One displacement statistic cannot settle that trade. Local MINDE moved colors
less by CIEDE2000 on this grid, while a separate hue-coordinate diagnostic
showed a wider tail.

### A smooth shoulder was not automatically better

The soft-knee method modified 108 colors, including 14 that were already in
sRGB but lay above the protected core. It increased mean displacement to
`4.195` and reached only `0.913` median boundary utilization among modified
colors. Smoothness at the knee is a mathematical property; it does not by
itself establish a better rendering result.

## Why yellow failed under CIELAB radial mapping

The CIELAB baseline searches along a constant-L\*, constant-hue ray and uses
the first gamut exit connected to neutral. For P3 yellow, the legal source
color lies in a later high-chroma region: input C\* is `127.63`, while the
first connected Display-P3 boundary is only `28.48` and the sRGB boundary is
`22.74`. The method therefore discards `104.89 C*` even though the original
color is legal in its source space.

This is not a data error. It is a concrete counterexample to assuming that a
radial gamut is one continuous interval from neutral to the source color.

## What this establishes

The experiment separates two design decisions that are often changed at the
same time:

- changing coordinates can repair a severe ray-geometry failure while making
  an aggregate slightly worse;
- changing the algorithm can reduce color-difference displacement while
  worsening a separate hue-coordinate tail; and
- a continuous compression curve can still move more colors and preserve less
  boundary reach than a hard method.

The result is a numerical comparison of ideal encoding gamuts. It is not a
measurement of a display or printer, an image-rendering preference study, or a
basis for calling one method universally best.

## What a stronger test would require

A practical rendering study would apply the declared methods to representative
images, characterize the actual source and destination devices, control the
viewing environment, and collect observer judgments tied to a specific task.
That experiment could ask whether the numerical trade-offs found here predict
visible differences or preference. The present study identifies the algorithms
and counterexamples such an experiment should include; it does not substitute
for it.
