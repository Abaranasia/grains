# Waveform Display Specification

## Purpose

Show the loaded sample as a waveform with the `position` marker and a read-only jitter band, let click/drag set `position`, and show the grains currently playing as live lane dots in a strip under the waveform. Pure logic (peaks, geometry, grain snapshot, mailbox, display model) lives in `grains_core` (JUCE-free, Catch2-tested); the plugin view only paints, polls, and forwards mouse input through the geometry.

Test-layer legend: **[Automated]** = Catch2 on `grains_core` (host-free). **[Manual]** = manual verification (JUCE glue, not covered by Catch2).

## Requirements

### R-WD-1 Peaks contract
`WaveformPeaks` MUST compute, from a source buffer, a fixed number of buckets `N` (caller-supplied, N >= 1), each holding a `min` and `max` of the mixed-mono signal (arithmetic mean of all channels per frame) over that bucket's frame range. The buckets MUST cover the full sample with no gaps or overlaps (every frame belongs to exactly one bucket). For every bucket, `min <= max`. Output length MUST equal `N` for any non-empty input. Non-finite input samples (NaN, +/-Inf) MUST NOT appear in the output: they MUST be treated as 0.0. Empty input (zero frames or zero channels) MUST yield an empty result (no buckets) and MUST NOT throw. When frames < `N`, the result MUST still have `N` buckets without reading out of range (buckets with no frames repeat the nearest valid frame's value or are flat at 0; either way finite and `min <= max`). The computation MUST be deterministic: identical input and `N` MUST produce bit-identical output. Mono input MUST equal the channel itself (mixed-mono of one channel).

#### Scenario: Mixed mono min/max per bucket [Automated]
- Given a stereo buffer with left = +1.0 and right = -1.0 everywhere
- When peaks are computed with any `N`
- Then every bucket has `min == max == 0.0`

#### Scenario: Bucket count and coverage [Automated]
- Given a 1000-frame mono buffer containing a single spike at frame 500
- When peaks are computed with `N = 10`
- Then exactly 10 buckets are returned, exactly one bucket holds the spike as its `max`, and all buckets satisfy `min <= max`

#### Scenario: Empty input [Automated]
- Given a buffer with zero frames (or zero channels)
- When peaks are computed
- Then the result is empty and no exception is thrown

#### Scenario: Fewer frames than buckets [Automated]
- Given a 3-frame buffer
- When peaks are computed with `N = 64`
- Then the result has 64 buckets, all values are finite, and `min <= max` for each

#### Scenario: NaN and Inf input [Automated]
- Given a buffer containing NaN and +/-Inf samples
- When peaks are computed
- Then all output values are finite

#### Scenario: Determinism [Automated]
- Given the same buffer and `N` computed twice
- When the two results are compared
- Then they are bit-identical

### R-WD-2 Geometry mapping
`WaveformGeometry` MUST map between a horizontal pixel coordinate and a normalised position in [0, 1] for a view of a given pixel width. `xToPosition(x, width)` MUST return `x / width` clamped to [0, 1]; `positionToX(p, width)` MUST return `p * width` with `p` clamped to [0, 1]. A width <= 0 MUST NOT divide by zero: `xToPosition` MUST return 0.0 and `positionToX` MUST return 0.0. Round trip `xToPosition(positionToX(p, w), w)` MUST equal `p` within floating-point tolerance for `w > 0` and `p` in [0, 1]. The jitter band MUST be computed from `position` and `positionJitter` (both normalised) as `[position - jitter, position + jitter]` clamped to [0, 1] on both ends, with `lo <= hi`; out-of-range or non-finite inputs MUST be clamped (non-finite treated as 0). Geometry MUST NOT depend on JUCE.

#### Scenario: x to position and back [Automated]
- Given a width of 400 pixels
- When x = 100 is mapped to a position and back
- Then the position is 0.25 and the round trip returns x = 100 within tolerance

#### Scenario: Clamp outside the view [Automated]
- Given a width of 400 pixels
- When x = -20 and x = 500 are mapped to positions
- Then the results are 0.0 and 1.0 respectively

#### Scenario: Zero width [Automated]
- Given a width of 0
- When x is mapped to a position and a position to x
- Then both results are 0.0 and no division by zero occurs

#### Scenario: Jitter band clamped to [0, 1] [Automated]
- Given `position = 0.1` and `positionJitter = 0.3`
- When the jitter band is computed
- Then it is `[0.0, 0.4]`; and for `position = 0.9`, `positionJitter = 0.3` it is `[0.6, 1.0]`

#### Scenario: Non-finite geometry input [Automated]
- Given a NaN position or jitter
- When the band or x is computed
- Then the result is finite and within [0, 1]

