# Delta for Grain Rendering

Convention: [Catch2] = automated offline test; [Manual] = manual verification.

## ADDED Requirements

### R-GR-12 Position scan
`EngineParams` MUST carry a signed `scan` field in sample lengths per second (1.0 = the full source length per second of output), valid range [-1, +1], default 0. `scan` MUST be clamped to [-1, +1] and NaN MUST be treated as 0 (consistent with R-GR-7). The engine MUST maintain a scan offset, in fractions of the source length, equal to `base + scan * elapsedSamples / sampleRate`, where `elapsedSamples` is an integer sample counter of rendered output samples since the last rebase and `base` is the offset at the last rebase. The engine MUST rebase (base := current offset, counter := 0) whenever the applied `scan` value changes, so a changed `scan` affects only the time after the change and the offset is continuous. The offset MUST be evaluated at each grain's spawn sample (inside scheduler segments, not once per block). A spawned grain's start position MUST be `clamp01(fold(position + offset) + positionJitter * draw)`, where `fold` wraps the sum around the source ends (period 1.0 of the source length) and is applied BEFORE the jitter clamp; the jitter draw and its rng consumption MUST be unchanged. When the offset is exactly 0, `fold` MUST NOT alter the anchor (so `position` = 1.0 behaves as today). The offset MUST hold its value when `scan` returns to 0 (it is not pulled back to the anchor). The offset MUST NOT use per-block floating-point accumulation, and MUST be independent of block size and, for a given elapsed time in seconds, of sample rate.

#### Scenario: Zero scan is identical to today [Catch2]
- GIVEN a fixed source, seed and parameters with `scan` = 0 (including `position` = 1.0 and `positionJitter` > 0)
- WHEN 1 s is rendered
- THEN the output is bit-identical to the output of the engine before this change

#### Scenario: Offset advances at the stated rate [Catch2]
- GIVEN a ramp source of N samples, `scan` = +0.5, jitter 0, anchor 0, rate 48 kHz
- WHEN grains spawn at known sample indices
- THEN a grain spawned at output sample t starts at source index (0.5 * t / 48000) * (N - 1) within one source sample

#### Scenario: Negative scan moves backwards [Catch2]
- GIVEN `scan` = -0.25 and anchor 0.5, jitter 0
- WHEN a grain spawns 1 s after reset
- THEN its start position equals fold(0.5 - 0.25) = 0.25 of the source

#### Scenario: Wrap at both ends [Catch2]
- GIVEN `scan` = +1 and anchor 0.75, then `scan` = -1 and anchor 0.25
- WHEN a grain spawns 0.5 s after reset in each case
- THEN the start positions are fold(1.25) = 0.25 and fold(-0.25) = 0.75 (wrapped, not clamped to an end)

#### Scenario: Wrap precedes jitter clamp [Catch2]
- GIVEN an offset-bearing sum that wraps to near 0 and `positionJitter` > 0
- WHEN grains spawn
- THEN every start position is `clamp01(wrapped + jitter)`, so jitter never causes a wrap and positions stay within [0, 1]

#### Scenario: Scan change is continuous [Catch2]
- GIVEN `scan` = 0.5 for 1 s, then `scan` = -0.5
- WHEN grains spawn at 1 s and 1.5 s
- THEN the second start position is 0.25 of a source length behind the first (offset continuous, no jump)

#### Scenario: Offset holds when scan returns to 0 [Catch2]
- GIVEN `scan` = 0.5 for 1 s, then `scan` = 0
- WHEN grains spawn afterwards without Play or load
- THEN their start position stays at fold(anchor + 0.5)

#### Scenario: Scan is clamped and NaN-safe [Catch2]
- GIVEN `scan` in {-5, +5, NaN, +/-infinity}
- WHEN applied and rendered
- THEN the effective scan is -1, +1, 0, and -1/+1 respectively and all output is finite with |y| < 1.0

#### Scenario: Block-split and sample-rate independence [Catch2]
- GIVEN constant non-zero `scan` and the same seed
- WHEN 1 s is rendered as 512-sample blocks, 37-sample blocks and one block, and at 44.1 kHz versus 48 kHz for equal elapsed seconds
- THEN the block splits are bit-identical and the start positions at equal elapsed seconds agree within one sample of difference at the lower rate

## MODIFIED Requirements

### R-GR-2 Grain playback model
A grain MUST read source samples at a constant playback rate r = 2^(semitones/12), starting at its start position, for a duration given in OUTPUT samples (size). Its source span is therefore size*r. Output MUST be window(n/size) times the interpolated source sample (linear interpolation in this change; the interpolation method MUST be replaceable without changing grain logic).
(Previously: the third scenario ended "no scan speed control exists in this change"; requirement text unchanged.)

#### Scenario: Unity pitch reproduces source
- Given a ramp source, r = 1 (0 st), position at sample 1000, size 256, gain normalization disabled or single grain
- When rendered
- Then grain output equals window(n/256)*source[1000+n] within 1e-6

#### Scenario: Octave up doubles consumed span
- Given +12 st, size 256
- When rendered
- Then the grain reads source indices 2n (n = 0..255) within 1e-5

#### Scenario: Pitch is playback rate only
- Given position fixed and `scan` = 0, pitch changed
- When rendering
- Then the start position is unchanged and only the consumed span changes (pitch never moves the start position; start position motion over time comes only from `scan`, R-GR-12)

### R-GR-6 Determinism
With identical source, parameters, seed, sample rate, and block partition, the engine MUST produce bit-identical output across runs. Output MUST also be bit-identical across different block partitions of the same total length (block-split invariance), except where parameter changes are applied per block. This MUST hold for any constant `scan` value, including non-zero values (R-GR-12).
(Previously: no statement about `scan`.)

#### Scenario: Same seed bit-identical
- Given two engine instances, same seed and parameters
- When each renders 2 s
- Then outputs are identical byte-for-byte

#### Scenario: Block-split invariance
- Given constant parameters and the same seed
- When 1 s is rendered as 512-sample blocks, 37-sample blocks, and one block
- Then outputs are bit-identical (or, if the design cannot guarantee that, within 1e-6 absolute)

#### Scenario: Block-split invariance with non-zero scan [Catch2]
- Given constant parameters with `scan` = 0.37 and the same seed
- When 1 s is rendered as 512-sample blocks, 37-sample blocks, and one block
- Then outputs are bit-identical (or within 1e-6 absolute)
