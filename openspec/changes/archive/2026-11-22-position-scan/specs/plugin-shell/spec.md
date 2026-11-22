# Delta for Plugin Shell

Purpose line (non-requirement text) changes from "9 automatable parameters" to "10 automatable parameters". Waveform-display is unchanged: the marker stays at the `position` anchor and dragging sets `position` (R-WD-4/5/7); the moving read point is not drawn.

## MODIFIED Requirements

### R-PS-3 Parameter exposure
The processor MUST create an `AudioProcessorValueTreeState` whose parameters are generated from the `parameter-mapping` spec table: exactly 10 parameters, using the table's string IDs, names, ranges, defaults, and skews. IDs MUST equal the table IDs exactly (stable automation identity). No parameter MAY exist that is not in the table. `scan` MUST be exposed as an automatable bipolar parameter with default 0.
(Previously: exactly 9 parameters, no `scan`.)

#### Scenario: Ten parameters with table IDs [Manual]
- Given the plugin instantiated in Standalone or a host
- When its parameter list is enumerated
- Then there are exactly 10 parameters whose IDs equal the table IDs and whose defaults equal the table defaults

#### Scenario: Automation round trip [Manual]
- Given a host or Standalone UI
- When a parameter is moved to a value and read back
- Then the displayed value matches within the mapping's conversion tolerance

#### Scenario: Scan bipolar display and automation [Manual]
- Given a host or Standalone UI
- When `scan` is moved through -1, 0 and +1 (UI and host automation)
- Then the displayed values are -1, 0 and +1, the 0 position is the centre of the control, and fine adjustment near 0 is practical

### R-PS-4 Processing and output
`prepareToPlay` MUST prepare the engine with the host sample rate (`GranularEngine::prepare(sampleRate, seed)` takes no block size); the maximum block size MUST be used only to size any scratch buffers. `prepareToPlay` MUST also publish an empty grain snapshot (`count == 0`) so no stale display data survives a re-prepare. `prepareToPlay` MUST also leave the transport Stopped (`transport-gate` R-TG-8); this applies to every call, including a host-driven `prepareToPlay` in mid-session (for example a sample-rate or buffer change, bypass or offline-render setup), which returns a Playing plugin to Stopped and requires a new Play click (accepted behaviour). `processBlock` MUST, per block, acquire any newly published source from `SourceExchange` and call `GranularEngine::setSource` only when the source changed, in which case it MUST also hard-stop the engine (`transport-gate` R-TG-10) and clear the `playing` flag; it MUST then read the non-parameter atomic `playing` flag once and pass it to the engine, read parameter values, map them via `parameter-mapping` to `EngineParams`, apply them to the engine, render, and then, as the final step on every render path (mono source and stereo source), take a grain snapshot and publish it to the display mailbox (`waveform-display` R-WD-6). A Play or Stop request MUST take effect at a block boundary (block-quantised). The existing `getNumChannels() <= 0` early return, which runs before any render, deliberately does not publish (it returns before anything is rendered, so there is nothing to display). While no sample is loaded (or the last restore reported a missing file), the output MUST be silence (exact zeros). After every successful load, restore, and `prepareToPlay` the transport MUST be Stopped, and the output MUST be exact zeros until Play is requested. While Playing (and during the ~20 ms fade-in and fade-out defined by `transport-gate`), the engine MUST render grains (there is no MIDI gating), and the output amplitude MUST be bounded by the engine's soft output limiter (`grain-rendering` R-GR-10), so that |y| < 1.0 (0 dBFS) at all times; the plugin applies no additional limiter or gain stage beyond the transport gate. While Stopped the engine MUST NOT schedule or render grains, and `processBlock` MUST still publish a snapshot every block (empty once the pool has been reset) so that display markers clear. The publish step MUST NOT change the output. The engine MUST NOT produce NaN/Inf, and no clicks, glitches, or denormal-related CPU spikes MAY occur, except a possible brief dropout when the engine pool resets on a source swap. The plugin MUST accept a stereo output bus; input bus handling MAY be disabled. A Play click made after the loader has published a new source but before the audio thread has adopted it MAY be discarded by that adoption (the plugin stays Stopped); this is accepted, and the editor MUST resync the buttons within 100 ms. A load or restore that fails MUST keep the previously loaded source and MUST NOT disable the buttons if a sample was already loaded. A failed user load MUST NOT change the transport state (a Playing plugin keeps Playing); a restore of recognised plugin state returns the plugin to Stopped even when the referenced sample file cannot be found, while an empty or unrecognised state blob is ignored and changes nothing. The Play/Stop state MUST NOT be a parameter and MUST NOT be saved in state (R-PS-3 and R-PS-7 are unchanged).
(Previously: scenario "Parameter moves while silent" swept 9 parameters; requirement text unchanged. The `scan` offset is reset by Play, load and prepare through `transport-gate` R-TG-4, not by this requirement.)

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
- When all 10 parameters are swept through their full ranges
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

