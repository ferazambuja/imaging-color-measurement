# Computing the CAM16 brightness and background comparison

[Study](../studies/color-model-equation-audit.md) ·
[Report](../reports/cam16-equation-audit.md) ·
[Header](../code/include/camera_iq/cam16_equation_audit.hpp) ·
[Implementation](../code/src/cam16_equation_audit.cpp) ·
[Tests](../code/tests/test_cam16_equation_audit.cpp) ·
[Browser calculator](https://ferazambuja.github.io/imaging/cam16-hellwig-comparator/) ·
[Standalone forward-model comparator](https://github.com/ferazambuja/cam16-hellwig-comparator)

## Scope

This module evaluates normalized brightness, the isolated `N_cb^0.9` term, one
bounded coupled background expression, and the corrected colorfulness
coefficient. Its inputs are already-computed correlates and relative background
values; it does not perform chromatic adaptation or return a full CAM16
appearance specification.

The standalone Python comparator supplies six-correlate forward `J, Q, C, M,
s, h` paths for standard CAM16 and the Hellwig–Fairchild 2022 proposal when a
full XYZ-to-appearance calculation is needed.

## Calculation flow

```text
input J and relative background values
              │
              ├── normalized brightness: sqrt(J/100) and J/100
              │
              ├── isolated background factor: (Yb_ref/Yb)^0.18
              │
              └── coupled background expression
                    ├── n-dependent base
                    ├── z(n) = 1.48 + sqrt(n)
                    └── reference-lightness exponent
```

The calculation evaluates 21 brightness points (`J = 0…100` in steps of five),
eight backgrounds, and nine positive reference-lightness values for each
background. This produces 72 coupled points.

## Operating conditions

The isolated factor is defined only for finite relative backgrounds in
`(0,100]`. The coupled expression additionally requires reference lightness in
`(0,100]`; zero is rejected because it appears as the base of a generally
non-zero exponent. Normalized brightness accepts finite `J` in `[0,100]`.

The coupled sweep holds adapted responses fixed. Its result applies to this
controlled isolation of the background-dependent terms, not to arbitrary XYZ
stimuli or viewing conditions.

## Corrected colorfulness relation

The corrected relation is:

```text
M = 43 N_c e_t hypot(a, b)
```

`N_c` and `e_t` must be finite and non-negative; the opponent coordinates must
be finite. `hypot` avoids the unnecessary intermediate overflow risk of
forming `a² + b²` directly. A non-finite result is rejected.

## Numerical checks

The checks cover:

- the different brightness midpoints at `J = 25` and `50`;
- exact isolated factors at four background values;
- the two endpoints of the `Y_background = 0.1`, `J = 10…90` coupled range;
- the fact that `2.595×` lies inside, rather than bounds, that range;
- the coefficient `43` using a 3-4-5 opponent vector;
- 21 brightness, 8 isolated, and 72 coupled report points;
- all six source-paper performance values; and
- rejection of invalid domains and non-finite inputs.

These checks cover the numerical implementation. Perceptual validation requires
observer data and is outside this calculation.
