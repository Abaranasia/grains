# Plugin Shell Specification (delta)

## ADDED Requirements

### R-PS-8 Host rate wiring and reload on rate change
`prepareToPlay` MUST record the host sample rate atomically so the loader can use it as the target rate for subsequent requests. When a source is loaded (or a restore reference exists) and the recorded rate differs from the new rate, `prepareToPlay` MUST request one reload through the loader and the existing generation gate, and MUST NOT block, decode, or resample on the calling thread. When `prepareToPlay` runs with the same rate as before, it MUST NOT request a reload. The first `prepareToPlay` after a restore that was published at the file rate MUST reload only if the file rate differs from the host rate. The reload MUST NOT allocate or free sample buffers on the audio thread, `processBlock` MUST be unchanged, and the consumer-side contract of R-SS-1 to R-SS-3 MUST be unchanged. A stale rate from a reload superseded by a newer `prepareToPlay` MUST NOT be published.

#### Scenario: Rate change reloads once [Manual]
- Given a 96 kHz file playing in a host at 44.1 kHz
- When the host changes to 48 kHz and `prepareToPlay` runs
- Then exactly one reload runs, the published rate becomes 48000 Hz, and there is no dropout beyond the brief pool reset of R-PS-4

#### Scenario: Same rate does not reload [Manual]
- Given a source loaded at host rate 48 kHz
- When `prepareToPlay` is called again at 48 kHz (for example on a block-size change)
- Then no load is requested and the source is not re-published

#### Scenario: Rapid rate changes publish only the last [Manual]
- Given `prepareToPlay` called at 44.1 kHz and then at 96 kHz before the first reload finishes
- When both requests finish
- Then only the 96 kHz result is published

#### Scenario: No source and no state [Manual]
- Given no sample loaded and none restored
- When `prepareToPlay` runs at any rate
- Then no load is requested and the output is silent

#### Scenario: Audio thread stays allocation-free [Manual]
- Given a reload in flight
- When blocks are processed
- Then `processBlock` performs no allocation, lock, I/O, or buffer free and audio continues from the current source

## Out of scope (delta)
Removed from the out-of-scope list: "load-time resampling". Added: runtime (pitch-shift) aliasing fixes, interpolator or oversampling changes in `processBlock`, and removal of the engine's source-rate compensation.
