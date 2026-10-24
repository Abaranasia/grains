# Grain Rendering Specification

## Purpose

Rendering turns active grains into audio from an immutable, preloaded sample (mono or stereo), offline and without JUCE.

## Requirements

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

### R-GR-2 Grain playback model
A grain MUST read source samples at a constant playback rate r = 2^(semitones/12), starting at its start position, for a duration given in OUTPUT samples (size). Its source span is therefore size*r. Output MUST be window(n/size) times the interpolated source sample (linear interpolation in this change; the interpolation method MUST be replaceable without changing grain logic).

#### Scenario: Unity pitch reproduces source
- Given a ramp source, r = 1 (0 st), position at sample 1000, size 256, gain normalization disabled or single grain
- When rendered
- Then grain output equals window(n/256)*source[1000+n] within 1e-6

#### Scenario: Octave up doubles consumed span
- Given +12 st, size 256
- When rendered
- Then the grain reads source indices 2n (n = 0..255) within 1e-5

#### Scenario: Pitch is playback rate only
- Given position fixed, pitch changed
- When rendering
- Then the start position is unchanged and only the consumed span changes (no scan speed control exists in this change)

### R-GR-3 Boundary-safe reads
A grain MUST NOT read outside the source and MUST NOT be terminated mid-window. If its span would exceed the available source, the grain length (in output samples) MUST be SHRUNK at spawn so the whole windowed grain fits (user decision D1; the start position is not moved to preserve full length); the window MUST complete over the shrunk length so no truncation click occurs. Only if the shrunk length would be below the 2-sample minimum MAY the start be moved back to fit 2 samples, otherwise the spawn is skipped. For a source shorter than a minimum useful span, the grain MUST still be well defined (length >= 2 samples or skipped) without out-of-bounds access.

#### Scenario: Grain near end of source
- Given (grains shrink near the buffer end, not clamp-start) a 10000-sample constant-1 source, position at 9990, size 4800, +12 st
- When rendered
- Then no read index >= source length occurs, the grain's final output sample magnitude <= sin^2(pi/len) + eps (len = the shrunk grain length in output samples, eps a small numeric tolerance; the window phase is n/len so the last sample is not exactly 0), and max |sample-to-sample difference| of the grain output is <= the max of the window slope plus tolerance (no discontinuity)

#### Scenario: Interpolation at last sample
- Given reads at the final source index and fractional positions between the last two samples
- When interpolating
- Then the result stays within the range of the neighbouring valid samples and no out-of-bounds memory is touched (verifiable under sanitizer or with guarded buffers)

#### Scenario: Tiny source
- Given a 1-sample and a 2-sample source
- When any grain is spawned
- Then output is finite and no out-of-bounds access occurs

### R-GR-4 Stereo, mono, and pan
Mono sources MUST render identically to a stereo source with equal channels. Per-grain pan MUST be constant-power (L^2+R^2 equal to the grain's mono power at every pan position), controlled by a pan-spread parameter in [0, 1] where 0 yields centre for all grains.

#### Scenario: Mono equals dual-mono
- Given the same seed, parameters, and signal as mono and as identical-channel stereo
- When rendered
- Then outputs are bit-identical (or within 1e-6) on both channels

#### Scenario: Zero spread centres
- Given pan spread = 0
- When rendered with a mono source
- Then left and right outputs are equal

### R-GR-5 Overlap gain normalization
Engine output MUST be normalized so that a dense cloud has approximately the same loudness as a sparse one. The normalization gain MUST be derived from parameters (1/sqrt(max(1, density*size)), with size in seconds), fixed per grain at spawn, and not smoothed; the gain parameter is also baked in at spawn. Because gain is constant per grain, there are no zipper steps from active-count changes. The gain MUST be finite when zero grains are active.

#### Scenario: Dense vs sparse loudness
- Given a stationary noise or sine source, same seed, size 50 ms
- When rendering 5 s at density 20 gr/s and at 400 gr/s
- Then output RMS values differ by no more than 3 dB

#### Scenario: Silence with no grains
- Given density 0 or an empty/zero-length source
- When rendered
- Then output is exactly 0.0 and finite

#### Scenario: Density sweep step bound
- Given a constant source and a density sweep (e.g. 1 to 1000 gr/s and back) while rendering
- When rendered
- Then no output sample-to-sample step exceeds the documented bound

### R-GR-6 Determinism
With identical source, parameters, seed, sample rate, and block partition, the engine MUST produce bit-identical output across runs. Output MUST also be bit-identical across different block partitions of the same total length (block-split invariance), except where parameter changes are applied per block.

#### Scenario: Same seed bit-identical
- Given two engine instances, same seed and parameters
- When each renders 2 s
- Then outputs are identical byte-for-byte

#### Scenario: Block-split invariance
- Given constant parameters and the same seed
- When 1 s is rendered as 512-sample blocks, 37-sample blocks, and one block
- Then outputs are bit-identical (or, if the design cannot guarantee that, within 1e-6 absolute)

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

### R-GR-9 Instance isolation
Engine state (RNG, pool, accumulators, smoothing) MUST be per instance with no mutable globals, so two instances with different seeds do not influence each other.

#### Scenario: Interleaved instances
- Given instances A and B with different seeds rendered interleaved block by block
- When compared with A and B each rendered alone
- Then each instance's output is identical to its solo run