### R-WD-3 Peaks delivery and lifecycle
Peaks MUST be computed on the loader thread (never on the audio or message thread) and delivered as an immutable `std::shared_ptr` inside `SampleLoader::Result`. The processor MUST hold the current peaks and MUST apply delivered results in the order loads take effect on the audio path (commit order): a superseded load MUST NOT replace the peaks of a later-committed source, and a later load that fails without replacing the playing source MUST leave the current peaks unchanged. Peaks MUST be cleared when a Restore clears the source or reports a missing file. When the loader redelivers the source after a host sample-rate change (R-PS-8 Reload), peaks MUST be redelivered with it. If computing peaks fails (for example allocation failure), the load MUST still publish the source to the audio path and play normally, and the view MUST show a "no waveform" state. The change MUST NOT alter `SourceExchange`, `processBlock`, or audio output: output MUST be bit-identical to before the change. An editor opened after a sample was loaded MUST show the current waveform.

#### Scenario: Peaks arrive with the loaded sample [Manual]
- Given the Standalone with no sample
- When a valid audio file is loaded
- Then the waveform appears for that file

#### Scenario: Stale load does not replace peaks [Manual]
- Given load A is in flight and load B is requested before A finishes
- When both finish
- Then only B's waveform is shown

#### Scenario: Failed later load keeps current waveform [Manual]
- Given sample A is playing and its waveform is shown
- When a later load fails without replacing A
- Then A keeps playing and A's waveform stays shown

#### Scenario: Restore with missing file clears waveform [Manual]
- Given a waveform is shown
- When a state with a missing sample file is restored
- Then the waveform is cleared, the "drop a sample" hint is shown, and the label reads "Missing: <name>"

#### Scenario: Rate change redelivers peaks [Manual]
- Given a sample is loaded and shown
- When the host sample rate changes and the reload completes
- Then the waveform is still shown and matches the loaded sample

#### Scenario: Peaks failure still loads and plays [Manual]
- Given peaks computation fails during a load
- When the load finishes
- Then the sample is published and audible, and the view shows a "no waveform" state

#### Scenario: Late-opened editor shows waveform [Manual]
- Given a sample is loaded and the editor is closed
- When the editor is reopened
- Then it shows the current waveform

#### Scenario: Audio output unchanged [Manual]
- Given identical parameters and sample before and after this change
- When the same blocks are rendered
- Then the output is bit-identical

### R-WD-4 Waveform view
The editor MUST show a `WaveformView` above the generic parameter editor, displaying the full sample (no zoom or scroll) as mixed mono min/max peaks, a `position` marker, and a read-only jitter band derived from `position` and `positionJitter` via `WaveformGeometry`. Mouse down in the view MUST set `position` to the clicked x (absolute jump) and subsequent drag MUST continue to update `position` from the pointer x (clamped to [0, 1]). The drag MUST drive the `position` parameter through a `juce::ParameterAttachment`, calling begin-change-gesture on mouse down and end-change-gesture on mouse up so host automation records the move. The view MUST NOT change any parameter other than `position` (the jitter band is not draggable). The marker and band MUST follow `position` and `positionJitter` changes from automation or the generic sliders. When no peaks are available, the view MUST show a "drop a sample" hint if no sample is loaded, and a "no waveform" indication if a sample is loaded without peaks. The static layers (waveform, jitter band, `position` marker) MUST stay change-driven: they MUST repaint only on change (parameter, peaks, or resize) and MUST be cached so a live tick does not recompute them. The view MAY own a poll timer at a single named rate constant of 30 Hz, and that timer MUST run only while the view is showing, a waveform is displayed (Waveform state) and a poll function is set (started when all three hold, stopped on hide, on leaving the Waveform state, and on destruction). A timer tick MUST request a repaint only when a fresh grain frame was received or when the display model transitions to stale (R-WD-7); otherwise it MUST NOT repaint. The editor window size MUST be fixed (not resizable), taller than before to fit the view and the lane strip; all existing editor elements remain.

#### Scenario: Click jumps position [Manual]
- Given a loaded sample and `position` at 0.1
- When the user clicks at 75% of the view width
- Then `position` becomes 0.75 and the marker moves there immediately

#### Scenario: Drag continues from click and clamps [Manual]
- Given the user pressed in the view
- When the pointer is dragged past the left and right edges
- Then `position` follows the pointer and clamps to 0.0 and 1.0

#### Scenario: Gestures recorded for automation [Manual]
- Given the host is recording automation
- When the user presses, drags, and releases in the view
- Then one begin and one end change gesture bracket the move and the `position` automation lane records it

#### Scenario: Jitter band is read-only and follows parameter [Manual]
- Given a loaded sample
- When the user drags the `positionJitter` slider
- Then the band width updates, and dragging inside the view never changes `positionJitter`

