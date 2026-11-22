# Plugin Shell Specification

## Purpose

An opt-in JUCE plugin (VST3 + Standalone) that hosts `grains_core` with 9 automatable parameters, a sample loader, and an editor embedding the generic parameter editor. Output is silent until a sample is loaded; once loaded the engine renders only while Playing. JUCE glue is thin; behaviour is verified manually. Any logic that can be tested offline lives in `parameter-mapping` and `sample-source`.

Test-layer legend: **[Manual]** = manual verification (no automated coverage; JUCE glue). **[Catch2]** = automated.

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
Plugin metadata MUST use placeholders pending user confirmation: company name "Grains", plugin name "Grains", a 4-character manufacturer code, and a 4-character plugin code (manufacturer code with at least one uppercase letter per JUCE convention). The plugin MUST be an instrument that accepts MIDI input and ignores it (no note, gate, or pitch handling); MIDI handling is a later change. Final identity (company, codes, bundle ID) is explicitly deferred.

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
`prepareToPlay` MUST prepare the engine with the host sample rate (`GranularEngine::prepare(sampleRate, seed)` takes no block size); the maximum block size MUST be used only to size any scratch buffers. `prepareToPlay` MUST also publish an empty grain snapshot (`count == 0`) so no stale display data survives a re-prepare. `prepareToPlay` MUST also leave the transport Stopped (`transport-gate` R-TG-8); this applies to every call, including a host-driven `prepareToPlay` in mid-session (for example a sample-rate or buffer change, bypass or offline-render setup), which returns a Playing plugin to Stopped and requires a new Play click (accepted behaviour). `processBlock` MUST, per block, acquire any newly published source from `SourceExchange` and call `GranularEngine::setSource` only when the source changed, in which case it MUST also hard-stop the engine (`transport-gate` R-TG-10) and clear the `playing` flag; it MUST then read the non-parameter atomic `playing` flag once and pass it to the engine, read parameter values, map them via `parameter-mapping` to `EngineParams`, apply them to the engine, render, and then, as the final step on every render path (mono source and stereo source), take a grain snapshot and publish it to the display mailbox (`waveform-display` R-WD-6). A Play or Stop request MUST take effect at a block boundary (block-quantised). The existing `getNumChannels() <= 0` early return, which runs before any render, deliberately does not publish (it returns before anything is rendered, so there is nothing to display). While no sample is loaded (or the last restore reported a missing file), the output MUST be silence (exact zeros). After every successful load, restore, and `prepareToPlay` the transport MUST be Stopped, and the output MUST be exact zeros until Play is requested. While Playing (and during the ~20 ms fade-in and fade-out defined by `transport-gate`), the engine MUST render grains (there is no MIDI gating), and the output amplitude MUST be bounded by the engine's soft output limiter (`grain-rendering` R-GR-10), so that |y| < 1.0 (0 dBFS) at all times; the plugin applies no additional limiter or gain stage beyond the transport gate. While Stopped the engine MUST NOT schedule or render grains, and `processBlock` MUST still publish a snapshot every block (empty once the pool has been reset) so that display markers clear. The publish step MUST NOT change the output. The engine MUST NOT produce NaN/Inf, and no clicks, glitches, or denormal-related CPU spikes MAY occur, except a possible brief dropout when the engine pool resets on a source swap. The plugin MUST accept a stereo output bus; input bus handling MAY be disabled. A Play click made after the loader has published a new source but before the audio thread has adopted it MAY be discarded by that adoption (the plugin stays Stopped); this is accepted, and the editor MUST resync the buttons within 100 ms. A load or restore that fails MUST keep the previously loaded source and MUST NOT disable the buttons if a sample was already loaded. A failed user load MUST NOT change the transport state (a Playing plugin keeps Playing); a restore of recognised plugin state returns the plugin to Stopped even when the referenced sample file cannot be found, while an empty or unrecognised state blob is ignored and changes nothing. The Play/Stop state MUST NOT be a parameter and MUST NOT be saved in state (R-PS-3 and R-PS-7 are unchanged).

#### Scenario: Silence without a sample [Manual]
- Given the Standalone running with default parameters and no sample loaded
- When audio runs for at least 30 seconds
- Then the output is silent with no glitches, and CPU stays idle-level

