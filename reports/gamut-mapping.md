# Comparing four Display-P3 to sRGB gamut-mapping methods

[Reports](README.md) · [reader-focused study](../studies/gamut-mapping.md) ·
[method and formulas](../methods/gamut-mapping.md) ·
[input grid](../data/gamut-synthetic-input.csv)

## Summary

This report compares four ways to move ideal Display-P3 colors into ideal
sRGB. The experiment changes one design decision at a time: first the radial
coordinate space, then the mapping algorithm. That separation reveals a result
that a single score would hide.

Changing CIELAB radial mapping to OkLCh radial mapping reduced the severe
P3-yellow displacement from `23.928` to `5.523`, but raised the 125-color grid
mean from `2.857` to `2.947` and moved the worst case to red. Keeping OkLCh and
changing only the algorithm to Local MINDE lowered the mean to `2.323` and the
maximum to `7.602`, while increasing the 90th-percentile IPT hue-coordinate
change from `3.368°` to `4.806°`.

No method wins every reported criterion. The useful result is the location and
shape of the trade-offs.

## Where the question came from

An earlier color-management course project reproduced fine-art prints through
a profiled workflow. Its gamut-mapping choice was a setting inside commercial
software: the rendered result could be measured, but the rule that decided how
out-of-gamut colors moved could not be inspected.

This study revisits that practical question at the algorithm level. Instead of
asking only whether a print or screen result differs, it implements declared
rules and records which property each rule preserves, what it changes, and
where it fails.

## Experiment design

### Input and destination

The input table is a five-level cube in encoded Display-P3 with component
levels `0`, `0.25`, `0.5`, `0.75`, and `1.0`. It contains 125 colors; 94 fall
outside ideal sRGB. The cube gives deterministic coverage of axes, corners,
neutrals, and mixed colors. It is not sampled from photographs and carries no
frequency interpretation.

Both RGB spaces use their ideal D65 primaries and the shared piecewise transfer
curve. Mapping is evaluated in unclipped destination-linear RGB. A color is in
gamut only when all three channels lie in `[0,1]` within the declared numerical
tolerance.

### Controlled comparisons

| Comparison | Held fixed | Changed |
|---|---|---|
| CIELAB radial → OkLCh radial | source/destination, radial first-exit rule, input grid | mapping coordinates |
| OkLCh radial → Local MINDE | source/destination, OkLCh coordinates, input grid | mapping algorithm |
| CIELAB radial → soft knee | CIELAB coordinates, radial boundary, input grid | hard boundary clip versus protected-core compression |

This structure prevents a coordinate-space effect from being attributed to an
algorithm change, or vice versa.

### Four declared methods

For CIELAB radial mapping, a color is represented at fixed L\* and hue `h`:

```text
a(C) = C cos(h)
b(C) = C sin(h)
```

If the input is outside sRGB, its chroma is reduced to the first sRGB boundary
connected to neutral. OkLCh radial mapping uses the same rule at fixed OkLab
lightness and OkLCh hue.

