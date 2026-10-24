# Delta for Grain Rendering

## MODIFIED Requirements

### R-GR-1 Window
The grain window MUST be a Hann window served from a precomputed table (no per-sample trigonometry at render time). It MUST be 0 at both endpoints, 1.0 (within 1e-3) at the centre, symmetric, and within [0, 1] everywhere. Between table entries values MUST be interpolated or the table MUST be dense enough that error vs. the analytic Hann is <= 1e-3.

#### Scenario: Window shape
- Given the window table
- When sampled at phase 0, 0.5, 1 and at 1000 random phases
- Then values are 0, ~1, 0, are symmetric about 0.5, and stay within [0, 1] and within 1e-3 of the analytic Hann

#### Scenario: Window range over a dense phase sweep
- Given the window table
- When sampled at no fewer than 100000 phases covering [0, 1] uniformly, including both endpoints and the table-entry boundaries
- Then every value is finite and within [0, 1] with no tolerance outside that interval

### R-GR-7 Numeric safety and extremes
For every valid or out-of-range parameter combination, output samples MUST be finite (no NaN, no Inf) and bounded (|y| <= a documented ceiling, default 4.0 for inputs in [-1, 1]). Parameters MUST be clamped to documented ranges, NaN inputs treated as defaults. Ranges (amended, user decision D2): density [0, 1000] gr/s (0 = no spawns), size [1 ms, 1000 ms] with a minimum grain length of 2 output samples, pitch [-24, +24] st, position [0, 1] of source length, position jitter [0, 1], pitch jitter [0, 12] st, pan spread [0, 1], gain [0, 2] linear. Denormals MUST NOT be produced in output when grains decay or the source is silent. Grain length in output samples MUST be derived from the engine sample rate only, and MUST NOT depend on the source's own sample rate. Numeric-safety checks MUST hold on both supported toolchains (MSVC and clang), including under UndefinedBehaviorSanitizer with float-cast-overflow checking enabled for the core and tests.

#### Scenario: Extremes grid
- Given size in {0, 1 sample, 2000 ms, 1e9 ms, NaN}, pitch in {-100, -24, 24, 100, NaN}, density in {0, 2000, 1e9}, position in {-1, 0, 1, 2}
- When at least 0.25 s is rendered for every combination of the grid
- Then every output sample is finite and within the ceiling, with no out-of-bounds access

#### Scenario: Extremes worst-case subset
- Given a subset of about 4 worst-case combinations from the grid, each with a saturated pool (for example density 1e9 or 2000 with size 2000 ms or 1e9 ms, pitch at +/-100, position at the source edges)
- When 1 s is rendered for each combination
- Then every output sample is finite and within the ceiling, with no out-of-bounds access, so that release and reacquire churn while the pool is full is exercised

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

### R-GR-8 Real-time safety
After preparation (sample set, pool and tables built), a render call MUST perform zero heap allocations and MUST NOT lock, throw, or block, including when the pool is full and when parameters are at extremes. All storage used by rendering MUST be pre-sized. The real-time extremes test MUST also prove that the engine actually produces grains while parameters are extreme, so that "zero allocations" cannot pass vacuously.

#### Scenario: No allocation in render
- Given a test binary that counts global operator new/delete calls
- When the counter is reset after preparation and then many blocks are rendered (normal, pool-full, extreme parameters, varying block sizes up to 4096)
- Then the allocation count is 0

#### Scenario: Pool full still allocation-free
- Given density at max and long grains so the pool saturates
- When rendered for 2 s
- Then allocation count is 0 and output is finite

#### Scenario: Extremes exercise real grain activity
- Given the real-time extremes test with a non-silent source and density greater than 0
- When it renders block by block
- Then the maximum active-grain count observed across blocks is greater than 0, the output is audibly non-zero (peak or RMS above a small positive threshold), and the allocation count is 0

#### Scenario: Density zero plays nothing
- Given the same real-time test setup with density = 0
- When it renders block by block
- Then the active-grain count is 0 in every block, the output is exactly 0.0, and the allocation count is 0
