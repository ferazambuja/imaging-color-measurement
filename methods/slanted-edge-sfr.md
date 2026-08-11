# Slanted-edge SFR: formulas and implementation

[Study](../studies/sfr-aperture-and-field.md) ·
[report](../reports/sfr-mtf.md) ·
[header](../code/include/camera_iq/sfr.hpp) ·
[implementation](../code/src/sfr.cpp) ·
[tests](../code/tests/test_sfr.cpp)

The code linked here is the estimator that produced the published results, not
a simplified restatement of it. What is excluded is everything around it: RAW
decoding, archive layout, command handling, and the parsers for the advisory
tool's output.

## Inputs

The estimator takes a non-owning view of an already black-subtracted 2×2 mosaic
([`cfa_image_view.hpp`](../code/include/camera_iq/cfa_image_view.hpp)):

```cpp
struct CfaImageView {
  int width, height, row_stride_pixels;
  std::array<int, 4> color_at_position;
  std::string_view cdesc;
  std::span<const double> samples;
  double white_level;
  std::array<double, 4> black_per_channel;
};
```

Two things are deliberate. Black subtraction happens **before** this boundary,
because subtracting a pedestal twice is silent and produces a plausible wrong
answer. The white and black levels come along anyway, because near-saturation
has to be detectable here: clipping flattens the bright side of a transition and
makes a system look sharper than it is.

Position index is `(y & 1) * 2 + (x & 1)`, and `color_at_position` maps that to a
channel. Green occupies two of the four positions, so a region is snapped to
whole 2×2 blocks before sampling
([`roi.cpp`](../code/src/roi.cpp)) — otherwise one green position would be
weighted more heavily than the other.

## From edge to MTF

**1 — Locate the edge per scan line.** For each line crossing the transition,
the centroid of the derivative gives a sub-pixel crossing position. Fitting a
straight line through those centroids gives the edge angle. The tilt is what
makes the method work: each line samples the transition at a different sub-pixel
phase.

**2 — Project and bin.** Every green sample is projected onto the edge normal,
giving its signed distance `d` from the fitted line. Samples are accumulated
into bins of `0.25 px`, producing an oversampled edge-spread function — four
times finer than the sensor pitch, from data the sensor could not resolve
directly.

**3 — Require two-sided support.** With `d10` and `d90` the 10% and 90%
crossings, the transition center is `d_c = (d10 + d90) / 2`. Let `s_short` and
`s_long` be the shorter and longer distances from `d_c` to the ends of the
measured range. The region is rejected when

```text
2 * s_short < s_long
```

and otherwise the data is cropped to `[d_c - s_short, d_c + s_short]`. A Fourier
estimate needs measured plateau on **both** sides of the transition; windowing
cannot recover a side that was never sampled. Cropping symmetrically also puts
the transition at the midpoint of the array, which is what makes the positional
window in step 5 centered on it rather than on an arbitrary region boundary.

**4 — Differentiate.** `LSF[i] = ESF[i+1] − ESF[i]`, the line-spread function.

**5 — Window.** A Hamming window tapers the ends so the finite record does not
introduce its own frequency content:

```text
w[i] = 0.54 − 0.46 * cos(2 * pi * i / (N − 1))
```

**6 — Transform and normalize.** The discrete Fourier magnitude of the windowed
LSF, divided by its zero-frequency value, gives modulation against frequency.
Bin `k` corresponds to

```text
f_k = (k / N) / bin_spacing   cycles per pixel
```

**7 — Correct the differencing.** Step 4 is a finite difference, not a
derivative, and it attenuates high frequencies by a known factor. Dividing it
out is why the result is comparable to an analytic MTF rather than merely
self-consistent:

```text
MTF(f) = |DFT(w · LSF)|(f) / |DFT(w · LSF)|(0) / |sinc(f * bin_spacing)|
```

## Reported quantities

| Field | Meaning |
|---|---|
| `mtf50_cy_per_px` | Frequency where MTF first falls to 0.5 |
| `mtf50p_cy_per_px` | Same, relative to the curve's own peak rather than to DC |
| `mtf_at_nyquist` | Response at 0.5 cycles/pixel, the sampling limit |
| `r1090_px` | 10–90% rise distance, a spatial-domain cross-check |
| `edge_angle_deg` | Fitted angle; a near-zero or near-45° angle is rejected |
| `orientation` | Whether the edge runs predominantly horizontally or vertically |

`mtf50` and `mtf50p` differ when the curve overshoots above its DC value —
sharpening, or ringing from a strong optical low-pass filter. Carrying both
keeps that distinguishable instead of hidden.

## Invalid inputs

Every rejection sets `accepted = false` with a named `rejection_reason`. Common
codes at the public estimator boundary include:

| Reason | Condition |
|---|---|
| `insufficient_edge_support` | The two-sided rule in step 3 fails |
| `low_contrast` | The transition is too shallow to locate |
| `rise_distance_not_found` | The 10% or 90% crossing is not measured |
| `dc_normalization_zero` | The zero-frequency term vanishes |
| `roi_saturated` | One or more selected samples reaches the declared near-saturation gate |
| `roi_too_small` | The clipped, CFA-balanced region is absent or below the minimum size |
| `invalid_raw_image` | Image dimensions, stride, sample span, levels, or selected samples are invalid |
| `edge_angle_out_of_range` | The fitted edge angle is outside the declared operating range |
| `nyquist_not_sampled` | The computed frequency axis does not contain 0.5 cycles/pixel |
| `mtf50_not_found` / `mtf50p_not_found` | The required falling crossing is not measured |

No zero sentinel is accepted as an unmeasured MTF quantity. A missing Nyquist
sample or falling crossing makes the result a rejection rather than a plausible
placeholder.

## What the tests pin

[`test_sfr.cpp`](../code/tests/test_sfr.cpp) runs entirely on synthetic edges
generated in the test itself, so the numerical behavior is checkable without
any camera capture.

The central case is an oracle: a Gaussian-blurred step of width `sigma` has
MTF50 at `0.18739 / sigma` cycles per pixel analytically, and the implementation
recovers it to within `0.018`, with edge angle to within `0.08°`. A separate
narrow-Gaussian oracle checks the numerical Nyquist response to within `0.03`.
For the broad Gaussian, the positive lower gate rejects a zero/default result
and the upper gate establishes strong suppression; those two inequalities do
not themselves establish Nyquist accuracy.

The remaining cases cover DFT bin placement; the differencing correction at DC
and three non-zero frequencies; horizontal and vertical edges; stability when
the same edge is translated within a sufficiently supported region; crossings
between DC and the first non-DC bin; and explicit rejections for missing Nyquist,
missing post-peak MTF50P, insufficient support, low contrast, near-saturation,
invalid image geometry, out-of-range angle, non-finite samples, and the
mosaic-block rule.

Numeric tolerances retain the estimator's reviewed synthetic regression bounds;
they were not tightened around the output of this public copy.
