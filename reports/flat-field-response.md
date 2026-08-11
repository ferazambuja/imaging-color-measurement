# CFA flat-field response: archive screening and spatial characterization

[Reports](README.md) · [reader-focused study](../studies/cfa-flat-field-response.md) ·
[method and formulas](../methods/flat-field-response.md) ·
[screening table](../data/flat-field-screening.csv) ·
[response table](../data/flat-field-response.csv)

## Summary

This report tests whether a centered radial model describes an archived
integrating-sphere capture series. It retains the four CFA positions
independently, screens for headroom before normalization, and measures spatial
response on three accepted frames.

The primary accepted frame reaches **0.480104** green response relative to its
center and produces **19.648%** green corner-field asymmetry. The matched repeat
measures **19.996%**; the third accepted exposure measures **16.087%**. Because
the four corner blocks are at equal radius, those values exclude a
radius-only scalar model for the measured capture-system field. They do not
identify which component caused the departure.

## The experiment this would ideally have been

A camera-only shading characterization would use an independently mapped,
stable source; retain source radiance and spectrum; acquire unsaturated repeats
at several apertures; rotate source and camera relative to one another; verify
dark and linearity controls; and repeat with matched lens/body substitutions to
separate optical and sensor-side terms.

The retained work was not designed to isolate those components. It was an earlier course
project whose 52 sphere frames, dark controls, and exposure metadata survived,
but whose source-uniformity map, rotation controls, alignment record, and
repeatability plan did not. The analysis can recover a bounded composite-field
result. It cannot reconstruct controls that were never recorded.

## What the archive supports

The screening table contains 52 neutral public sample identifiers:

| Aperture | Retained frames | Accepted | Rejected |
|---|---:|---:|---:|
| f/5.6 | 18 | 0 | 18 |
| f/8 | 21 | 3 | 18 |
| f/9 | 13 | 0 | 13 |
| **Total** | **52** | **3** | **49** |

Every rejection is a near-ceiling rejection. No usable frame remains at f/5.6 or
f/9, so the archive cannot support an aperture trend. The accepted set consists
of two f/8, 1/1000 s frames and one f/8, 1/1600 s frame.

This is a limitation of the retained exposure series, not a reason to coerce
the brighter frames into measurements. Once clipping has compressed the field,
no later normalization can recover the response that was clipped away.

## Input and admission conditions

The measurement consumes a signed, black-subtracted Bayer mosaic. For CFA
position `p`, the headroom denominator is the signal-referred range:

```text
ceiling[p] = white level - black level[p]
near threshold[p] = 0.98 × ceiling[p]
```

Near-ceiling and finite-coverage fractions are measured over both the whole
plane and a centered gate, separately for all four positions. The declared
admission policies are:

| Check | Declared condition | Why it exists |
|---|---:|---|
| Near ceiling | no more than 1% at or above 98% of signal range | clipping makes falloff look smaller |
| Screening coverage | at least 90% finite in full plane and gate | a low ratio over a mostly missing plane does not describe a usable field |
| Center signal | center median at least 5% of signal range | protects every normalization denominator |
| Negative samples | no more than 1% | catches black/pedestal inconsistency |
| Per-bin coverage | at least 90% finite | rejects incomplete maps |

These are project analysis choices, not camera-industry standards. The measured
fractions travel with each verdict in the screening table.

### Why whole-frame screening is insufficient

For the f/8, 1/500 s negative case, the worst CFA position measures
**0.496401%** near ceiling over the whole frame and **11.631902%** inside the
center gate. The whole-frame number alone passes the 1% condition; the center
number rejects it. The gate therefore prevents a locally clipped normalizer
from making the response appear artificially flat.

## Spatial calculation

Each accepted CFA plane is divided into a 16 × 12 grid. Every cell stores the
median black-subtracted signal, and each plane is divided by its own separate
center-block median. Red, green-1, green-2, and blue are never pooled before
normalization.

The public response table contains **576 rows**: 192 spatial cells for each of
the three accepted fields. For the primary field:

| Quantity | Minimum | Maximum |
|---|---:|---:|
| Green relative response | 0.480104 | 1.000534 |
| `C_RG` | 0.977316 | 0.999956 |
| `C_BG` | 0.999718 | 1.044729 |
| `C_G1G2` | 0.998943 | 1.002342 |

The large intensity falloff and the smaller chromatic shifts are different
results. The separate map ranges in the figure keep them from being mistaken
for equal-size effects.

## Equal-radius corner test

Four blocks of equal size are inset symmetrically from the frame edges. After
center normalization, the green statistic is:

```text
G(q) = [R_G1(q) + R_G2(q)] / 2
A = [max_q G(q) - min_q G(q)] / mean_q G(q)
```

Equal radius is the precondition: a centered scalar field of radius assigns the
same value to all four blocks and drives `A` to zero. The code rejects odd
mosaic geometry or blocks that cannot preserve this relationship.

| Accepted field | Exposure | `A` | Declared policy exceeded? |
|---|---|---:|---|
| flat-field-19 | f/8, 1/1000 s | 0.196484 | yes |
| flat-field-20 | f/8, 1/1000 s | 0.199964 | yes |
| flat-field-25 | f/8, 1/1600 s | 0.160875 | yes |

The primary and repeat differ in `A` by 0.00348, while the third differs from
the primary by 0.03561. This supports the high-asymmetry verdict in all three
frames; it does not establish a population repeatability distribution or
calibrate the 0.05 policy.

## Matched pair and dark-control scope

For the two 1/1000 s fields, the maximum absolute change over four corners and
four CFA positions is **0.378748 percentage points**, with **0.181309 pp RMS**.
A purely multiplicative exposure change cancels after each frame is normalized
to its own center, so the nonzero pair difference remains visible rather than
being absorbed into exposure scale.

All three accepted rows record `dark_controls_verified = true`. Detailed and
response results require that bounded control because an additive pedestal does
not cancel from center-normalized corners. The 52-row screening inventory has a
different job: it records whether a frame reaches the response-analysis gate,
so rejected screening rows intentionally defer the pedestal check instead
of requiring a full dark pairing for a map that will never be computed.

That separation is important. A screening pass is not a response result, while
a verified dark control is a supporting check rather than a correction—the
dark is not subtracted from the already black-subtracted sphere samples.

## What this result does not identify

The minimum green map cell is bottom-left in all three accepted fields, and the
top-right is the brightest corner cell. The repeated orientation constrains the
observation to the retained capture geometry.

It does not identify a physical cause. The source port was not independently
characterized, and no source/camera rotation pair survived. Illumination,
alignment, lens shading, mechanical obstruction, sensor angular response, and
residual dark behavior cannot be varied one at a time in these records. They
are confounded, so the result is reported for the complete capture system.

## What would resolve it

The next experiment should acquire unsaturated repeats at several apertures,
measure the source field independently, and rotate the camera relative to the
source. That would separate source-fixed from camera/lens-fixed structure. A
matched lens/body substitution would then be needed to narrow optics versus
sensor-side contributions.

Until those controls exist, the defensible conclusion is specific: the
retained capture path required a directional spatial map, and a centered radial
scalar correction would not describe it.