#### Scenario: Silent after load until Play [Manual]
- Given the Standalone running with no sample
- When a valid audio file is loaded and no button is pressed
- Then the output stays silent (exact zeros) and the Stopped state is shown

#### Scenario: Sound after Play [Manual]
- Given a sample loaded and the plugin Stopped
- When Play is pressed
- Then audible grain output fades in over about 20 ms without any MIDI input and peak output magnitude does not exceed 1

#### Scenario: Stop is click-free and silent [Manual]
- Given the plugin Playing
- When Stop is pressed
- Then the output fades to silence within about 20 ms with no click, and stays exact zeros with idle-level CPU

#### Scenario: Default Stopped after prepare and restore [Catch2/Manual]
- Given the plugin Playing
- When `prepareToPlay` runs (sample-rate change) or state is restored and the sample reloads
- Then the plugin is Stopped and the output is silent until Play is pressed

#### Scenario: Host prepareToPlay mid-session returns to Stopped [Catch2/Manual]
- Given a sample loaded and the plugin Playing
- When the host calls `prepareToPlay` again mid-session (same or different sample rate)
- Then the plugin is Stopped, the output is exact zeros, the editor shows Stop selected, and sound resumes only after a new Play click

#### Scenario: New sample stops playback [Catch2/Manual]
- Given the plugin Playing with sample A
- When the audio thread adopts a newly loaded sample B
- Then the engine is hard-stopped, output is exact zeros with no fade, the `playing` flag is cleared, and B sounds only after Play is pressed

#### Scenario: Play click lost to source adoption [Manual]
- Given a load has just completed and the audio thread has not yet adopted the new source
- When Play is pressed in that window
- Then the plugin MAY remain Stopped, and within 100 ms the buttons show Stop selected, matching the audio

#### Scenario: Toggle is block-quantised [Catch2]
- Given the `playing` flag changes in the middle of a block
- When that block and the next are processed
- Then the change takes effect at the start of the next block and the first sample of that block already uses the new state

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

#### Scenario: Snapshot still published while stopped [Catch2/Manual]
- Given the plugin stopped after being Playing
- When blocks are processed after the fade-out
- Then a snapshot with `count == 0` is published every block and the dots clear

### R-PS-5 Real-time safety
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
- Then the allocation count is 0 (core part; the processor call site is verified by review)

### R-PS-6 Editor
The plugin MUST provide a custom editor that embeds `juce::GenericAudioProcessorEditor`, showing all 9 parameters, together with the sample-loader UI defined in `sample-source` (Load button, drag-and-drop, file/status label), a Play button and a Stop button, and the waveform/position view defined in `waveform-display` (`WaveformView` above the generic editor). The top bar (40 px) MUST contain, in order, the Load button (100 px), the Play button (about 64 px), the Stop button (about 64 px), and the status label. Play and Stop MUST be two toggle buttons in one radio group that reflect the processor's Play/Stop state, MUST be disabled until a sample has loaded successfully, MUST be enabled on a successful load, and MUST show Stop as selected after every load or restore. The editor MUST sync the buttons to the processor's Play/Stop state and loaded-sample state with a 10 Hz editor timer plus the existing change callback (which syncs immediately); button toggle and enabled states MUST be updated only on change and without sending notifications, so a sync never triggers a Play/Stop request. The editor MUST NOT add global keyboard shortcuts, pause/resume, or any other transport control. The editor window MUST have a fixed size. MIDI note handling and any other custom parameter UI MUST NOT be provided. The set of parameters MUST remain exactly the 9 of R-PS-3; the waveform view only drives the existing `position` parameter and MUST NOT add parameters.

#### Scenario: Editor lists 9 controls, load UI, transport and waveform view [Manual]
- Given the Standalone launched
- When the editor window opens
- Then it shows the waveform view above 9 parameter controls labelled with the table display names, a Load button, Play and Stop buttons, and a file/status label

#### Scenario: Parameter count unchanged [Manual]
- Given the plugin with the waveform view and the Play/Stop buttons
- When its parameter list is enumerated
- Then there are still exactly 9 parameters with the table IDs (R-PS-3)

