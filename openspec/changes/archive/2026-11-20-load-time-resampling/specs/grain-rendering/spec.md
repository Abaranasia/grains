# Grain Rendering Specification (delta)

## MODIFIED Requirements

### R-GR-7 Numeric safety and extremes
For every valid or out-of-range parameter combination, output samples MUST be finite (no NaN, no Inf) and bounded by the documented ceiling C = 1.0: |y| < 1.0 strictly, enforced by the soft output limiter (R-GR-10). Parameters MUST be clamped to documented ranges, NaN inputs treated as defaults. Ranges (amended, user decision D2): density [0, 1000] gr/s (0 = no spawns), size [1 ms, 1000 ms] with a minimum grain length of 2 output samples, pitch [-24, +24] st, position [0, 1] of source length, position jitter [0, 1], pitch jitter [0, 12] st, pan spread [0, 1], gain [0, 2] linear. Denormals MUST NOT be produced in output when grains decay or the source is silent. Grain length in output samples MUST be derived from the engine sample rate only, and MUST NOT depend on the source's own sample rate. The per-grain read-rate compensation `srcSr / sampleRate_` MUST be kept for sources whose rate differs from the engine rate (for example while a reload is in flight), and MUST equal 1 (no change to read rate) when the source rate equals the engine rate. Numeric-safety checks MUST hold on both supported toolchains (MSVC and clang), including under UndefinedBehaviorSanitizer with float-cast-overflow checking enabled for the core and tests.
(Previously: no statement about the rate compensation being retained or being neutral at equal rates.)

#### Scenario: Extremes grid
- Given size in {0, 1 sample, 2000 ms, 1e9 ms, NaN}, pitch in {-100, -24, 24, 100, NaN}, density in {0, 2000, 1e9}, position in {-1, 0, 1, 2}
- When at least 0.25 s is rendered for every combination of the grid
- Then every output sample is finite and strictly within the ceiling (|y| < 1.0), with no out-of-bounds access [Catch2]

#### Scenario: Extremes worst-case subset
- Given a subset of about 4 worst-case combinations from the grid, each with a saturated pool (for example density 1e9 or 2000 with size 2000 ms or 1e9 ms, pitch at +/-100, position at the source edges)
- When 1 s is rendered for each combination
- Then every output sample is finite and strictly within the ceiling (|y| < 1.0), with no out-of-bounds access, so that release and reacquire churn while the pool is full is exercised [Catch2]

#### Scenario: Clamping equivalence
- Given pitch = +100 and pitch = +24, same seed
- When rendered
- Then outputs are identical

#### Scenario: Engine-level pitch clamp is bit-identical in both directions
- Given engine parameters with all else equal and the same seed
- When rendered with pitch +100 versus +24, and with pitch -100 versus -24, through the engine (not only the parameter clamp helper)
- Then each pair of outputs is bit-identical
- And as a negative control, pitch 24 versus pitch 23 produce outputs that differ

#### Scenario: Sample-rate range
- Given fs in {22050, 44100, 48000, 96000, 192000}
- When rendered
- Then output is finite and grain sizes in ms scale to the correct sample counts

#### Scenario: Grain length scales with engine sample rate
- Given size = 10 ms and fs in {22050, 44100, 48000, 96000, 192000}
- When the grain length in output samples is computed or observed at spawn
- Then it is 221, 441, 480, 960 and 1920 samples respectively
- And the length is the same whether the source was authored at the engine rate or at a different rate

#### Scenario: Mismatched source keeps pitch and duration [Catch2]
- Given a 1 kHz sine source authored at 96000 Hz and an engine at 48000 Hz, pitch 0
- When rendered
- Then the dominant output frequency is 1 kHz within 1 percent, as for a source authored at 48000 Hz

#### Scenario: Matched source rate is neutral [Catch2]
- Given the same material authored at 48000 Hz and an engine at 48000 Hz
- When rendered with a fixed seed
- Then the output is bit-identical to the output before this change

#### Scenario: Float-to-index casts are overflow-safe
- Given size = 1e9 ms, position outside [0, 1], and extreme pitch/jitter values, run on the clang sanitizer build with float-cast-overflow enabled
- When rendered
- Then no sanitizer diagnostic is reported (any fix required for this is small, otherwise the check is dropped and logged as a follow-up)
