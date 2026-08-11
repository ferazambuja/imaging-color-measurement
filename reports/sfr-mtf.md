# Slanted-edge SFR across aperture and field: two capture systems

[Study](../studies/sfr-aperture-and-field.md) ·
[method and formulas](../methods/slanted-edge-sfr.md) ·
[published aggregate](../data/sfr-aperture-summary.csv) ·
[figure](../figures/sfr-aperture-field.svg)

## The experiment this would ideally have been

The clean version of this question is a controlled optical-bench comparison. It
would hold one physical lens sample fixed and move it between bodies, verify
focus at each aperture rather than assuming it, repeat every capture so that
focus and alignment scatter becomes a measured spread, record sagittal and
tangential edge orientations separately, and normalize to line pairs per
millimeter so systems with different sensor pitch stay comparable.

None of that was available. What survived is an archived pair of aperture sweeps
and field-map captures made during earlier laboratory sessions, plus one matched
batch of results from a commercial analysis tool generated at the time. The
sessions were run to produce a result, not to survive as a calibration record.

That gap is the reason this report is careful about what it concludes. It is not
a reason the comparison is uninformative: the two sweeps ran through the same
chart with the same lens model, which is enough to ask whether one center number
generalizes — and enough to show that it does not — without being enough to
assign the cause.

## What the archive retained

- Two aperture sweeps of nine apertures each, f/1.4 through f/16, on two camera
  bodies recorded with the same 50 mm f/1.4 lens model at 50 mm, an approximate
  0.84 m focus distance, and ISO 100.
- Field maps at four D810 apertures and all nine D800 apertures, with 23 regions
  per map, contributing the declared **299-region** field set (92 + 207).
- One matched batch of per-file results from a commercial tool, retained as an
  advisory cross-check.

What it does not retain: lens serial identity, controlled refocusing, repeat
captures, synchronized camera clocks, or controlled coverage of both principal
edge orientations. The two sets also differ in a way the metadata does record —
the D810 set used autofocus and the D800 set was focused manually — and that
difference is not separable from the optical differences.

## Measurement design

Each region is measured on sensor-linear green samples taken from the
black-subtracted mosaic. Demosaic, luma conversion, and gamma are all spatial or
tonal operations that would be measured as part of the system response, so none
is applied.

Regions are selected on complete 2×2 mosaic blocks so the two green positions
are equally weighted, and a region is rejected rather than measured when:

- the edge transition lacks measured plateau on both sides of it;
- contrast across the region is too weak to locate the edge;
- the samples approach the sensor ceiling, where clipping flattens the
  transition and makes the system look sharper than it is; or
- the recovered edge angle falls outside the range the geometry supports.

Rejections carry a reason and are retained. A rejected region is a candidate
result the estimator declines to report because its measurement preconditions
were not met.

## Results

Center MTF50 in cycles per pixel, from
[`sfr-aperture-summary.csv`](../data/sfr-aperture-summary.csv). The final
column is center minus the strongest physical corner at the four field-map
apertures; negative means the corner outresolved the center.

| System | f/ | Center MTF50 | Advisory | Center − corner |
|---|---|---|---|---|
| D810 | 1.4 | 0.1075 | 0.1158 | — |
| D810 | 1.8 | 0.0840 | 0.0899 | — |
| D810 | 2 | 0.1081 | 0.1121 | — |
| D810 | 2.8 | 0.1992 | 0.1707 | — |
| D810 | 4 | 0.1997 | 0.1949 | −0.0011 |
| D810 | **5.6** | **0.2713** | 0.2400 | +0.0715 |
| D810 | 8 | 0.2202 | 0.2388 | +0.0244 |
| D810 | 11 | 0.2048 | 0.1989 | +0.0225 |
| D810 | 16 | 0.1668 | 0.1735 | — |
| D800 | 1.4 | 0.1082 | 0.1029 | +0.0111 |
| D800 | 1.8 | 0.1307 | 0.1204 | +0.0251 |
| D800 | 2 | 0.1445 | 0.1377 | +0.0337 |
| D800 | 2.8 | 0.1443 | 0.1395 | −0.0085 |
| D800 | 4 | 0.1426 | 0.1385 | **−0.0457** |
| D800 | 5.6 | 0.1648 | 0.1649 | −0.0238 |
| D800 | **8** | **0.1684** | 0.1831 | −0.0102 |
| D800 | 11 | 0.1674 | 0.1707 | +0.0082 |
| D800 | 16 | 0.1477 | 0.1583 | +0.0113 |

