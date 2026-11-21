# Delta for plugin-shell

Delivery tag: **PR2** = plugin PR (the core pieces it relies on land in PR1). No new parameters: R-PS-3 stays at exactly 9 parameters and is unchanged.

## MODIFIED Requirements

### R-PS-4 Processing and output [PR2]
`prepareToPlay` MUST prepare the engine with the host sample rate (`GranularEngine::prepare(sampleRate, seed)` takes no block size); the maximum block size MUST be used only to size any scratch buffers. `prepareToPlay` MUST also publish an empty grain snapshot (`count == 0`) so no stale display data survives a re-prepare. `processBlock` MUST, per block, acquire any newly published source from `SourceExchange` and call `GranularEngine::setSource` only when the source changed, read parameter values, map them via `parameter-mapping` to `EngineParams`, apply them to the engine, render, and then, as the final step on every render path (mono source and stereo source, including any early return), take a grain snapshot and publish it to the display mailbox (`waveform-display` R-WD-6). While no sample is loaded (or the last restore reported a missing file), the output MUST be silence (exact zeros). Once a sample is loaded, the engine MUST render grains continuously (always-on drone; there is no MIDI gating), and the output amplitude MUST be bounded by the engine's soft output limiter (`grain-rendering` R-GR-10), so that |y| < 1.0 (0 dBFS) at all times; the plugin applies no additional limiter or gain stage. The publish step MUST NOT change the output. The engine MUST NOT produce NaN/Inf, and no clicks, glitches, or denormal-related CPU spikes MAY occur, except a possible brief dropout when the engine pool resets on a source swap. The plugin MUST accept a stereo output bus; input bus handling MAY be disabled.

#### Scenario: Silence without a sample [Manual]
- Given the Standalone running with default parameters and no sample loaded
- When audio runs for at least 30 seconds
- Then the output is silent with no glitches, and CPU stays idle-level

#### Scenario: Sound after load [Manual]
- Given the Standalone running with no sample
- When a valid audio file is loaded
- Then audible grain output starts without any MIDI input and peak output magnitude does not exceed 1

#### Scenario: Source set only on change [Manual]
- Given a source is loaded and playing
- When many blocks are processed with no new publish
- Then `setSource` is not called again and rendering is uninterrupted

#### Scenario: Parameter moves while silent [Manual]
- Given the Standalone running
- When all 9 parameters are swept through their full ranges
- Then there is no crash, hang, or audible artifact

#### Scenario: Prepare at multiple rates and block sizes [Manual]
- Given the Standalone or host configured at 44.1 kHz, 48 kHz, and 96 kHz and different block sizes
- When the plugin is prepared and run
- Then it starts without error and outputs silence when no sample is loaded

#### Scenario: Publish runs on mono and stereo paths [Manual]
- Given a mono sample and a stereo sample loaded in turn
- When blocks are processed
- Then a snapshot is published after every block on both paths and the audio output is unchanged by the publish step

#### Scenario: Empty snapshot after prepare [Manual]
- Given grains are displayed and the host re-prepares the plugin
- When the editor next polls
- Then it receives an empty snapshot and shows no stale markers

### R-PS-5 Real-time safety [PR2]
`processBlock` MUST NOT allocate, lock, block, perform I/O, or throw. Parameter reads MUST use lock-free atomic access (APVTS raw parameter values). Any allocation MUST happen in the constructor or `prepareToPlay`. `ScopedNoDenormals` (or equivalent) MUST be applied in `processBlock`. The grain snapshot and publish step in `processBlock` MUST be lock-free and allocation-free: the snapshot is built in pre-sized storage and handed to the wait-free mailbox (`waveform-display` R-WD-6), with no lock, wait, allocation, or throw.

#### Scenario: Code review for RT rules [Manual]
- Given the processor source
- When `processBlock` and the functions it calls (including the snapshot and publish step) are reviewed
- Then no allocation, lock, I/O, or exception site is present and `ScopedNoDenormals` is used

#### Scenario: Engine/mapper allocation-free [Catch2]
- Given the existing engine allocation-free test and `parameter-mapping` `noexcept` mapping
- When the suite runs
- Then the engine render path and mapping function are covered by automated tests and pass

#### Scenario: Snapshot and publish allocation-free [Catch2]
- Given the test binary that counts global operator new/delete calls
- When the counter is reset after preparation and many snapshot-and-publish cycles run
- Then the allocation count is 0 (core part, PR1; the processor call site is verified by review)
