# Parameter Mapping Specification (NEW capability)

## Purpose

A JUCE-free parameter layer in `grains_core` that is the single source of truth for the 9 user-facing parameters: stable ID, display name, range, default, skew, and conversion to `EngineParams`. All requirements here are verifiable with Catch2 offline, with no JUCE source, module, or header present.

Test-layer legend: **[Catch2]** = automated, offline, strict TDD.

## Requirements

### R-PM-1 Parameter spec table
The layer MUST expose a constant spec table with exactly 9 entries, one per `EngineParams` field, in this order and with these stable string IDs: `grainSize`, `density`, `position`, `positionJitter`, `pitch`, `pitchJitter`, `timingJitter`, `panSpread`, `gain`. Each entry MUST carry: ID, display name, plain-value minimum, maximum, default, and a skew factor (> 0, where 1 means linear). Minimum, maximum, and default MUST be derived from `grains::limits` constants and the `EngineParams` field defaults, never duplicated as independent literals. Pitch range MUST be [-kPitchSemitonesMax, +kPitchSemitonesMax]; density MUST be [0, kDensityMax]. IDs MUST be unique, non-empty, and MUST NOT change once released (they are the host automation identity).

#### Scenario: Table size and IDs [Catch2]
- Given the spec table
- When its entries are enumerated
- Then there are exactly 9, with the IDs above in that order and no duplicates

#### Scenario: Ranges come from limits [Catch2]
- Given each entry
- When min/max/default are compared against `grains::limits` and a default-constructed `EngineParams`
- Then they match, and for every entry min < max and min <= default <= max

#### Scenario: Skew is valid [Catch2]
- Given each entry
- When its skew factor is read
- Then it is finite and greater than 0

### R-PM-2 Normalised/plain conversion
The layer MUST convert between plain values and normalised [0, 1] values per entry, honouring the entry's range and skew. Conversion MUST be monotonically increasing, MUST map min to 0 and max to 1, and plain -> normalised -> plain MUST round-trip within 1e-4 relative to the range span. Out-of-range plain inputs MUST clamp to [min, max]; normalised inputs outside [0, 1] MUST clamp to [0, 1]; NaN MUST be treated as the entry default and MUST NOT propagate.

#### Scenario: Endpoints [Catch2]
- Given any entry
- When plain min, plain max are normalised
- Then the results are exactly 0 and 1, and denormalising 0 and 1 gives min and max

#### Scenario: Round trip [Catch2]
- Given any entry and a sweep of at least 101 normalised values in [0, 1]
- When each is denormalised then re-normalised
- Then the result equals the input within 1e-4

#### Scenario: Monotonic [Catch2]
- Given any entry and an increasing sweep of normalised values
- When denormalised
- Then the plain values are non-decreasing

#### Scenario: Out-of-range and NaN [Catch2]
- Given inputs below min, above max, normalised -0.5 and 1.5, and NaN
- When converted
- Then results are clamped to range, NaN yields the entry default, and every result is finite

### R-PM-3 Mapping to EngineParams
The layer MUST provide a pure function that maps the 9 plain parameter values (indexed by the table order or by ID) to an `EngineParams`: `grainSize` -> `grainSizeMs`, `density` -> `density`, `position` -> `position`, `positionJitter` -> `positionJitter`, `pitch` -> `pitchSemitones`, `pitchJitter` -> `pitchJitterSemitones`, `timingJitter` -> `timingJitter`, `panSpread` -> `panSpread`, `gain` -> `gain`. The result MUST be free of non-finite values and MUST already lie within the ranges enforced by `EngineParams::clamped()` (mapping a value then clamping MUST be a no-op). The function MUST be `noexcept` and MUST NOT allocate.

#### Scenario: Defaults map to default EngineParams [Catch2]
- Given every parameter at its table default
- When mapped
- Then the result equals a default-constructed `EngineParams` field by field

#### Scenario: Field routing [Catch2]
- Given nine distinct in-range plain values
- When mapped
- Then each `EngineParams` field holds the value of its corresponding ID and no two fields are swapped

#### Scenario: Extremes are clamp-stable [Catch2]
- Given min, max, out-of-range, infinite, and NaN plain values for every parameter
- When mapped
- Then every field is finite and `mapped == mapped.clamped()`

### R-PM-4 JUCE independence
The parameter layer headers/sources MUST live under `Source/core/` (part of `grains_core`), MUST be C++17, and MUST NOT include or reference any JUCE header. The existing `grains_assert_juce_free` guard MUST continue to pass for `grains_core`.

#### Scenario: Core stays JUCE-free [Catch2 / CMake configure]
- Given the default configuration (`GRAINS_BUILD_PLUGIN=OFF`)
- When configured, built, and `ctest` is run
- Then the parameter tests compile and pass and the JUCE-free assertion still holds

## Assumptions
- Exact skew values and display names are design decisions; this spec only constrains validity (R-PM-1) and behaviour (R-PM-2).
- Per-parameter units/label strings are not required by this spec.
