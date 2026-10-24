# Plugin Shell Specification

## Purpose

An opt-in JUCE plugin (VST3 + Standalone) that hosts `grains_core` with 9 automatable parameters, a sample loader, and an editor embedding the generic parameter editor. Output is silent until a sample is loaded; once loaded the engine renders continuously. JUCE glue is thin; behaviour is verified manually. Any logic that can be tested offline lives in `parameter-mapping` and `sample-source`.

Test-layer legend: **[Manual]** = manual verification in verify phase (no automated coverage; JUCE glue). **[Catch2]** = automated.

## Requirements

### R-PS-1 Targets
When `GRAINS_BUILD_PLUGIN=ON` and JUCE is available, the build MUST define a plugin target producing VST3 and Standalone formats, linking `grains_core`. `grains_core` MUST NOT link or depend on the plugin or JUCE. AU, AAX, and other formats MUST NOT be built. Plugin targets MUST NOT be part of the default `all` build (`EXCLUDE_FROM_ALL` for JUCE; plugin built by explicit target).

#### Scenario: VST3 and Standalone build [Manual]
- Given a local JUCE checkout and `-DGRAINS_BUILD_PLUGIN=ON`
- When the plugin target is built
- Then a `.vst3` bundle and a Standalone executable are produced

#### Scenario: Core never depends on plugin [Manual]
- Given the plugin build is enabled
- When `grains_core` link dependencies are inspected
- Then no JUCE or plugin target appears

### R-PS-2 Placeholder metadata
Plugin metadata MUST use placeholders pending user confirmation: company name "Grains", plugin name "Grains", a 4-character manufacturer code, and a 4-character plugin code (manufacturer code with at least one uppercase letter per JUCE convention). The plugin MUST be an instrument that accepts MIDI input and ignores it in this change (no note, gate, or pitch handling); MIDI handling is a later change. Final identity (company, codes, bundle ID) is explicitly deferred.

#### Scenario: Metadata present [Manual]
- Given a built plugin
- When its reported name, manufacturer, and codes are inspected
- Then they match the placeholders and codes are exactly 4 characters

### R-PS-3 Parameter exposure
The processor MUST create an `AudioProcessorValueTreeState` whose parameters are generated from the `parameter-mapping` spec table: exactly 9 parameters, using the table's string IDs, names, ranges, defaults, and skews. IDs MUST equal the table IDs exactly (stable automation identity). No parameter MAY exist that is not in the table.

#### Scenario: Nine parameters with table IDs [Manual]
- Given the plugin instantiated in Standalone or a host
- When its parameter list is enumerated
- Then there are exactly 9 parameters whose IDs equal the table IDs and whose defaults equal the table defaults

#### Scenario: Automation round trip [Manual]
- Given a host or Standalone UI
- When a parameter is moved to a value and read back
- Then the displayed value matches within the mapping's conversion tolerance

### R-PS-4 Processing and output
`prepareToPlay` MUST prepare the engine with the host sample rate (`GranularEngine::prepare(sampleRate, seed)` takes no block size); the maximum block size MUST be used only to size any scratch buffers. `processBlock` MUST, per block, acquire any newly published source from `SourceExchange` and call `GranularEngine::setSource` only when the source changed, read parameter values, map them via `parameter-mapping` to `EngineParams`, apply them to the engine, and render. While no sample is loaded (or the last restore reported a missing file), the output MUST be silence (exact zeros). Once a sample is loaded, the engine MUST render grains continuously (always-on drone; there is no MIDI gating), and the output amplitude MUST be bounded by the engine's documented ceiling of ±4 (no limiter is applied). The engine MUST NOT produce NaN/Inf, and no clicks, glitches, or denormal-related CPU spikes MAY occur, except a possible brief dropout when the engine pool resets on a source swap. The plugin MUST accept a stereo output bus; input bus handling MAY be disabled.

#### Scenario: Silence without a sample [Manual]
- Given the Standalone running with default parameters and no sample loaded
- When audio runs for at least 30 seconds
- Then the output is silent with no glitches, and CPU stays idle-level

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

#### Scenario: Prepare at multiple rates and block sizes [Manual]
- Given the Standalone or host configured at 44.1 kHz, 48 kHz, and 96 kHz and different block sizes
- When the plugin is prepared and run
- Then it starts without error and outputs silence when no sample is loaded

### R-PS-5 Real-time safety
`processBlock` MUST NOT allocate, lock, block, perform I/O, or throw. Parameter reads MUST use lock-free atomic access (APVTS raw parameter values). Any allocation MUST happen in the constructor or `prepareToPlay`. `ScopedNoDenormals` (or equivalent) MUST be applied in `processBlock`.

#### Scenario: Code review for RT rules [Manual]
- Given the processor source
- When `processBlock` and the functions it calls are reviewed
- Then no allocation, lock, I/O, or exception site is present and `ScopedNoDenormals` is used

#### Scenario: Engine/mapper allocation-free [Catch2]
- Given the existing engine allocation-free test and `parameter-mapping` `noexcept` mapping
- When the suite runs
- Then the engine render path and mapping function are covered by automated tests and pass

### R-PS-6 Editor
The plugin MUST provide a custom editor that embeds `juce::GenericAudioProcessorEditor`, showing all 9 parameters, together with the sample-loader UI defined in `sample-source` (Load button, drag-and-drop, file/status label). MIDI note handling and any other custom parameter UI MUST NOT be provided.

#### Scenario: Editor lists 9 controls and load UI [Manual]
- Given the Standalone launched
- When the editor window opens
- Then it shows 9 parameter controls labelled with the table display names, a Load button, and a file/status label

### R-PS-7 State persistence
`getStateInformation` / `setStateInformation` MUST save and restore the APVTS state, including the loaded sample's absolute path property (see `sample-source`), so parameter values and the loaded file survive a host session reload; a restored path MUST be reloaded asynchronously. Invalid or empty state data MUST be ignored without crashing.

#### Scenario: State round trip [Manual]
- Given parameters set to non-default values and a sample loaded
- When the state is saved and restored (host reload or Standalone state)
- Then all 9 values are restored and the sample path is restored and reloaded

#### Scenario: Bad state data [Manual]
- Given empty or corrupt state bytes
- When `setStateInformation` is called
- Then parameters and loaded sample are unchanged and there is no crash

## Out of scope (explicit)
MIDI note handling and gating, custom parameter UI, waveform display, recent files, relative-path lookup, embedded audio in state, load-time resampling, AU, presets, soft limiter, plugin CI job, FetchContent JUCE fallback.

## Assumptions (pending user confirmation)
Placeholder metadata (company "Grains", 4-char codes); no soft limiter, so output can reach the ±4 ceiling and the drone is always on once a sample is loaded; absolute sample path stored in state; JUCE licensing (AGPLv3 vs commercial) is the user's distribution decision.
