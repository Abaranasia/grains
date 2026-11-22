# Delta for Parameter Mapping

Purpose line (non-requirement text) changes from "the 9 user-facing parameters" to "the 10 user-facing parameters".

## MODIFIED Requirements

### R-PM-1 Parameter spec table
The layer MUST expose a constant spec table with exactly 10 entries, one per `EngineParams` field, in this order and with these stable string IDs: `grainSize`, `density`, `position`, `positionJitter`, `pitch`, `pitchJitter`, `timingJitter`, `panSpread`, `gain`, `scan`. Each entry MUST carry: ID, display name, plain-value minimum, maximum, default, and a skew factor (> 0, where 1 means linear). Minimum, maximum, and default MUST be derived from `grains::limits` constants and the `EngineParams` field defaults, never duplicated as independent literals. Pitch range MUST be [-kPitchSemitonesMax, +kPitchSemitonesMax]; density MUST be [0, kDensityMax]; `scan` MUST be [-kScanMax, +kScanMax] with kScanMax = 1 (sample lengths per second) and default 0. The `scan` entry MUST provide finer resolution near 0 than near the ends, and the 0 default MUST map to the centre of the normalised range (mid position). IDs MUST be unique, non-empty, and MUST NOT change once released (they are the host automation identity).
(Previously: exactly 9 entries, no `scan`.)

#### Scenario: Table size and IDs [Catch2]
- Given the spec table
- When its entries are enumerated
- Then there are exactly 10, with the IDs above in that order (`scan` last) and no duplicates

#### Scenario: Ranges come from limits [Catch2]
- Given each entry
- When min/max/default are compared against `grains::limits` and a default-constructed `EngineParams`
- Then they match, and for every entry min < max and min <= default <= max

#### Scenario: Skew is valid [Catch2]
- Given each entry
- When its skew factor is read
- Then it is finite and greater than 0

#### Scenario: Scan range, default and centre [Catch2]
- Given the `scan` entry
- When its min, max, default and the plain value at normalised 0.5 are read
- Then they are -1, +1, 0 and 0, and the plain value at normalised 0.25 has a magnitude below 0.5 (finer resolution near 0)

### R-PM-3 Mapping to EngineParams
The layer MUST provide a pure function that maps the 10 plain parameter values (indexed by the table order or by ID) to an `EngineParams`: `grainSize` -> `grainSizeMs`, `density` -> `density`, `position` -> `position`, `positionJitter` -> `positionJitter`, `pitch` -> `pitchSemitones`, `pitchJitter` -> `pitchJitterSemitones`, `timingJitter` -> `timingJitter`, `panSpread` -> `panSpread`, `gain` -> `gain`, `scan` -> `scan`. The result MUST be free of non-finite values and MUST already lie within the ranges enforced by `EngineParams::clamped()` (mapping a value then clamping MUST be a no-op). The function MUST be `noexcept` and MUST NOT allocate.
(Previously: 9 parameters, no `scan` routing.)

#### Scenario: Defaults map to default EngineParams [Catch2]
- Given every parameter at its table default
- When mapped
- Then the result equals a default-constructed `EngineParams` field by field (including `scan` = 0)

#### Scenario: Field routing [Catch2]
- Given ten distinct in-range plain values
- When mapped
- Then each `EngineParams` field holds the value of its corresponding ID and no two fields are swapped

#### Scenario: Extremes are clamp-stable [Catch2]
- Given min, max, out-of-range, infinite, and NaN plain values for every parameter (including `scan`)
- When mapped
- Then every field is finite and `mapped == mapped.clamped()`
