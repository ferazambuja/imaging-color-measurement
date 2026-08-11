# Gamut-mapping method and implementation

[Methods](README.md) · [study](../studies/gamut-mapping.md) ·
[scientific report](../reports/gamut-mapping.md) ·
[public header](../code/include/camera_iq/gamut_mapping.hpp) ·
[implementation](../code/src/gamut_mapping.cpp) ·
[tests](../code/tests/test_gamut_mapping.cpp)

## Scope

The published module maps one encoded RGB color at a time between ideal sRGB
and Display-P3. It implements four declared numerical intents and returns both
the mapped color and the diagnostics needed to explain the branch taken.

It does not load ICC profiles, process spatial neighborhoods, characterize a
display, or model observer preference. Those are separate layers rather than
hidden assumptions inside this API.

## Public data types

The interface uses separate types for each stage:

- `EncodedRgb` for nonlinear RGB code values;
- `LinearRgb` for light-linear RGB;
- `Xyz` for relative D65 tristimulus values;
- `Lab`, `Oklab`, and `Oklch` for mapping and diagnostics; and
- `GamutMappingResult` for output plus branch, boundary, coordinate, and
  Local-MINDE diagnostics.

A request names the source space, destination space, intent, boundary-search
options, and soft-knee fraction. Keeping those choices explicit prevents a
result from silently changing meaning when an algorithm changes.

```cpp
GamutMappingResult map_encoded_rgb_to_gamut(
    const EncodedRgb& input, const GamutMapOptions& options);
```

## Calculation flow

```text
finite encoded RGB in [0,1]
  -> piecewise transfer-function decode
  -> source linear RGB to relative D65 XYZ
  -> destination linear-RGB membership test
  -> CIELAB or OkLCh mapping coordinates
  -> first neutral-connected boundary or Local-MINDE search
  -> mapped D65 XYZ
  -> destination linear RGB and independent gamut check
  -> tolerance-scale clamp for encoding only
  -> encoded RGB plus typed diagnostics
```

The final clamp is not the gamut-mapping algorithm. It runs only after the
unclipped destination-linear value has passed the declared gamut test, and
removes tolerance-scale numerical roundoff before encoding.

## RGB and XYZ transforms

sRGB and Display-P3 use the same transfer curve here:

```text
decode(v) = v / 12.92                              for v <= 0.04045
          = ((v + 0.055) / 1.055)^2.4             otherwise

encode(v) = 12.92 v                                for v <= 0.0031308
          = 1.055 v^(1/2.4) - 0.055               otherwise
```

Rational D65 matrices convert each linear RGB space to and from XYZ. The
public tests pin all six primary vectors and their round trips rather than
checking only neutral white.

Destination membership is evaluated in linear RGB:

```text
-tolerance <= R,G,B <= 1 + tolerance
```

The default mapping tolerance is `1e-12`. It belongs to numerical membership,
not to a description of a display's black, peak, quantization, or visibility.

## Analytic first-exit boundary

For CIELAB radial mapping at fixed L\* and hue `h`:

```text
a(C) = C cos(h)
b(C) = C sin(h)
```

Within each branch of the inverse Lab function, X and Z are cubic polynomials
in chroma `C`, while Y is fixed. Multiplication by the XYZ-to-RGB matrix makes
each destination channel another cubic polynomial:

```text
channel(C) = c0 + c1 C + c2 C^2 + c3 C^3
```

The implementation therefore does not advance along the ray at a fixed step.
It:

1. partitions the chroma axis at inverse-Lab branch changes;
2. forms each channel polynomial;
3. finds derivative critical points, giving monotone intervals;
4. brackets every crossing of `channel = 0` and `channel = 1`;
5. tests the intervals in order from neutral; and
6. refines the first in-gamut to out-of-gamut transition.

The returned boundary is the in-gamut side of the final bracket. This handles
rays that leave the RGB cube and later re-enter; selecting the largest in-gamut
chroma would describe a different and unsafe radial contract.

At fixed OkLab lightness and hue, inverse-OkLab LMS components are affine in
OkLCh chroma. Cubing them makes the destination RGB channels cubic without the
piecewise Lab breakpoint. The OkLCh solver then applies the same first-exit
logic.