Three results follow directly:

**The D810 center curve has a clean f/5.6 peak.** Among the four mapped field
apertures, the center exceeds the strongest physical corner at f/5.6, f/8, and
f/11; f/4 is a near tie in the other direction.

**The D800 field maximum sits off-axis across four consecutive apertures.** At
f/2.8, f/4, f/5.6 and f/8 the strongest corner outresolves the center. The
largest margin, **0.0457** at f/4, exceeds the entire change in the D800's own
center reading between f/4 and f/8.

**The D800 aperture ordering inverts.** Its f/4 center, **0.1426**, falls below
its f/16 center, **0.1477**. The advisory reference reproduces the inversion
(0.1385 against 0.1583), so it is not an artifact of this implementation.

## Cross-check

The advisory tool runs a rendered-luma and gamma path against this
implementation's sensor-linear green path, so exact agreement is not required.
The comparison is read for trend and plausible scale.

Both agree on where each system peaks, and both reproduce the D800 f/4-below-f/16
inversion. The largest disagreement is the D810 at f/5.6 — **0.2713** here
against **0.2400** advisory — which is the aperture where the two methods differ
most and still place the peak identically.

Beyond the archive, the estimator is checked against synthetic edges with known
answers: a Gaussian-blurred step has MTF50 at `0.18739 / sigma` cycles per
pixel, which the implementation recovers to within 0.018, and edge angle to
within 0.08°. Those tests ship with the code and run in continuous integration;
see [`code/tests/test_sfr.cpp`](../code/tests/test_sfr.cpp).

## Limitations

This is a system measurement, not a lens characterization. It lacks verified
lens-sample identity, controlled refocusing, repeat captures, lp/mm
normalization, and sagittal/tangential coverage. It is sensor-linear green SFR,
not rendered-luminance equivalence.

The D800/D810 difference cannot be attributed to one component. The bodies
specify different optical low-pass filter designs, which is a plausible
body-side contribution but is not isolated here. The shared lens-model label
does not establish a shared physical sample. Focus mode differs between the
sets, and the D800's focus accuracy is unverified — a focus error combined with
field curvature could produce a similar soft-center and off-axis pattern.
Chart alignment, focus-plane tilt, decentering, and edge orientation also remain
uncontrolled alternatives.

## What would resolve it

Two direct controls would narrow the most obvious alternatives:

1. **Refocus and repeat each aperture.** Focus accuracy becomes a measured
   spread instead of an assumption, and a focus error stops being
   indistinguishable from an optical result.
2. **Move one lens sample between both bodies.** The lens is then held fixed by
   identity rather than by model label, which separates the body contribution
   from the lens contribution.

With both in place, lens identity and focus would be better controlled, but the
OLPF contribution would still not be isolated from other body-side and setup
differences. Controlled alignment, orthogonal edge orientations, and an
otherwise matched optical/sensor comparison would be needed for that stronger
ranking. Without them, the honest statement is that these two capture systems
needed separate acceptance criteria, and that the center measurement alone
would not have revealed it.

## Note on scope

Three numbers appear in this work and should not be conflated. The **18** rows
above are the center aperture sweep. The **299** field regions are the declared
public field set. A broader private reprocessing run over the retained archive
was used to verify that a change to the estimator left every accepted region
accepted; that run validates the implementation and is not itself a published
study result.