#### Scenario: Marker follows automation [Manual]
- Given host automation moving `position`
- When playback runs
- Then the `position` marker follows the parameter change and repaints on the parameter change, not because of a timer tick

#### Scenario: Empty state hint [Manual]
- Given no sample loaded
- When the editor is shown
- Then the view shows a "drop a sample" hint instead of a waveform

#### Scenario: Timer only while showing and fixed size [Manual]
- Given the running editor
- When the view is hidden or the editor is closed, and when the user tries to resize the window
- Then the poll timer is stopped while hidden and after close, a single 30 Hz constant defines its rate, and the window does not resize

#### Scenario: Idle tick does not repaint [Manual]
- Given a loaded sample, host transport stopped, and markers already cleared
- When the timer ticks repeatedly with no parameter or peaks change
- Then no repaint is requested and the static layers are not recomputed

#### Scenario: Editor open during load [Manual]
- Given the editor is open
- When a sample finishes loading or the state is restored
- Then the view updates without a crash or stale waveform

### R-WD-5 Grain snapshot contract
`GrainSnapshot` MUST be a plain, JUCE-free, trivially copyable value type with a fixed-capacity array of at most 128 `GrainMark` entries (matching the pool capacity), a `count`, and a format `version` field so the layout can be extended later without changing the mailbox. Each `GrainMark` MUST carry: pool `slot` index, `position` (grain head position in the source, normalised to [0, 1]), `progress` (grain age fraction, [0, 1]) and `level` (envelope level at the current progress, [0, 1]). All normalised fields MUST be finite and clamped to [0, 1]; non-finite inputs MUST be treated as 0. `slot` MUST be less than the pool capacity. `count` MUST equal the engine's `activeGrainCount()` at the instant of capture and MUST NOT exceed 128. An engine with no source, or with no active grains, MUST yield `count == 0` and MUST NOT throw. `GranularEngine::snapshotGrains(GrainSnapshot&) const noexcept` MUST fill the snapshot by const iteration over the pool, with at most 128 iterations and no allocation.

#### Scenario: Count equals active grains [Automated]
- Given an engine rendering with several active grains
- When a snapshot is taken after a block
- Then `snapshot.count == engine.activeGrainCount()` and `count <= 128`

#### Scenario: Marks are normalised and finite [Automated]
- Given an engine rendering at extreme parameters (position outside [0, 1], extreme pitch and size)
- When snapshots are taken after each block
- Then every mark has `position`, `progress`, and `level` finite and within [0, 1], and `slot` below the pool capacity

#### Scenario: Slots are distinct and stable [Automated]
- Given an engine with several active grains
- When a snapshot is taken
- Then no two marks share a `slot`, and a grain keeps the same `slot` across consecutive snapshots while it lives

#### Scenario: Empty and no source [Automated]
- Given an engine with no source, and an engine with a source and density 0
- When a snapshot is taken
- Then `count == 0` and no exception is thrown

#### Scenario: Saturated pool [Automated]
- Given a saturated pool (density and size at their maxima)
- When a snapshot is taken
- Then `count` equals the pool capacity and never exceeds 128

#### Scenario: Snapshot is versioned [Automated]
- Given a default-constructed `GrainSnapshot`
- When its `version` is read
- Then it equals the documented current version constant and the type is trivially copyable

### R-WD-6 Snapshot publication
Snapshots MUST be published from the audio thread to the message thread through a generic, JUCE-free triple-buffer mailbox that is wait-free on both sides: the writer MUST NOT lock, wait, allocate, or throw, and the reader MUST NOT block the writer. A frame read by the reader MUST be tear-free (never a mix of two published frames). The reader MUST be able to tell whether a frame is fresh (newer than the last frame it consumed); consuming without a new publish MUST report not-fresh. The mailbox MUST be generic over the payload type so other display payloads can reuse it. The processor MUST publish after render on both the mono-source and stereo-source render paths of `processBlock` (the stereo early-return MUST NOT skip the publish), and MUST publish an empty snapshot (`count == 0`) after `prepareToPlay` so no stale marks survive a re-prepare. Publication MUST NOT alter audio output.

#### Scenario: Reader sees last published frame [Automated]
- Given a mailbox and a writer that publishes frames A then B
- When the reader consumes
- Then it receives B marked fresh, and a second consume reports not-fresh

#### Scenario: Not fresh before any publish [Automated]
- Given a newly created mailbox
- When the reader consumes
- Then it reports not-fresh