## Mapping intents

### CIELAB radial

Destination-gamut inputs are identity results. An out-of-gamut input keeps L\*
and Lab hue while its chroma becomes:

```text
Cout = min(Cin, D)
```

where `D` is the first neutral-connected destination boundary.

### OkLCh radial

The rule remains radial, but the fixed quantities are OkLab lightness and
OkLCh hue. The comparison with CIELAB radial therefore isolates the effect of
coordinates rather than changing coordinates and algorithm together.

For a neutral input, hue is undefined. If that neutral is already in gamut it
is returned unchanged and no radial-boundary diagnostics are returned. An
out-of-gamut color with no defined mapping direction is rejected rather than
assigned an arbitrary hue.

### Binary Search Gamut Mapping with Local MINDE

The Local-MINDE path follows the algorithm in the 28 July 2026 CSS Color 4
Candidate Recommendation Draft. For an out-of-gamut OkLCh candidate it:

1. converts the candidate to destination linear RGB;
2. clips the RGB components to the destination cube;
3. converts the clipped result back to OkLab;
4. computes ΔEOK between candidate and clipped color; and
5. searches chroma until the declared local threshold and search epsilon are
   satisfied.

The result can differ slightly in lightness or hue coordinates because the
algorithm may return the clipped color instead of forcing another radial
chroma reduction. The typed result records whether Local MINDE applied, how
many iterations ran, the final ΔEOK, and whether a clipped color was returned.

### Experimental protected-core compression

For destination boundary `D`, knee `K`, and input chroma `C`, values above the
knee use:

```text
C' = K + (D - K)(C - K) / ((D - K) + (C - K))
```

Below `K`, `C' = C`. The default `K = 0.75D` leaves a protected core but moves
the in-gamut shoulder between `K` and `D`. A knee fraction of `1` is rejected
because it leaves no compression span.

## What the result records

`GamutMappingResult` carries:

- input and output Lab, OkLab, OkLCh, XYZ, linear RGB, and encoded RGB;
- whether the input was already in the destination;
- whether the result changed and which branch produced it;
- input and output chroma in both reporting and mapping coordinates;
- the source and destination first-exit brackets when radial diagnostics apply;
- soft-knee position; and
- Local-MINDE threshold, epsilon, iteration count, final ΔEOK, and clip flag.

This is why the report can distinguish “in gamut,” “unchanged,” “mapped to a
radial boundary,” and “returned from a local clipping search” rather than
publishing only an output triplet.

## Invalid inputs

The module rejects:

- non-finite encoded RGB, XYZ, Lab, lightness, or hue;
- encoded components outside `[0,1]`;
- source colors outside the declared source gamut;
- invalid or zero search tolerances;
- non-positive maximum chroma or zero refinement iterations;
- lightness outside the declared CIELAB or OkLab range;
- a soft-knee fraction outside `[0,1)`;
- a missing radial direction for an out-of-gamut neutral;
- a boundary search that cannot converge to its tolerance; and
- any mapped value that fails the independent destination-gamut check.

The implementation reports an undefined or unresolved result instead of
replacing it with a plausible zero, clipped color, or later boundary.

## Tests

The focused public test uses only synthetic inputs and published numerical
references. It covers transfer breakpoints, all six RGB-primary matrix vectors,
common-gamut identity, the CIELAB and OkLCh radial contracts, the dated
Local-MINDE P3-yellow oracle, the narrow first-exit counterexample, the soft
knee, and invalid-domain rejections.

Its deterministic adversarial set contains exactly 3,229 encoded colors:

- `9 × 9 × 9` extrema and transfer-breakpoint-adjacent combinations;
- 2,000 fixed-seed samples from a self-contained generator; and
- 500 near-neutral samples where hue conditions need careful scoping.

Every sample runs through all four methods. Output finiteness and destination
membership apply universally. Fixed-L\*, non-increasing Lab chroma, and the
`2e-7`-radian Lab-hue bound apply only to CIELAB radial/soft results, with the
hue bound further limited to input and output chroma above `1e-6`. The OkLCh
radial `1e-8`-degree hue bound applies only when hue is defined. These are
operating conditions, not universal guarantees.