#### Scenario: Scan moves the read point while playing [Manual]
- Given a sample loaded, `position` set, `scan` = 0.1, and Play pressed
- When audio runs for several seconds, then Stop and Play are pressed again
- Then the grain content audibly travels through the sample, the waveform marker stays at the `position` anchor, and each Play restarts from the anchor

### R-PS-6 Editor
The plugin MUST provide a custom editor that embeds `juce::GenericAudioProcessorEditor`, showing all 10 parameters, together with the sample-loader UI defined in `sample-source` (Load button, drag-and-drop, file/status label), a Play button and a Stop button, and the waveform/position view defined in `waveform-display` (`WaveformView` above the generic editor). The top bar (40 px) MUST contain, in order, the Load button (100 px), the Play button (about 64 px), the Stop button (about 64 px), and the status label. Play and Stop MUST be two toggle buttons in one radio group that reflect the processor's Play/Stop state, MUST be disabled until a sample has loaded successfully, MUST be enabled on a successful load, and MUST show Stop as selected after every load or restore. The editor MUST sync the buttons to the processor's Play/Stop state and loaded-sample state with a 10 Hz editor timer plus the existing change callback (which syncs immediately); button toggle and enabled states MUST be updated only on change and without sending notifications, so a sync never triggers a Play/Stop request. The editor MUST NOT add global keyboard shortcuts, pause/resume, or any other transport control. The editor window MUST have a fixed size, sized so that all 10 parameter controls (including `scan`) are fully visible and not clipped. MIDI note handling and any other custom parameter UI MUST NOT be provided. The set of parameters MUST remain exactly the 10 of R-PS-3; the waveform view only drives the existing `position` parameter and MUST NOT add parameters.
(Previously: 9 parameters, no explicit requirement that the fixed window fits every control.)

#### Scenario: Editor lists 10 controls, load UI, transport and waveform view [Manual]
- Given the Standalone launched
- When the editor window opens
- Then it shows the waveform view above 10 parameter controls (including `scan`) labelled with the table display names, all fully visible, a Load button, Play and Stop buttons, and a file/status label

#### Scenario: Parameter count unchanged by the UI [Manual]
- Given the plugin with the waveform view and the Play/Stop buttons
- When its parameter list is enumerated
- Then there are exactly 10 parameters with the table IDs (R-PS-3)

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
`getStateInformation` / `setStateInformation` MUST save and restore the APVTS state, including the loaded sample's reference properties (`samplePath`, `sampleName`, and, when anchored, `sampleAnchor` and `sampleRel`; see `sample-source` R-SS-8), so parameter values and the loaded file survive a host session reload, including after the sample folder or the user account changed; a restored reference MUST be reloaded asynchronously. State saved by the previous format (absolute path only) MUST still restore. State saved before `scan` existed (9 parameter values, no `scan`) MUST still restore, with `scan` at its default 0. Invalid or empty state data MUST be ignored without crashing.
(Previously: no statement about state saved without `scan`. Needs design confirmation: how the missing `scan` value is guaranteed to read 0 rather than a stale in-memory value.)

#### Scenario: State round trip [Manual]
- Given parameters set to non-default values (including `scan`) and a sample loaded
- When the state is saved and restored (host reload or Standalone state)
- Then all 10 values are restored and the sample reference is restored and reloaded

#### Scenario: Bad state data [Manual]
- Given empty or corrupt state bytes
- When `setStateInformation` is called
- Then parameters and loaded sample are unchanged and there is no crash

#### Scenario: Old-format state still restores [Manual]
- Given state saved with only the absolute `samplePath` property and 9 parameter values
- When `setStateInformation` is called
- Then the 9 values are restored, `scan` is 0, and the sample reloads from the path

#### Scenario: Pre-scan state restores with scan at 0 [Catch2/Manual]
- Given a plugin with `scan` set to a non-zero value, and state saved by a build without `scan` (9 parameter values)
- When `setStateInformation` is called
- Then the 9 values are restored and `scan` reads 0

#### Scenario: Moved sample folder restores [Manual]
- Given state saved with a sample, then the sample moved to a folder reachable through the stored anchor or a search directory
- When the state is restored
- Then the sample loads and the label reads "Loaded: <name> (relocated)"

#### Scenario: Missing sample label unchanged [Manual]
- Given state whose sample exists at no candidate location
- When the state is restored
- Then the label reads "Missing: <name>" and the 10 parameter values are restored
