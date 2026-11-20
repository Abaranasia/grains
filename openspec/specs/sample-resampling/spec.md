# Sample Resampling Specification

## Purpose

A JUCE-free, throw-free core resampler that converts planar float audio from a source rate to a target rate with a band-limited filter, so the engine can run at a base rate of 1. All requirements are **[Catch2]** unless marked **[Perf]**. Terms: `n` = input frames, `src`/`dst` = source/target rates, `cutoff` = `min(1, dst/src)` of the source Nyquist expressed relative to the lower of the two Nyquist frequencies.

## Requirements

### R-RS-1 Output length
For `n >= 1`, valid rates, the output MUST hold `max(1, round(n * dst / src))` frames per channel, and every channel MUST have the same length.

#### Scenario: Length for common ratios
- Given 48000 frames at 96000 to 48000, 44100 frames at 44100 to 48000, and 1 frame at 96000 to 8000
- When resampled
- Then the output lengths are 24000, 48000 and 1

### R-RS-2 Bit-exact passthrough
When `src == dst` the output MUST be bit-identical to the input, for mono and stereo.

#### Scenario: Equal rates
- Given random stereo input at 48000 to 48000
- When resampled
- Then every output sample is bit-equal to the input and the length is `n`

### R-RS-3 Stopband and passband quality
Frequencies above `1.1 x cutoff` MUST be attenuated by at least 100 dB (this includes aliasing of out-of-band tones when downsampling and spectral images when upsampling). Passband gain MUST be within 0.1 dB of unity (peak-to-peak ripple at most 0.1 dB) for frequencies up to `0.9 x cutoff`.

#### Scenario: Downsample rejects aliasing
- Given full-scale sines at 0.275 x src (26400 Hz = 1.1 x the 24000 Hz cutoff) and above, up to 1.9 x the cutoff, at 96000 to 48000
- When resampled
- Then the output residual is at least 100 dB below the input level (steady-state region)

#### Scenario: Passband ripple
- Given sines from 100 Hz to 0.9 x cutoff at 96000 to 44100 and at 44100 to 96000
- When resampled
- Then every steady-state output amplitude is within 0.1 dB of the input amplitude

#### Scenario: Upsample rejects images
- Given a sine at 10 kHz at 44100 to 96000
- When resampled
- Then no spectral component above 1.1 x 22050 Hz is within 100 dB of the tone

### R-RS-4 Finite output
Finite input MUST yield finite output (no NaN, no Inf), including silence, full-scale and single-frame input.

#### Scenario: Edge inputs finite
- Given all-zero, full-scale square, and 1-frame inputs at 22050 to 192000 and 192000 to 22050
- When resampled
- Then every output sample is finite and no out-of-bounds access occurs

### R-RS-5 Determinism
Identical input and rates MUST produce bit-identical output across runs, and the output MUST NOT depend on how the work is split into abort-check chunks.

#### Scenario: Repeatable
- Given the same stereo input resampled twice, and once with a different internal chunk size
- When the outputs are compared
- Then all are bit-identical

### R-RS-6 Abortable
The resampler MUST poll a caller-supplied abort predicate at least once per 65536 output frames. When the predicate returns true it MUST stop, report an aborted result, and return no partial output as success.

#### Scenario: Abort honored
- Given a 60 s input and a predicate that becomes true on its 3rd poll
- When resampled
- Then the call returns aborted after no more than 3 x 65536 output frames and the result carries no usable buffer

#### Scenario: Never-abort completes
- Given a predicate that is always false
- When resampled
- Then the call completes with the length of R-RS-1

### R-RS-7 Channel handling
One or two channels MUST be accepted; channels MUST be processed independently with identical filtering.

#### Scenario: Identical channels stay identical
- Given a stereo input whose channels are equal
- When resampled
- Then both output channels are bit-identical

### R-RS-8 Rejection
Empty input (zero channels or frames), unequal channel lengths, and a non-finite or non-positive source or target rate MUST be rejected with a distinguishable result, MUST NOT throw, and MUST NOT produce output. A downsampling ratio beyond 1000:1 (`src / dst > 1000`, that is `min(1, dst/src) < 1/1000`) is outside the kernel limit and MUST be rejected with the same kind of result and no output (the resampler reports `InvalidArgument`; the builder reports `BadSampleRate`, see `sample-source` R-SS-5). The resampler MUST depend on no JUCE header.

#### Scenario: Invalid arguments
- Given zero frames, channels of different lengths, rates 0, -1, NaN and Inf
- When resampled
- Then each call is rejected with no output and no exception

#### Scenario: Ratio beyond 1000:1 rejected
- Given a valid 192000 Hz input and target rate 100 Hz (ratio 1920:1)
- When resampled
- Then the call is rejected with `InvalidArgument` and no output

### R-RS-9 Load-time budget
A 300 s stereo 96000 to 48000 conversion MUST complete in at most 3 s in a Release build on the reference machine. This check is tagged `[perf]` and MUST NOT be part of the CI gate.

#### Scenario: Budget [Perf]
- Given 300 s of stereo noise at 96000
- When resampled to 48000 in Release
- Then wall time is at most 3 s
