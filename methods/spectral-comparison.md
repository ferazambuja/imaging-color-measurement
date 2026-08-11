# Spectral comparison method

[Study](../studies/spectral-measurement-crosscheck.md) ·
[Report](../reports/spectral-measurement-crosscheck.md) ·
[Header](../code/include/camera_iq/spectral_compare.hpp) ·
[Implementation](../code/src/spectral_compare.cpp) ·
[Tests](../code/tests/test_spectral_compare.cpp)

## Inputs

The numerical core accepts two groups of `SampledSpectrum` values and an
explicit `SpectralComparisonOptions` contract. Every reading within a group
must share one finite, strictly increasing, uniform wavelength axis. The caller
declares the common comparison grid, diagnostic exclusions, and any offset
sweep; the module does not infer them from filenames or units.

`SampledSpectrum` deliberately carries no implied radiometric or colorimetric
unit. This method compares numeric spectral level and shape; it does not turn a
spectrum into XYZ unless a separate, explicit illuminant/observer path is used.

## Data flow

```text
reading groups on native grids
          │
          ├── native equal-weight integral → level statistics
          └── integral-normalized readings → mean shapes
                                      │
                         linear resampling to common grid
                                      │
                         normalization on shared support
                                      │
              directional relative L2 + per-band fractions
                                      │
                    diagnostic exclusion and offset views
```

Means are formed on each native axis before resampling. Both resampled means
are then normalized again on the common support. That order prevents a native
wavelength tail present in only one series from changing the scale of the
comparison.

## Residual and localization

For common-grid reference `r` and candidate `c`:

```text
residual_norm  = hypot over (c_k - r_k)
reference_norm = hypot over r_k
relative_L2    = residual_norm / reference_norm

band_fraction_k = (c_k - r_k)² / Σ_j(c_j - r_j)²
```

The direction is part of the result contract. Per-band fractions are
renormalized to sum to one when the residual is non-zero. Exclusions create a
separate result over retained bands; they do not rewrite the primary result.

## Offset sensitivity

The caller chooses which axis is shifted and the sign convention is serialized
in the result. Every requested offset is evaluated on one interior grid that is
supported for the entire sweep. Both spectra are resampled and normalized for
each offset, then a fresh residual and reference norm are retained.

The zero-offset objective is calculated on that same fixed interior even when
zero is outside the requested sweep range. This prevents comparison of a fitted
minimum with a baseline computed on different wavelength support.

Because the denominator can change across offsets, a ratio between two
directional relative-L2 values is an objective change, not a residual-energy
fraction. A minimum chosen from the same spectra is a sensitivity diagnostic,
not a calibration result.

## Invalid inputs

The module rejects:

- common grids with fewer than two samples, non-finite values, decreasing
  coordinates, or non-uniform steps;
- interpolation outside a retained wavelength range;
- exclusions that do not appear on the common grid;
- non-positive or non-representable normalization and reference norms;
- offset ranges that are invalid, unbounded, or not an integer number of
  declared steps; and
- sweeps with fewer than two wavelengths in their common supported interior.

## What the public tests establish

Synthetic tests pin resample-then-normalize ordering, the directional `1/3`
oracle, a `2/3` localized band fraction, separate diagnostic exclusions, the
offset sign convention, fixed-support zero baseline, and invalid-input cases. These
are library-level numerical checks. They do not validate the archive labels,
instrument performance, or the physical cause of the retained discrepancy.
