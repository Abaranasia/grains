# Plugin Shell Specification (delta)

## MODIFIED Requirements

### Purpose (modified)
An opt-in JUCE plugin (VST3 + Standalone) that hosts `grains_core` with 9 automatable parameters, a sample loader, and an editor embedding the generic parameter editor. Output is silent until a sample is loaded; once loaded the engine renders continuously. JUCE glue is thin; behaviour is verified manually. Any logic that can be tested offline lives in `parameter-mapping` and `sample-source`.

(Previously: "...with 9 automatable parameters and a generic editor. This first shell is deliberately silent: no sample loader exists, so output is silence. JUCE glue is thin; behaviour is verified manually. Any logic that can be tested offline lives in `parameter-mapping`.")

Test-layer legend unchanged.

### R-PS-4 Processing and output
`prepareToPlay` MUST prepare the engine with the host sample rate (`GranularEngine::prepare(sampleRate, seed)` takes no block size); the maximum block size MUST be used only to size any scratch buffers. `processBlock` MUST, per block, acquire any newly published source from `SourceExchange` and call `GranularEngine::setSource` only when the source changed, read parameter values, map them via `parameter-mapping` to `EngineParams`, apply them to the engine, and render. While no sample is loaded (or the last restore reported a missing file), the output MUST be silence (exact zeros). Once a sample is loaded, the engine MUST render grains continuously (always-on drone; there is no MIDI gating), and the output amplitude MUST be bounded by the engine's documented ceiling of ±4 (no limiter is applied). The engine MUST NOT produce NaN/Inf, and no clicks, glitches, or denormal-related CPU spikes MAY occur, except a possible brief dropout when the engine pool resets on a source swap. The plugin MUST accept a stereo output bus; input bus handling MAY be disabled.

(Previously: "...Because no sample is loaded, the output MUST be silence (exact zeros), the engine MUST NOT produce NaN/Inf, and no clicks, glitches, or denormal-related CPU spikes MAY occur." with no acquire/setSource step, no rendering-once-loaded clause, and no ±4 ceiling or always-on statement.)

#### Scenario: Silence without a sample [Manual]
- Given the Standalone running with default parameters and no sample loaded
- When audio runs for at least 30 seconds
- Then the output is silent with no glitches, and CPU stays idle-level

(Unchanged from previous.)

#### Scenario: Sound after load [Manual]
- Given the Standalone running with no sample
- When a valid audio file is loaded
- Then audible grain output starts without any MIDI input and peak output magnitude does not exceed 4

#### Scenario: Source set only on change [Manual]
- Given a source is loaded and playing
- When many blocks are processed with no new publish
- Then `setSource` is not called again and rendering is uninterrupted

#### Scenario: Parameter moves while silent [Manual]
- Given the Standalone running
- When all 9 parameters are swept through their full ranges
- Then there is no crash, hang, or audible artifact

(Unchanged from previous.)

#### Scenario: Prepare at multiple rates and block sizes [Manual]
- Given the Standalone or host configured at 44.1 kHz, 48 kHz, and 96 kHz and different block sizes
- When the plugin is prepared and run
- Then it starts without error and outputs silence when no sample is loaded

(Previously: "...outputs silence.")

### R-PS-6 Editor
The plugin MUST provide a custom editor that embeds `juce::GenericAudioProcessorEditor`, showing all 9 parameters, together with the sample-loader UI defined in `sample-source` (Load button, drag-and-drop, file/status label). MIDI note handling and any other custom parameter UI MUST NOT be provided.

(Previously: "The plugin MUST use `juce::GenericAudioProcessorEditor` as its editor, showing all 9 parameters. No custom UI, sample-loader UI, or MIDI note handling is provided in this change.")

#### Scenario: Editor lists 9 controls and load UI [Manual]
- Given the Standalone launched
- When the editor window opens
- Then it shows 9 parameter controls labelled with the table display names, a Load button, and a file/status label

(Previously: "...shows 9 parameter controls labelled with the table display names" only.)

### R-PS-7 State persistence
`getStateInformation` / `setStateInformation` MUST save and restore the APVTS state, including the loaded sample's absolute path property (see `sample-source`), so parameter values and the loaded file survive a host session reload; a restored path MUST be reloaded asynchronously. Invalid or empty state data MUST be ignored without crashing.

(Previously: "...MUST save and restore the APVTS state so parameter values survive a host session reload. Invalid or empty state data MUST be ignored without crashing.")

#### Scenario: State round trip [Manual]
- Given parameters set to non-default values and a sample loaded
- When the state is saved and restored (host reload or Standalone state)
- Then all 9 values are restored and the sample path is restored and reloaded

(Previously: parameters only.)

#### Scenario: Bad state data [Manual]
- Given empty or corrupt state bytes
- When `setStateInformation` is called
- Then parameters and loaded sample are unchanged and there is no crash

(Previously: "parameters are unchanged".)

## Out of scope (explicit) (modified)
MIDI note handling and gating, custom parameter UI, waveform display, recent files, relative-path lookup, embedded audio in state, load-time resampling, AU, presets, soft limiter, plugin CI job, FetchContent JUCE fallback.

(Previously: "Sample loader and RT-safe publish/retire, MIDI note handling, custom UI, AU, presets, soft limiter, plugin CI job, FetchContent JUCE fallback.")

## Assumptions (modified)
Placeholder metadata (company "Grains", 4-char codes); no soft limiter, so output can reach the ±4 ceiling and the drone is always on once a sample is loaded; absolute sample path stored in state; JUCE licensing (AGPLv3 vs commercial) is the user's distribution decision.

(Previously: "Silent shell with no loader; placeholder metadata...; no soft limiter; JUCE licensing...")

## Unchanged requirements
R-PS-1, R-PS-2, R-PS-3, R-PS-5 are unchanged.
