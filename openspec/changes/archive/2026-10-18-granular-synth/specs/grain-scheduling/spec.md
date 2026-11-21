# Delta Spec: grain-scheduling (NEW)

Scheduling decides WHEN grains start (sample-accurate offset within a block) and whether a slot is available. Density is expressed in grains per second.

## Requirements

### R-GS-1 Density and fractional accumulation
Given density D (grains/s) and sample rate fs, the scheduler MUST spawn grains at a mean interval of fs/D samples using a fractional accumulator so that no rounding drift accumulates. With zero jitter, over N samples the number of spawns MUST equal floor(N*D/fs) or that value plus one, and each spawn offset MUST be within 1 sample of its ideal time.

#### Scenario: Periodic spacing, integer period
- Given fs = 48000, D = 100, jitter = 0
- When 48000 samples are scheduled
- Then exactly 100 grains are spawned, spaced 480 samples apart

#### Scenario: Fractional period has no drift
- Given fs = 48000, D = 70 (period 685.714... samples), jitter = 0
- When 10 seconds are scheduled
- Then the spawn count is 700 (+/- 1) and no spawn deviates more than 1 sample from k*fs/D

#### Scenario: Zero density
- Given D = 0
- When any number of blocks is scheduled
- Then no grain is spawned

### R-GS-2 Block-split invariance
The sequence of absolute spawn times (and all per-grain randomized values) MUST be identical regardless of how the same total sample count is divided into blocks, including blocks of 1 sample and blocks of differing sizes.

#### Scenario: Different block partitions
- Given identical seed and parameters
- When 4800 samples are scheduled as one block, as 64-sample blocks, as 1-sample blocks, and as a mixed irregular partition
- Then all four runs yield the same absolute spawn sample positions and the same per-grain random draws

### R-GS-3 In-block offsets
Each spawn MUST carry a start offset in [0, blockSize) relative to the block in which it occurs, so grains begin at their exact sample rather than at block boundaries.

#### Scenario: Offset bounds
- Given any parameters and block size B
- When blocks are scheduled
- Then every reported offset satisfies 0 <= offset < B

### R-GS-4 Jitter (regularity)
A timing jitter parameter in [0, 1] MUST interpolate between periodic (0) and stochastic (1) spawn timing. For any jitter value the long-run mean spawn rate MUST remain D within statistical tolerance, and spawn times MUST be monotonically non-decreasing.

#### Scenario: Jitter preserves mean rate
- Given D = 100, fs = 48000, jitter = 1, 20 seeds
- When 60 seconds are scheduled per seed
- Then the mean spawn count per second across seeds is within 5% of 100

#### Scenario: Jitter = 0 is periodic
- Given jitter = 0
- When two different seeds are used
- Then spawn times are identical

### R-GS-5 Seeded deterministic RNG
All randomness (timing jitter, position jitter, pitch jitter, pan) MUST come from an injectable generator with an explicit seed. Identical seed and parameters MUST yield identical sequences; different seeds SHOULD yield different sequences when jitter > 0. No global or time-based entropy source MAY be used.

#### Scenario: Same seed reproducible
- Given two schedulers seeded with 12345 and equal parameters, jitter = 0.5
- When both run for 1 second
- Then their spawn sequences are identical

#### Scenario: Different seed differs
- Given seeds 1 and 2, jitter = 0.5
- When both run for 1 second
- Then the spawn sequences are not identical

### R-GS-6 Pool capacity and skip-on-full
The grain pool MUST have a fixed capacity of 128 simultaneous grains. When a spawn is due and no slot is free, the new grain MUST be skipped (existing grains are never stolen or truncated). A skipped spawn MUST still consume its scheduled time and its random draws, so the schedule after the skip is unchanged. Slots MUST be reusable as soon as their grain finishes.

#### Scenario: Pool saturation
- Given capacity 128, long grains (size >> 128 periods), high density
- When more than 128 spawns come due
- Then exactly 128 grains are active, the extra spawns are skipped, and active grains are unaffected (their output is unchanged versus an unsaturated run of the same first 128)

#### Scenario: Slot reuse
- Given a full pool
- When one grain completes
- Then the next due spawn is accepted into the freed slot

#### Scenario: Schedule unaffected by skip
- Given two runs with identical seed, one with capacity 128 and one with an effectively unlimited pool
- When both are scheduled
- Then the sequence of due spawn times is identical; only acceptance differs

### R-GS-7 Parameter clamping
Out-of-range density, jitter, and sample-rate-derived values MUST be clamped to documented limits rather than rejected. Density MUST be clamped to [0, 1000] grains/s (amended, user decision D2); a density of 0 MUST mean no spawns, and a positive density below the design minimum (0.1) MUST clamp to that minimum. Jitter MUST be clamped to [0, 1]. NaN input MUST be treated as the parameter's default and MUST NOT propagate.

#### Scenario: Extremes
- Given D = -5, D = 1e9, D = NaN, jitter = -1, jitter = 7
- When scheduled
- Then no crash, no non-finite internal state, spawn rate respects the clamp
