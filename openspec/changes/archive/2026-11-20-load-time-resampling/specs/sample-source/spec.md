# Sample Source Specification (delta)

## MODIFIED Requirements

### R-SS-5 Sample builder: length cap
The builder MUST cap length at 5 minutes measured at the file's sample rate (`floor(300 * rate)` frames), and the cap MUST be clamped to `INT_MAX` frames. A rate whose `floor(300 * rate)` is 0 (a positive rate below 1/300 Hz) yields a cap of 0 frames and MUST be rejected as `BadSampleRate`, without producing a source. A rate large enough that `300 * rate` overflows the frame-count type (including `DBL_MAX`) MUST yield a cap of exactly `INT_MAX`, with no undefined behaviour. Longer input MUST be truncated to the cap and the result MUST report that truncation occurred. Input at or below the cap MUST be kept in full with no truncation reported. When a target rate is supplied, the builder MUST resample the capped input to that rate (see `sample-resampling`), the published source MUST report the target rate, and the resampled output MUST be capped at `floor(300 * target)` frames (clamped to `INT_MAX`), reporting truncation if the cap bites. When no target rate is supplied, or it equals the file rate, the builder MUST NOT alter the samples.
(Previously: "The builder MUST NOT resample"; no output cap at a target rate.)

#### Scenario: Over-length input truncated [Catch2]
- Given input longer than 300 seconds at its sample rate
- When the builder runs
- Then the result has exactly `floor(300 * rate)` frames and reports truncation

#### Scenario: Input at cap not truncated [Catch2]
- Given input of exactly `floor(300 * rate)` frames
- When the builder runs
- Then all frames are kept and no truncation is reported

#### Scenario: Cap is floored [Catch2]
- Given a sample rate for which `300 * rate` is not an integer (for example 44100.5 Hz)
- When `maxSourceFrames` is evaluated for that rate
- Then it returns `floor(300 * rate)`, not a rounded-up value

#### Scenario: Cap floors to zero is rejected [Catch2]
- Given a finite sample rate greater than 0 for which `floor(300 * rate)` is 0 (for example 0.001 Hz)
- When `maxSourceFrames` is evaluated, and the builder runs on otherwise valid data at that rate
- Then `maxSourceFrames` returns 0, and the builder reports `BadSampleRate` and produces no source

#### Scenario: Cap clamped to INT_MAX [Catch2]
- Given a finite sample rate for which `floor(300 * rate)` exceeds `INT_MAX` (for example 1e9 Hz)
- When `maxSourceFrames` is evaluated
- Then it returns exactly `INT_MAX`

#### Scenario: Extreme rate does not overflow [Catch2]
- Given a sample rate of `DBL_MAX`
- When `maxSourceFrames` is evaluated
- Then it returns exactly `INT_MAX` with no undefined behaviour (clean under UBSan float-cast-overflow)

#### Scenario: Resampled to target rate [Catch2]
- Given 1 s of stereo input at 96000 Hz and target rate 48000
- When the builder runs
- Then the source has 48000 frames, its rate is 48000, and no truncation is reported

#### Scenario: Equal rate is bit-exact [Catch2]
- Given input at 48000 Hz and target 48000
- When the builder runs
- Then the source samples are bit-identical to the input

#### Scenario: Output cap at target rate [Catch2]
- Given a file at 22050 Hz holding `floor(300 * 22050)` frames and target rate 192000
- When the builder runs
- Then the source has exactly `floor(300 * 192000)` frames and truncation is reported only if the resampled length exceeded that cap

#### Scenario: Invalid target rate rejected [Catch2]
- Given a target rate of 0, -1, NaN or Inf
- When the builder runs on otherwise valid data
- Then it is rejected with `BadSampleRate` and no source is produced

## ADDED Requirements

### R-SS-15 Loader target rate and rate-change reload
The loader MUST accept a target rate for each request, captured at request time, and the published `SampleBuffer::sampleRate()` MUST equal that target rate. Resampling MUST run on the loader thread only, MUST check request currency (the generation gate) between chunks, and a superseded request MUST abort without publishing or clearing. A request issued before any target rate is known (a restore before the first `prepareToPlay`) MUST publish the file at its own rate (passthrough). The decode buffer MUST be released as soon as resampling ends. When a reload is requested for a rate change the loader MUST re-decode the file from disk; if the file can no longer be read, the currently published buffer MUST stay active, nothing MUST be published or cleared, and the label MUST NOT read "Missing". The status label rules of R-SS-10 and R-SS-14 are unchanged.

#### Scenario: Published rate equals target [Manual]
- Given a 96 kHz wav and a host at 44.1 kHz
- When the file is loaded
- Then the published source reports 44100 Hz and the file plays at its original pitch and duration

#### Scenario: Restore before prepare publishes at file rate [Manual]
- Given state with a 96 kHz sample restored before the first `prepareToPlay`
- When the restore completes
- Then the source is published at 96000 Hz, plays, and the first `prepareToPlay` at 48000 Hz triggers one reload to 48000 Hz

#### Scenario: Restore before prepare, rates equal [Manual]
- Given a 48 kHz sample restored before the first `prepareToPlay` at 48000 Hz
- When `prepareToPlay` runs
- Then no reload is requested

#### Scenario: Superseded resample aborts [Manual]
- Given a long file being resampled and a newer load request issued
- When the older request next checks currency
- Then it aborts, publishes nothing, clears nothing, and the newer file becomes active

#### Scenario: File gone on rate-change reload [Manual]
- Given a published buffer at 44100 Hz whose file has been deleted
- When the host rate changes to 48000 Hz
- Then the 44100 Hz buffer keeps playing, nothing is cleared, and the label does not read "Missing"

#### Scenario: Peak memory released [Manual]
- Given a 5 minute stereo 192 kHz file loaded at a 48 kHz target
- When the load finishes
- Then the decode buffer has been freed and only the 48 kHz source remains

## Out of scope (delta)
Removed from the out-of-scope list: "load-time resampling". Added: pitch-shift (runtime) aliasing, keeping the original decode in RAM, and a user-adjustable resampler quality.
