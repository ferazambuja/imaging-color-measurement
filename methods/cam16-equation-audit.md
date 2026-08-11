# CAM16 equation-audit method

[Study](../studies/color-model-equation-audit.md) ·
[Report](../reports/cam16-equation-audit.md) ·
[Header](../code/include/camera_iq/cam16_equation_audit.hpp) ·
[Implementation](../code/src/cam16_equation_audit.cpp) ·
[Tests](../code/tests/test_cam16_equation_audit.cpp) ·
[Browser calculator](https://ferazambuja.github.io/imaging/cam16-hellwig-comparator/) ·
[Standalone forward-model comparator](https://github.com/ferazambuja/cam16-hellwig-comparator)

## Scope

This module evaluates selected scalar relations and one bounded coupled
expression. It does not accept XYZ, perform chromatic adaptation, calculate
adapted cone responses, or return a complete CAM16 appearance specification.
That narrow API prevents an equation audit from being mistaken for a full
model implementation.

The standalone Python comparator supplies six-correlate forward `J, Q, C, M,
s, h` paths for standard CAM16 and the Hellwig–Fairchild 2022 proposal. It does
not widen what this C++ study establishes: this module remains an isolation
experiment whose inputs are already-declared correlates and background values.

## Calculation flow

```text
declared J and relative background values
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

The report builder evaluates 21 brightness points (`J = 0…100` in steps of
five), eight backgrounds, and nine positive reference-lightness values for
each background. This produces 72 coupled points.

## Operating conditions

The isolated factor is defined only for finite relative backgrounds in
`(0,100]`. The coupled expression additionally requires reference lightness in
`(0,100]`; zero is rejected because it appears as the base of a generally
non-zero exponent. Normalized brightness accepts finite `J` in `[0,100]`.

The coupled sweep holds adapted responses fixed. Its result applies to that
declared isolation of the background-dependent terms, not to arbitrary XYZ
stimuli or viewing conditions.

## Corrected colorfulness relation

The tested relation is:

```text
M = 43 N_c e_t hypot(a, b)
```

`N_c` and `e_t` must be finite and non-negative; the opponent coordinates must
be finite. `hypot` avoids the unnecessary intermediate overflow risk of
forming `a² + b²` directly. A non-finite result is rejected.

## What the tests establish

The synthetic test executable checks:

- the different brightness midpoints at `J = 25` and `50`;
- exact isolated factors at four declared backgrounds;
- the two endpoints of the `Y_background = 0.1`, `J = 10…90` coupled range;
- the fact that `2.595×` lies inside, rather than bounds, that range;
- the coefficient `43` using a 3-4-5 opponent vector;
- 21 brightness, 8 isolated, and 72 coupled report points;
- all six source-paper performance values; and
- rejection of invalid domains and non-finite inputs.

The tests establish the implementation's numerical contract. They do not
validate the perceptual model or reproduce the paper's observer analysis.
