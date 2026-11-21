# Plugin Shell Specification (delta)

## MODIFIED Requirements

### R-PS-4 Processing and output
`prepareToPlay` MUST prepare the engine with the host sample rate (`GranularEngine::prepare(sampleRate, seed)` takes no block size); the maximum block size MUST be used only to size any scratch buffers. `processBlock` MUST, per block, acquire any newly published source from `SourceExchange` and call `GranularEngine::setSource` only when the source changed, read parameter values, map them via `parameter-mapping` to `EngineParams`, apply them to the engine, and render. While no sample is loaded (or the last restore reported a missing file), the output MUST be silence (exact zeros). Once a sample is loaded, the engine MUST render grains continuously (always-on drone; there is no MIDI gating), and the output amplitude MUST be bounded by the engine's soft output limiter (`grain-rendering` R-GR-10), so that |y| < 1.0 (0 dBFS) at all times; the plugin applies no additional limiter or gain stage. The engine MUST NOT produce NaN/Inf, and no clicks, glitches, or denormal-related CPU spikes MAY occur, except a possible brief dropout when the engine pool resets on a source swap. The plugin MUST accept a stereo output bus; input bus handling MAY be disabled.

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

### Out of scope (explicit)
MIDI note handling and gating, custom parameter UI, waveform display, recent files, relative-path lookup, embedded audio in state, load-time resampling, AU, presets, lookahead limiting and any user-adjustable ceiling or drive, plugin CI job, FetchContent JUCE fallback.

### Assumptions (pending user confirmation)
Placeholder metadata (company "Grains", 4-char codes); output is bounded to |y| < 1.0 by a fixed soft limiter in the engine with no user control, and the drone is always on once a sample is loaded; absolute sample path stored in state; JUCE licensing (AGPLv3 vs commercial) is the user's distribution decision.
