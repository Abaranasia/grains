# Plugin Shell Specification

## Purpose

An opt-in JUCE plugin (VST3 + Standalone) that hosts `grains_core` with 9 automatable parameters and a generic editor. This first shell is deliberately silent: no sample loader exists, so output is silence. JUCE glue is thin; behaviour is verified manually. Any logic that can be tested offline lives in `parameter-mapping`.

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

### R-PS-4 Processing and silent output
`prepareToPlay` MUST prepare the engine with the host sample rate (`GranularEngine::prepare(sampleRate, seed)` takes no block size); the maximum block size MUST be used only to size any scratch buffers. `processBlock` MUST, per block, read parameter values, map them via `parameter-mapping` to `EngineParams`, apply them to the engine, and render. Because no sample is loaded, the output MUST be silence (exact zeros), the engine MUST NOT produce NaN/Inf, and no clicks, glitches, or denormal-related CPU spikes MAY occur. The plugin MUST accept a stereo output bus; input bus handling MAY be disabled.

#### Scenario: Silence without a sample [Manual]
- Given the Standalone running with default parameters and no sample loaded
- When audio runs for at least 30 seconds
- Then the output is silent with no glitches, and CPU stays idle-level

#### Scenario: Parameter moves while silent [Manual]
- Given the Standalone running
- When all 9 parameters are swept through their full ranges
- Then there is no crash, hang, or audible artifact

#### Scenario: Prepare at multiple rates and block sizes [Manual]
- Given the Standalone or host configured at 44.1 kHz, 48 kHz, and 96 kHz and different block sizes
- When the plugin is prepared and run
- Then it starts without error and outputs silence

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

### R-PS-6 Generic editor
The plugin MUST use `juce::GenericAudioProcessorEditor` as its editor, showing all 9 parameters. No custom UI, sample-loader UI, or MIDI note handling is provided in this change.

#### Scenario: Editor lists 9 controls [Manual]
- Given the Standalone launched
- When the editor window opens
- Then it shows 9 parameter controls labelled with the table display names

### R-PS-7 State persistence
`getStateInformation` / `setStateInformation` MUST save and restore the APVTS state so parameter values survive a host session reload. Invalid or empty state data MUST be ignored without crashing.

#### Scenario: State round trip [Manual]
- Given parameters set to non-default values
- When the state is saved and restored (host reload or Standalone state)
- Then all 9 values are restored

#### Scenario: Bad state data [Manual]
- Given empty or corrupt state bytes
- When `setStateInformation` is called
- Then parameters are unchanged and there is no crash

## Out of scope (explicit)
Sample loader and RT-safe publish/retire, MIDI note handling, custom UI, AU, presets, soft limiter, plugin CI job, FetchContent JUCE fallback.

## Assumptions (pending user confirmation)
Silent shell with no loader; placeholder metadata (company "Grains", 4-char codes); no soft limiter; JUCE licensing (AGPLv3 vs commercial) is the user's distribution decision.