Local MINDE follows the Binary Search Gamut Mapping with Local MINDE algorithm
in the [28 July 2026 CSS Color 4 Candidate Recommendation Draft](https://www.w3.org/TR/2026/CRD-css-color-4-20260728/#binsearch). It searches in OkLCh and compares a candidate with its clipped sRGB
color using ΔEOK. The implementation records the draft's `0.02` local
difference threshold and `0.0001` search epsilon. Those constants define this
dated algorithm; they do not turn this experiment into an observer study.

The experimental soft method protects a core below `K = 0.75D`, where `D` is
the connected destination boundary, then maps input chroma `C` by:

```text
C' = K + (D - K)(C - K) / ((D - K) + (C - K))
```

The curve is continuous with unit slope at the knee, strictly increasing, and
approaches `D` without crossing it. It intentionally moves in-gamut shoulder
colors above `K`.

## Why the boundary solver matters

At fixed CIELAB L\* and hue, the inverse-Lab calculation makes each destination
linear-RGB channel a piecewise cubic function of chroma. The solver partitions
the ray at Lab breakpoints, finds channel extrema, enumerates crossings of the
`0` and `1` channel surfaces, and refines the first in-gamut to out-of-gamut
transition.

Taking the first exit is important because a ray can leave sRGB, re-enter, and
leave again. A test color at `L*=96.23856` and hue `1.80124` radians briefly
exits near `C*=57.64`. A coarse membership scan can step across that excursion
and select a later boundary. The analytic crossing search detects the first
transition.

The OkLCh solver uses the same channel-surface idea. At fixed OkLab lightness
and hue, the inverse transform is cubic in chroma without the CIELAB piecewise
breakpoint.

## Results

### Aggregate displacement

| Metric | CIELAB radial | OkLCh radial | Local MINDE | CIELAB soft knee |
|---|---:|---:|---:|---:|
| Input colors outside sRGB | 94 / 125 | 94 / 125 | 94 / 125 | 94 / 125 |
| Colors modified | 94 / 125 | 94 / 125 | 94 / 125 | 108 / 125 |
| Mean CIEDE2000 | 2.857 | 2.947 | 2.323 | 4.195 |
| Maximum CIEDE2000 | 23.928 | 9.956 | 7.602 | 24.026 |
| Worst grid color | P3 yellow | P3 red | P3 red | P3 yellow |
| P3-yellow output OkLCh chroma | 0.058 | 0.211 | 0.211 | 0.057 |
| Median boundary utilization | 1.000 | 1.000 | not applicable | 0.913 |

The full sample tables are published for
[CIELAB radial](../data/gamut-cielab-radial.csv),
[OkLCh radial](../data/gamut-oklch-radial.csv),
[Local MINDE](../data/gamut-css-local-minde.csv), and the
[soft knee](../data/gamut-soft-compression.csv).

### Secondary hue-coordinate diagnostic

Preserving a hue coordinate in one space does not guarantee constant hue in
another. Each modified input and output with defined IPT hue was therefore also
compared by its absolute IPT hue-angle difference.

| Method | Modified colors with defined IPT hue | Median | 90th percentile | Maximum | Above 3° |
|---|---:|---:|---:|---:|---:|
| CIELAB radial | 94 | 0.722° | 2.781° | 12.692° | 8 |
| OkLCh radial | 94 | 0.409° | 3.368° | 10.260° | 10 |
| Local MINDE | 94 | 1.637° | 4.806° | 9.220° | 23 |
| CIELAB soft knee | 108 | 1.086° | 5.720° | 12.961° | 29 |

The coordinate change lowered the median and maximum IPT hue difference while
slightly increasing the 90th percentile and count above `3°`. Local MINDE
lowered the CIEDE2000 aggregates and worst IPT hue difference, but widened the
IPT hue tail. These are model-to-model diagnostics, not visibility thresholds.

## The P3-yellow counterexample

P3 yellow explains most clearly why the CIELAB result cannot be summarized as
“radial clipping removes excess chroma.” Its input C\* is `127.63`, but the
first neutral-connected Display-P3 boundary on that constant-L\*, constant-hue
ray is only `28.48`; the first sRGB boundary is `22.74`. The legal source color
lies in a later, disconnected high-chroma interval.

CIELAB radial mapping therefore removes `104.89 C*` and produces the largest
displacement in the grid. OkLCh radial mapping changes the ray geometry and
retains substantially more chroma. That fixes this counterexample, but the
higher grid mean and new red worst case show why it is not a universal result.

## Tests

The public C++ test exercises the transform matrices, common-gamut identity,
the P3-red boundary at `C*=93.86561347147861` within `2e-9`, the narrow
leave-and-re-enter ray, the Local-MINDE P3-yellow output against an independent
oracle, and rejection of invalid domains or unresolved boundaries.

It also maps a deterministic set of 3,229 legal Display-P3 inputs through all
four methods: a `9 × 9 × 9` component cube containing transfer-function
neighbors and extrema, 2,000 fixed-seed samples, and 500 near-neutral samples.
For that set:

- every result must be finite and independently inside sRGB;
- the CIELAB radial and soft methods may not increase Lab chroma and must
  preserve L\*;
- their Lab-hue bound is `2e-7` radians only when both input and output chroma
  exceed `1e-6`, because an angle is not meaningful at a vanishing radius;
- OkLCh radial mapping may not increase mapping chroma, must preserve OkLab
  lightness, and must hold OkLCh hue within `1e-8` degrees when hue is defined;
  and
- all three hard intents must preserve destination-gamut inputs.

These tests establish numerical behavior and rejection boundaries. They do not
establish image preference, device characterization, or observer response.

## Limitations and next experiment

- The source and destination are ideal encoding spaces, not measured devices.
- The 125-color cube is a stress grid, not an image-color distribution.
- CIEDE2000, ΔEOK, and IPT hue angle answer different numerical questions; none
  is treated as a universal quality score.
- Fixed CIELAB or OkLCh hue is a coordinate constraint, not perceptual-hue
  validation.
- Local MINDE is tied to a dated draft algorithm for individual SDR colors.
- The soft knee is an experimental baseline and was not tested for observer preference.

The resolving extension is an image-and-observer study on characterized source
and destination devices. It would retain the algorithm outputs shown here, add
representative photographs and difficult synthetic colors, control viewing
conditions, and ask task-specific questions about preserved distinctions,
artifacts, and preference. Until then, the defensible conclusion is the
numerical trade-off—not a winning rendering intent.