#### Scenario: Fixed window size [Manual]
- Given the editor open
- When the user attempts to resize it
- Then the size does not change

#### Scenario: Buttons disabled before load [Manual]
- Given the Standalone launched with no sample
- When the editor is inspected
- Then Play and Stop are disabled

#### Scenario: Buttons enable on load, Stop selected [Manual]
- Given no sample loaded
- When a valid sample loads successfully
- Then Play and Stop become enabled and Stop is selected

#### Scenario: Radio behaviour [Manual]
- Given a sample loaded
- When Play is pressed and then Stop is pressed
- Then exactly one of the two buttons is selected at any time and the audio follows the selected state

#### Scenario: Failed load keeps buttons disabled [Manual]
- Given no sample loaded
- When a load fails or the restored file is missing
- Then Play and Stop remain disabled and the output is silent

#### Scenario: Failed load keeps old sample and buttons [Manual]
- Given a sample loaded and the plugin Stopped
- When a subsequent load or restore fails
- Then the old sample stays loaded, Play and Stop remain enabled, and the plugin stays Stopped

#### Scenario: Failed user load while Playing keeps playing [Manual]
- Given a sample loaded and the plugin Playing
- When a subsequent user load fails
- Then the old sample stays loaded, Play and Stop remain enabled, and the plugin keeps Playing

#### Scenario: Buttons follow processor state [Manual]
- Given the processor stopped by a prepare or state restore while the editor is open
- When the editor timer (10 Hz) next fires or the change callback runs
- Then the Stop button shows as selected within 100 ms, without triggering a Play/Stop request

### R-PS-7 State persistence
`getStateInformation` / `setStateInformation` MUST save and restore the APVTS state, including the loaded sample's reference properties (`samplePath`, `sampleName`, and, when anchored, `sampleAnchor` and `sampleRel`; see `sample-source` R-SS-8), so parameter values and the loaded file survive a host session reload, including after the sample folder or the user account changed; a restored reference MUST be reloaded asynchronously. State saved by the previous format (absolute path only) MUST still restore. Invalid or empty state data MUST be ignored without crashing.

#### Scenario: State round trip [Manual]
- Given parameters set to non-default values and a sample loaded
- When the state is saved and restored (host reload or Standalone state)
- Then all 9 values are restored and the sample reference is restored and reloaded

#### Scenario: Bad state data [Manual]
- Given empty or corrupt state bytes
- When `setStateInformation` is called
- Then parameters and loaded sample are unchanged and there is no crash

#### Scenario: Old-format state still restores [Manual]
- Given state saved with only the absolute `samplePath` property and 9 parameter values
- When `setStateInformation` is called
- Then the 9 values are restored and the sample reloads from the path

#### Scenario: Moved sample folder restores [Manual]
- Given state saved with a sample, then the sample moved to a folder reachable through the stored anchor or a search directory
- When the state is restored
- Then the sample loads and the label reads "Loaded: <name> (relocated)"

#### Scenario: Missing sample label unchanged [Manual]
- Given state whose sample exists at no candidate location
- When the state is restored
- Then the label reads "Missing: <name>" and the 9 parameter values are restored

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

## Out of scope (explicit)
MIDI note handling and gating, custom parameter UI (beyond the waveform/position view), recent files, embedded audio in state, runtime (pitch-shift) aliasing fixes, interpolator or oversampling changes in `processBlock`, removal of the engine's source-rate compensation, AU, presets, lookahead limiting and any user-adjustable ceiling or drive, plugin CI job, FetchContent JUCE fallback, following the host transport, pause/resume, natural tail on Stop, automatable or persisted Play state, keyboard shortcuts, and MIDI gating of the transport.

## Assumptions (pending user confirmation)
Placeholder metadata (company "Grains", 4-char codes); output is bounded to |y| < 1.0 by a fixed soft limiter in the engine with no user control, and after a sample is loaded the plugin starts Stopped and renders only while Playing (the Play/Stop state is not persisted); the sample reference (absolute path, name, and an optional anchor-relative path) is stored in state, and the absolute path is still written; JUCE licensing (AGPLv3 vs commercial) is the user's distribution decision.
