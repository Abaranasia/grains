# Transport Gate Specification

## Purpose

A JUCE-free `TransportGate` in `grains_core` that turns the engine's ungated stream into a Play/Stop stream: a linear ~20 ms fade, exact silence while stopped, and a deterministic restart on every Play. The plugin glue (atomic flag, buttons) is specified in `plugin-shell`; this capability covers the testable core behaviour.

Test-layer legend: **[Catch2]** = automated. **[Manual]** = manual verification.

Naming note: the gate states are exactly Stopped, Starting, Playing and Stopping (Starting = the linear fade-in ramp, Stopping = the linear fade-out ramp). This matches the design; the names FadingIn and FadingOut are not used.

Cross-references checked against the main `grain-rendering` spec: R-GR-6 (determinism and block-split invariance), R-GR-8 (real-time safety, zero allocation in render), R-GR-10 (soft output limiting), R-GR-11 (read-only grain introspection, snapshot is not altered by the gate: while Stopped the pool is empty so the snapshot reports `count == 0`).

## Requirements

### R-TG-1 States and ramp shape
The gate MUST have the states Stopped, Starting, Playing and Stopping, and a default of Stopped after construction and after `prepare`. The engine/test API MAY additionally offer an explicit initial transport of Playing (steady gain 1.0, no fade-in) so that existing sound-expecting tests run unramped; this MUST NOT change the default, and a plugin always starts Stopped. The ramp MUST be linear in gain and its duration MUST be a single named core constant (~20 ms, 0.020 s). The ramp length in samples MUST be `max(1, round(0.020 * sampleRate))`. The gain sequence of a fade-in (Starting) MUST start at exactly 0.0 and reach exactly 1.0, and a fade-out (Stopping) MUST start from the current gain and reach exactly 0.0; no step between consecutive samples MAY exceed `1 / rampSamples` (a Stop during Starting reverses from the current gain, never jumping). A Play during Stopping does NOT reverse (see R-TG-4).

#### Scenario: Fade-in endpoints [Catch2]
- Given a prepared gate in Stopped
- When Play is requested and gains are generated for `rampSamples + 1` samples
- Then the first gain is exactly 0.0, the last is exactly 1.0, and every step is at most `1 / rampSamples`

#### Scenario: Fade-out endpoints [Catch2]
- Given a gate in Playing
- When Stop is requested and gains are generated until the gate is Stopped
- Then the gains decrease monotonically from 1.0 to exactly 0.0 with steps at most `1 / rampSamples`

#### Scenario: Stop mid fade-in reverses without a jump [Catch2]
- Given a gate halfway through Starting
- When Stop is requested
- Then the next gain differs from the previous by at most `1 / rampSamples` and the Stopping ramp starts from the current gain

#### Scenario: Explicit initial Playing skips the fade-in [Catch2]
- Given an engine prepared with the explicit initial transport Playing
- When it renders
- Then the gain is exactly 1.0 from the first sample (output bit-identical to the ungated engine) and no fade-in occurs

#### Scenario: Prepare defaults to Stopped [Catch2]
- Given an engine prepared without an explicit initial transport
- When it renders before any Play
- Then the output is exact zeros

#### Scenario: Default state [Catch2]
- Given a freshly constructed or just prepared gate
- When its state is read
- Then it is Stopped and its gain is exactly 0.0

### R-TG-2 Steady playing is bit-identical
While the gate is Playing (gain exactly 1.0 and no transition pending) the gate MUST NOT modify the signal: the engine output MUST be bit-identical to the output of the engine without a gate for the same seed, parameters and source.

#### Scenario: Playing equals ungated output [Catch2]
- Given two engines with the same seed, source and parameters, one with the gate applied after the ramp has completed
- When both render the same blocks
- Then every output sample is bitwise identical

### R-TG-3 Exact silence after Stop
After a fade-out (Stopping) completes the engine output MUST be exact zeros (+0.0, no NaN, no denormals) for every sample until the next Play. On completing the fade-out the gate MUST trigger a reset of the grain pool and the scheduler exactly once (the source stays loaded).

#### Scenario: Zeros after fade-out [Catch2]
- Given a playing engine
- When Stop is requested and blocks are rendered past the ramp length
- Then all samples after the ramp end are exactly 0.0 and the active grain count is 0

#### Scenario: Reset happens once [Catch2]
- Given a gate completing a fade-out
- When blocks continue to be rendered while stopped
- Then the pool and scheduler reset hook is invoked exactly once for that Stop

### R-TG-4 Deterministic restart
Every transition to Starting from Stopped MUST reseed the engine rng with the engine's configured seed and start from a fresh pool and scheduler, so that two Plays with the same source and parameters produce bit-identical output sequences, each beginning with gain 0.0. The scan offset of `grain-rendering` R-GR-12 (offset, base and elapsed-sample counter) MUST also be reset to 0 on every such transition, so a Play always restarts reading from the `position` anchor; the same reset MUST occur on every new source load (hard stop on source swap, R-TG-10) and on `prepare`. A Play while already Playing or Starting MUST be a no-op (no reseed, no reset, no scan offset reset). A Play requested during Stopping MUST NOT reverse the ramp: the fade-out MUST run to completion (exact zeros, pool and scheduler reset as in R-TG-3) and the pending Play MUST then restart from fresh exactly like a Play from Stopped (rng reseed, pool and scheduler reset, scan offset reset, fade-in from 0.0). Consequently every Play that starts a fade-in, whether requested from Stopped or latched during Stopping, produces the identical output sequence. A Stop requested during Starting is unaffected (it reverses from the current gain, R-TG-1).

