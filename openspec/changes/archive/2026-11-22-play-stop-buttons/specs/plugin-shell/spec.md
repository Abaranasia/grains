# Plugin Shell Specification (delta)

Base: this delta is written against the real main plugin-shell text, which already contains the `live-grain-display` publish step in R-PS-4 and the waveform view in R-PS-6 (`live-grain-display` is the base of this branch). Only the intended Play/Stop changes are applied; the rest of each requirement is the exact main wording.

## MODIFIED Requirements

### Purpose
An opt-in JUCE plugin (VST3 + Standalone) that hosts `grains_core` with 9 automatable parameters, a sample loader, and an editor embedding the generic parameter editor. Output is silent until a sample is loaded; once loaded the engine renders only while Playing. JUCE glue is thin; behaviour is verified manually. Any logic that can be tested offline lives in `parameter-mapping` and `sample-source`.

### R-PS-4 Processing and output
`prepareToPlay` MUST prepare the engine with the host sample rate (`GranularEngine::prepare(sampleRate, seed)` takes no block size); the maximum block size MUST be used only to size any scratch buffers. `prepareToPlay` MUST also publish an empty grain snapshot (`count == 0`) so no stale display data survives a re-prepare. `prepareToPlay` MUST also leave the transport Stopped (`transport-gate` R-TG-8); this applies to every call, including a host-driven `prepareToPlay` in mid-session (for example a sample-rate or buffer change, bypass or offline-render setup), which returns a Playing plugin to Stopped and requires a new Play click (accepted behaviour). `processBlock` MUST, per block, acquire any newly published source from `SourceExchange` and call `GranularEngine::setSource` only when the source changed, in which case it MUST also hard-stop the engine (`transport-gate` R-TG-10) and clear the `playing` flag; it MUST then read the non-parameter atomic `playing` flag once and pass it to the engine, read parameter values, map them via `parameter-mapping` to `EngineParams`, apply them to the engine, render, and then, as the final step on every render path (mono source and stereo source), take a grain snapshot and publish it to the display mailbox (`waveform-display` R-WD-6). A Play or Stop request MUST take effect at a block boundary (block-quantised). The existing `getNumChannels() <= 0` early return, which runs before any render, deliberately does not publish (it returns before anything is rendered, so there is nothing to display). While no sample is loaded (or the last restore reported a missing file), the output MUST be silence (exact zeros). After every successful load, restore, and `prepareToPlay` the transport MUST be Stopped, and the output MUST be exact zeros until Play is requested. While Playing (and during the ~20 ms fade-in and fade-out defined by `transport-gate`), the engine MUST render grains (there is no MIDI gating), and the output amplitude MUST be bounded by the engine's soft output limiter (`grain-rendering` R-GR-10), so that |y| < 1.0 (0 dBFS) at all times; the plugin applies no additional limiter or gain stage beyond the transport gate. While Stopped the engine MUST NOT schedule or render grains, and `processBlock` MUST still publish a snapshot every block (empty once the pool has been reset) so that display markers clear. The publish step MUST NOT change the output. The engine MUST NOT produce NaN/Inf, and no clicks, glitches, or denormal-related CPU spikes MAY occur, except a possible brief dropout when the engine pool resets on a source swap. The plugin MUST accept a stereo output bus; input bus handling MAY be disabled. A Play click made after the loader has published a new source but before the audio thread has adopted it MAY be discarded by that adoption (the plugin stays Stopped); this is accepted, and the editor MUST resync the buttons within 100 ms. A load or restore that fails MUST keep the previously loaded source, MUST leave the plugin Stopped, and MUST NOT disable the buttons if a sample was already loaded. The Play/Stop state MUST NOT be a parameter and MUST NOT be saved in state (R-PS-3 and R-PS-7 are unchanged).

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
- Then it starts without error and outputs silence when no sample is loaded or while Stopped

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

#### Scenario: Buttons follow processor state [Manual]
- Given the processor stopped by a prepare or state restore while the editor is open
- When the editor timer (10 Hz) next fires or the change callback runs
- Then the Stop button shows as selected within 100 ms, without triggering a Play/Stop request

### Out of scope (explicit)
MIDI note handling and gating, custom parameter UI (beyond the waveform/position view), recent files, embedded audio in state, runtime (pitch-shift) aliasing fixes, interpolator or oversampling changes in `processBlock`, removal of the engine's source-rate compensation, AU, presets, lookahead limiting and any user-adjustable ceiling or drive, plugin CI job, FetchContent JUCE fallback, following the host transport, pause/resume, natural tail on Stop, automatable or persisted Play state, keyboard shortcuts, and MIDI gating of the transport.

### Assumptions (pending user confirmation)
Placeholder metadata (company "Grains", 4-char codes); output is bounded to |y| < 1.0 by a fixed soft limiter in the engine with no user control, and after a sample is loaded the plugin starts Stopped and renders only while Playing (the Play/Stop state is not persisted); the sample reference (absolute path, name, and an optional anchor-relative path) is stored in state, and the absolute path is still written; JUCE licensing (AGPLv3 vs commercial) is the user's distribution decision.

#### Scenario: Spec wording consistent [Manual]
- Given the merged main plugin-shell spec
- When it is searched for "drone", "always-on" and "renders continuously"
- Then none remain as current behaviour

## Unchanged
R-PS-1, R-PS-2, R-PS-3 (exactly 9 parameters), R-PS-5 (snapshot and publish step stays lock-free and allocation-free; the gate adds no allocation, see `transport-gate` R-TG-9), R-PS-7 (no Play/Stop in state, no migration), R-PS-8.
