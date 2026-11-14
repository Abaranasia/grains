# Grain Rendering Specification (delta)

## MODIFIED Requirements

### R-GR-7 Numeric safety and extremes
For every valid or out-of-range parameter combination, output samples MUST be finite (no NaN, no Inf) and bounded by the documented ceiling C = 1.0: |y| < 1.0 strictly, enforced by the soft output limiter (R-GR-10). Parameters MUST be clamped to documented ranges, NaN inputs treated as defaults. Ranges (amended, user decision D2): density [0, 1000] gr/s (0 = no spawns), size [1 ms, 1000 ms] with a minimum grain length of 2 output samples, pitch [-24, +24] st, position [0, 1] of source length, position jitter [0, 1], pitch jitter [0, 12] st, pan spread [0, 1], gain [0, 2] linear. Denormals MUST NOT be produced in output when grains decay or the source is silent. Grain length in output samples MUST be derived from the engine sample rate only, and MUST NOT depend on the source's own sample rate. Numeric-safety checks MUST hold on both supported toolchains (MSVC and clang), including under UndefinedBehaviorSanitizer with float-cast-overflow checking enabled for the core and tests.

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

#### Scenario: Float-to-index casts are overflow-safe
- Given size = 1e9 ms, position outside [0, 1], and extreme pitch/jitter values, run on the clang sanitizer build with float-cast-overflow enabled
- When rendered
- Then no sanitizer diagnostic is reported (any fix required for this is small, otherwise the check is dropped and logged as a follow-up)

## ADDED Requirements

### R-GR-10 Soft output limiting
The engine MUST pass every output sample, on both channels, through a fixed soft limiter as the last stage of rendering. The limiter MUST be a pure per-sample function of the sample value alone, with no state, no memory between samples, no lookahead, and no latency, so that block-split invariance (R-GR-6) is preserved. It MUST be exposed as a static function so it can be tested directly. It is parameterised by a ceiling C = 1.0 and a knee threshold T, a documented constant with 0.7 <= T <= 0.9 (nominal 0.8). For input x the limiter output y MUST satisfy:

- Ceiling: |y| < C strictly, for every finite x and for non-finite x (see out-of-domain handling below).
- Flush: NaN and any input with |x| < 1e-20 (the flush threshold, which includes every subnormal) MUST map to 0 before the curve is applied.
- Transparency: for x == 0 and for 1e-20 <= |x| <= T, y == x bit-exactly.
- Monotonicity: y is monotone non-decreasing in x.
- Symmetry: y(-x) == -y(x).
- Continuity: y is continuous everywhere, and its slope is at most 1 everywhere.
- Out-of-domain input: NaN MUST map to 0; +inf and -inf MUST map to finite values within (-1, +1) (the sign being preserved); subnormal inputs MUST map to 0 (flushed before the curve is applied, as are all inputs below the flush threshold). Inputs of very large magnitude MUST NOT produce NaN (no inf/inf).

The limiter MUST NOT allocate, lock, throw, or block, and MUST NOT introduce latency. Its result MUST be identical on both supported toolchains (MSVC and clang). The limiter MUST NOT depend on any user parameter; there is no user-adjustable ceiling or drive.

#### Scenario: Worst-case coherent input stays under the ceiling [Catch2]
- Given a constant (DC-like) source of full scale, gain at its maximum, density and size at their maxima, and a saturated pool so grains overlap coherently
- When at least 1 s is rendered
- Then every output sample on both channels is finite with |y| < 1.0
- And the peak magnitude is greater than 0.95, so the limiter is shown to be engaged rather than the signal being silent or attenuated

#### Scenario: Transparent below the knee [Catch2]
- Given inputs x equal to 0, and inputs with 1e-20 <= |x| <= T including +/-1e-20, +/-T, and a dense sweep of values over that range
- When passed through the limiter
- Then each output is bit-identical to its input
- And inputs with 0 < |x| < 1e-20 yield exactly 0

#### Scenario: Curve properties over a sweep [Catch2]
- Given a dense sweep of inputs across [-64, 64] (excluding the flushed region |x| < 1e-20, where the output is 0), including values immediately around +/-T and +/-1
- When passed through the limiter
- Then outputs are monotone non-decreasing, satisfy y(-x) == -y(x), stay strictly within (-1, 1), have no jump between adjacent samples larger than the sweep step (slope <= 1 within numeric tolerance), and approach C as |x| grows

#### Scenario: Out-of-domain inputs [Catch2]
- Given inputs NaN, +inf, -inf, +/-FLT_MAX, and subnormal values of both signs
- When passed through the limiter
- Then NaN yields 0, +inf and -inf yield finite values with |y| < 1 and the matching sign, +/-FLT_MAX yield finite values with |y| < 1, and subnormals yield 0

#### Scenario: Block-split invariance is preserved [Catch2]
- Given constant parameters, the same seed, and a source that drives the output above the knee
- When 1 s is rendered as 512-sample blocks, 37-sample blocks, and one block
- Then the outputs are bit-identical

#### Scenario: Limiter adds no allocation [Catch2]
- Given the test binary that counts global operator new/delete calls
- When the counter is reset after preparation and blocks are rendered with the pool saturated and output above the knee
- Then the allocation count is 0

#### Scenario: Loud material saturates softly in Standalone [Manual]
- Given the Standalone with a loaded sample and parameters set so that grains overlap heavily
- When the output is monitored by ear and on a meter
- Then the output never exceeds 0 dBFS, loud passages saturate smoothly rather than hard-clip, and quiet passages are unchanged in level