#### Scenario: Two Plays identical [Catch2]
- Given an engine that plays, is stopped to exact zeros, and is played again, with unchanged parameters and source
- When the first N samples after each Play are compared
- Then they are bitwise identical

#### Scenario: Play during fade-out finishes the fade, then restarts fresh [Catch2]
- Given an engine in the middle of Stopping
- When Play is requested
- Then the gain keeps decreasing to exactly 0.0 (no reversal, no jump), and afterwards a fade-in starts whose output equals, bitwise, the first Play's output for the same source and parameters

#### Scenario: Redundant Play is a no-op [Catch2]
- Given a gate in Playing
- When Play is requested again
- Then the output is bit-identical to the run without the extra request and no rng reseed occurs

#### Scenario: Play resets the scan offset [Catch2]
- Given `scan` = 0.5 played for 2 s, then Stop to exact zeros, then Play with unchanged parameters and source
- When the first N samples after each Play are compared
- Then they are bitwise identical and the first grain after the second Play starts at the `position` anchor

#### Scenario: Redundant Play keeps the scan offset [Catch2]
- Given `scan` = 0.5 Playing for 2 s
- When Play is requested again
- Then the output is bit-identical to the run without the extra request (offset not reset)

#### Scenario: New load resets the scan offset [Catch2]
- Given `scan` = 0.5 Playing for 2 s, then a new source is set and Play requested
- When the output is compared with a fresh engine playing the new source with the same seed and parameters
- Then the outputs are bitwise identical

### R-TG-5 No random draws while stopped
While Stopped the engine MUST NOT schedule grains, render grains, or draw from the rng. Stopped blocks MUST cost no grain rendering work.

#### Scenario: rng untouched while stopped [Catch2]
- Given a stopped engine and a recorded rng state
- When many blocks are processed
- Then the rng state is unchanged and the output is exact zeros

### R-TG-6 Bounded output during ramps
The gate MUST be applied to the grain sum before the soft limiter (`grain-rendering` R-GR-10), so |y| < 1.0 MUST hold for every sample during Starting, Stopping, and steady states, and NaN/Inf and denormals MUST still be flushed exactly as without the gate.

#### Scenario: Bounded through ramps [Catch2]
- Given extreme parameters (maximum density and overlap) with a loud source
- When Play and Stop are exercised repeatedly
- Then every output sample satisfies |y| < 1.0 and is finite

### R-TG-7 Block-split invariance across a toggle
Toggles MUST be block-quantised: a requested state change MUST take effect at the start of the next processed block. With the toggle applied at the same sample index in both runs, rendering the same total samples in different block splits MUST produce bit-identical output, preserving `grain-rendering` R-GR-6.

#### Scenario: Split invariance with Play and Stop [Catch2]
- Given the same engine configuration and a Play at sample 0 and a Stop at sample S in both runs
- When the output is rendered once as 1 block pattern A and once as pattern B with block boundaries at S
- Then the outputs are bitwise identical

### R-TG-8 Sample-rate recompute and reset on prepare
`prepare(sampleRate)` MUST recompute the ramp length in samples for the new rate, reset the gate to Stopped (gain 0.0), and clear any pending request. A ramp that is in progress when `prepare` runs MUST be discarded.

#### Scenario: Ramp length follows rate [Catch2]
- Given prepare at 44100 Hz and then at 96000 Hz
- When the ramp length is read
- Then it equals `round(0.020 * rate)` for each rate (within the named constant)

#### Scenario: Prepare stops [Catch2]
- Given a gate in Playing
- When `prepare` is called
- Then the gate is Stopped with gain 0.0

### R-TG-9 Real-time safety
Gate operations (request, per-block advance, per-sample gain, reset hooks, hard stop) MUST NOT allocate, lock, block, perform I/O, or throw, and MUST be `noexcept`, consistent with `grain-rendering` R-GR-8.

#### Scenario: Allocation-free gate path [Catch2]
- Given the realtime test harness with allocation counting
- When blocks are processed through Play, Stop and Play transitions
- Then zero allocations are recorded

### R-TG-10 Hard stop on source swap
The engine MUST offer a hard stop that, without any fade, puts the transport in Stopped (gain 0.0, target Stopped) and resets the grain pool and scheduler; the loaded source is unchanged. It is used when the audio thread adopts a changed source, so the new sample never sounds before an explicit Play. `setSource` itself MUST NOT change the transport state.

#### Scenario: Hard stop is immediate [Catch2]
- Given an engine Playing
- When the hard stop is invoked and the next block is rendered
- Then the whole block is exact zeros, the active grain count is 0, and the transport is Stopped

#### Scenario: setSource leaves transport alone [Catch2]
- Given a Playing engine
- When `setSource` is called with a new source
- Then the transport state is unchanged

## Out of scope
Pause/resume, natural tail, automatable or persisted Play state, following the host transport, and MIDI gating.
