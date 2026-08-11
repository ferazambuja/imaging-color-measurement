# Is one center-sharpness number enough?

Lens reviews usually answer "how sharp is it?" with a single measurement taken
at the middle of the frame, plotted against aperture. The shape of that curve is
familiar: soft wide open where residual aberrations dominate, best somewhere in
the middle, softening again when stopped down far enough for diffraction to take
over.

This study compares two archived capture series from different camera bodies,
made with the same 50 mm lens model and chart, asking whether that one center
number describes either field.

For the D810, the center curve behaves the way that summary predicts. For the
D800, the strongest measured corner at each of four consecutive apertures
outresolved the center, by as much as **0.0457 cycles/pixel**, and the f/4
center reading fell below the body's own f/16. Neither departure is visible in
the center curve alone. Different focus methods, unrecorded lens-unit identity,
and uncontrolled alignment and field orientation keep this at capture-session
scope: it is not a camera-body or lens ranking.

## What is being measured

Slanted-edge SFR recovers how much contrast a system preserves as detail gets
finer. Photograph a straight edge tilted a few degrees from vertical, and each
scan line crosses it at a slightly different sub-pixel position; combining the
lines reconstructs the edge profile far more finely than the pixel spacing
alone would allow.

**MTF50** is the usual summary: the spatial frequency, in cycles per pixel, at
which contrast has fallen to half its low-frequency value. Higher means finer
detail survives. It is one point on a curve, not the curve.

The measurement runs on sensor-linear green samples taken straight from the
black-subtracted mosaic — no demosaic, no luma conversion, no gamma — because
each of those steps is itself a spatial filter and would be measured as part of
the result.

![Reduced crop of the SFR target, showing slanted-edge blocks distributed
across the field](../figures/context/sfr-field-target.jpg)

*A reduced crop of the target actually captured. The repeated blocks are what
makes a field measurement possible: the same edge structure appears at many
positions, so center and corner are measured on identical geometry rather than
on whatever detail happens to fall there. Illustration only — not a calibration
reference or an analysis input.*

![Center MTF50 against aperture, and center minus strongest corner, for two
camera systems sharing a 50 mm lens model](../figures/sfr-aperture-field.svg)

*Left: center MTF50 in cycles per pixel against aperture for both systems.
Solid lines are this implementation's sensor-linear green measurement; dashed
lines are an advisory reference from a commercial tool that uses a different
luma and gamma path, read as a consistency check rather than as agreement.
Right: center minus the strongest physical corner. A negative bar means the
corner outresolved the center.*

## The result

The D810 center curve rises to a clean peak of **0.2713 cycles/pixel at f/5.6**.
At the four mapped field apertures, the center exceeds the strongest physical
corner at f/5.6, f/8, and f/11; at f/4 it falls short by **0.0011**, a near tie
at the reported precision.

The D800 system does not reproduce that shape. Its center peaks lower and later,
at **0.1684 at f/8**, and two things go wrong that a center-only number cannot
show:

- **The center is not the sharpest part of the frame across four consecutive
  apertures.** At f/2.8, f/4, f/5.6 and f/8 the strongest measured corner
  outresolves the center, by as much as **0.0457 cycles/pixel at f/4** — larger
  than the entire f/4-to-f/8 change in its own center reading.
- **Its aperture ordering inverts.** The D800 center at f/4 (**0.1426**) sits
  *below* its own f/16 result (**0.1477**), and the advisory reference
  reproduces that inversion. Under a well-focused comparable capture, the
  stronger diffraction penalty at f/16 makes f/4 normally expected to exceed
  it.

The answer is therefore narrower than a body ranking: the selected D810
center-to-corner comparison behaves conventionally at three mapped apertures,
while a center-only summary conceals the D800's off-axis behavior across four.
Nothing in the D800 center curve announces that limitation by itself.

## What this does and does not establish

This is a **capture-system** result. Slanted-edge SFR includes the lens,
aperture, focus and alignment, the optical low-pass filter, sensor sampling, and
the processing path, and this archive does not separate them.

Several explanations for the D800 behavior remain live and are not ranked
here. The retained records keep at least these alternatives open:

- **Optical low-pass filter.** The two bodies specify different OLPF designs,
  which is a plausible body-side contribution.
- **Lens sample.** Both files record the same lens *model*, but no serial
  number survived. The same model is not the same physical lens.
- **Focus.** The D800 set was manually focused with unverified accuracy; the
  D810 set used autofocus. A focus error combined with field curvature could
  produce a similar soft-center and off-axis pattern.
- **Setup and alignment.** Chart tilt, focus-plane tilt, capture alignment, or
  decentering could also produce upper/lower or center/field differences, and
  the retained captures do not isolate them.

Two direct controls would narrow the most obvious alternatives: refocus and
repeat each aperture so focus accuracy becomes a measured spread rather than an
assumption, and move one lens sample between both bodies so the lens is held
fixed by identity rather than by model label. Controlled target alignment and
orthogonal edge orientations would still be needed to separate setup and field
orientation effects. Until then this supports separate field criteria for these
captures, not a ranking of bodies or lenses.

The advisory reference is a consistency check, not a ground truth. It runs a
rendered-luma and gamma path where this measurement runs sensor-linear green, so
the two are expected to differ in scale; agreement in trend is the useful part.
Where they disagree most — the D810 at f/5.6, **0.2713** here against **0.2400**
advisory — both still place the peak at the same aperture.

## The practical consequence

For these archived measurements, a defensible sharpness criterion must be set
per capture system rather than transferred by camera model. A single limit wide
enough to pass both would have passed a system whose field maximum sat off-axis
through four apertures and whose f/4 center fell below its own f/16 reading.

That is the argument for measuring the field rather than the center — not
because corners matter more, but because the center number was the one that
looked normal.

---

**Detail:** [scientific report](../reports/sfr-mtf.md) ·
[method and formulas](../methods/slanted-edge-sfr.md) ·
[published aggregate](../data/sfr-aperture-summary.csv) ·
[reference implementation](../code/src/sfr.cpp)