#### Scenario: Tear-free under two-thread stress [Automated]
- Given a writer thread publishing self-consistent frames (all fields derived from one sequence number) at high rate and a reader thread consuming concurrently
- When the stress run completes
- Then every frame the reader obtains is internally consistent and sequence numbers it sees never decrease

#### Scenario: Publish is allocation-free [Automated]
- Given a test binary that counts global operator new/delete calls
- When the counter is reset after preparation and many snapshot-and-publish cycles run
- Then the allocation count is 0

#### Scenario: Publish on mono and stereo paths [Manual]
- Given a mono sample and a stereo sample loaded in turn
- When audio runs
- Then the lane strip shows live grains for both

#### Scenario: Empty snapshot after prepare [Manual]
- Given grains were shown and the host re-prepares the plugin
- When the next frame is displayed
- Then no stale dots remain from before the re-prepare

### R-WD-7 Live lane-dot view
The view MUST show a strip under the waveform (inside the same view, below the peaks area) with one dot slot per pool slot, laid out on a fixed number of rows (the strip height and slot-to-row mapping are design decisions, constant for a given view size). A dot MUST be drawn at the x of the mark's `position` (via `WaveformGeometry`) on the row of its `slot`, with alpha derived from its `level` (louder envelope is more opaque; level 0 is faint or invisible). The dots are a live-only layer: they MUST NOT alter or invalidate the cached static layers.

A JUCE-free `GrainDisplayModel` in `grains_core` MUST decide fresh versus stale: it takes the poll results and counts poll ticks (not wall-clock time), holds the last frame while frames keep arriving, and MUST clear the markers once no fresh frame has arrived for the stale timeout (one named constant of 250 ms, counted in poll ticks as 8 consecutive polls without a fresh frame, about 267 ms effective at 30 Hz; the poll rate is a separate single named constant of 30 Hz). Frames arriving again MUST resume display immediately. The model MUST report a stale transition exactly once per fresh-to-stale change so the view can repaint once to clear. Markers MUST be shown only in the Waveform state (peaks available) and MUST NOT be shown in the "no waveform" or "drop a sample" states. When the host is stopped (no `processBlock` publishes), the markers MUST clear after the same 8 polls (about 267 ms). Grains shorter than one audio block can start and finish between two snapshots and are not guaranteed to be shown (documented limitation; the display samples at block rate, so what is shown is representative, not exhaustive).

#### Scenario: Fresh frame displays [Automated]
- Given a display model and a fresh frame with 5 marks
- When the model is updated
- Then it exposes those 5 marks as visible

#### Scenario: Marks held until the timeout [Automated]
- Given a model that last received a fresh frame at t = 0
- When it is polled 7 consecutive times without a new frame (one below the stale tick count)
- Then the marks are still exposed and no stale transition is reported

#### Scenario: Stale clears markers once [Automated]
- Given a model that last received a fresh frame at t = 0
- When it is polled the 8th consecutive time without a new frame (the stale tick count), and again later
- Then the markers are cleared and the stale transition is reported exactly once

#### Scenario: Fresh frame resumes after stale [Automated]
- Given a model in the stale state
- When a fresh frame arrives
- Then markers are exposed again immediately

#### Scenario: Constants are named and distinct [Automated]
- Given the display model header
- When the poll rate and stale timeout are inspected
- Then the poll rate is a single named constant equal to 30 Hz and the stale timeout a single named constant of 250 ms, counted as 8 polls

#### Scenario: Dots appear under the waveform and follow the grains [Manual]
- Given a loaded sample and audio running
- When the user moves `position` and `positionJitter`
- Then dots appear in the strip under the waveform, spread around the position, with alpha following each grain's envelope

#### Scenario: Host stopped clears markers [Manual]
- Given markers are showing and the DAW transport stops (no `processBlock` calls)
- When 8 consecutive polls pass without a fresh frame (about 267 ms)
- Then the markers clear and the view stops repainting

#### Scenario: No markers without a waveform [Manual]
- Given no sample loaded, or a sample loaded with peaks unavailable
- When audio runs or the view is shown
- Then no grain markers are drawn and the existing hint or "no waveform" text is shown

#### Scenario: Static layers not recomputed per tick [Manual]
- Given a loaded sample and live markers running at 30 Hz
- When the view repaints for live ticks only
- Then the waveform, band, and position marker come from the cached image and resizing and DPI changes still render correctly

#### Scenario: CPU with multiple instances [Manual]
- Given several plugin instances open with their editors showing
- When audio runs with the timer active
- Then CPU use stays low and idle (hidden or stopped) views use no timer

## Out of scope (explicit)
Jitter dragging, new parameters, zoom/scroll, stacked stereo display, playhead, theming, WebView UI, extent bars, grain direction, audio or interaction real-time feedback (meters, scope), TSAN CI job (separate micro-PR).
