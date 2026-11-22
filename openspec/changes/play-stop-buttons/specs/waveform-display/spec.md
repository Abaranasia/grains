# Waveform Display Specification (delta)

Base: this delta is written against the real main waveform-display text (R-WD-1 to R-WD-7 as merged from `live-grain-display`). `live-grain-display` is the base of this branch (`feat/play-stop-buttons` is based on `feat/live-grain-view`); it is no longer a pending dependency. Only the changes below apply; everything else in the two requirements is unchanged.

Intended changes: (a) R-WD-4: a fresh empty frame that does not change what is shown MUST NOT cause a repaint, because while Stopped the processor publishes an empty snapshot every block; (b) R-WD-7: clarification that stopped transport (empty snapshot) clears the dots with no new publish path, polling rate, or Play/Stop knowledge in the view.

## MODIFIED Requirements

### R-WD-4 Waveform view
The editor MUST show a `WaveformView` above the generic parameter editor, displaying the full sample (no zoom or scroll) as mixed mono min/max peaks, a `position` marker, and a read-only jitter band derived from `position` and `positionJitter` via `WaveformGeometry`. Mouse down in the view MUST set `position` to the clicked x (absolute jump) and subsequent drag MUST continue to update `position` from the pointer x (clamped to [0, 1]). The drag MUST drive the `position` parameter through a `juce::ParameterAttachment`, calling begin-change-gesture on mouse down and end-change-gesture on mouse up so host automation records the move. The view MUST NOT change any parameter other than `position` (the jitter band is not draggable). The marker and band MUST follow `position` and `positionJitter` changes from automation or the generic sliders. When no peaks are available, the view MUST show a "drop a sample" hint if no sample is loaded, and a "no waveform" indication if a sample is loaded without peaks. The static layers (waveform, jitter band, `position` marker) MUST stay change-driven: they MUST repaint only on change (parameter, peaks, or resize) and MUST be cached so a live tick does not recompute them. The view MAY own a poll timer at a single named rate constant of 30 Hz, and that timer MUST run only while the view is showing, a waveform is displayed (Waveform state) and a poll function is set (started when all three hold, stopped on hide, on leaving the Waveform state, and on destruction). A timer tick MUST request a repaint only when a fresh grain frame was received that changes what is displayed, or when the display model transitions to stale (R-WD-7); otherwise it MUST NOT repaint. A fresh frame with `count == 0` received while no markers are displayed (for example the empty snapshot published every block while the transport is Stopped) does not change what is displayed and MUST NOT cause a repaint. The editor window size MUST be fixed (not resizable), taller than before to fit the view and the lane strip; all existing editor elements remain.

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

#### Scenario: Stopped plugin publishing empty frames does not repaint [Manual]
- Given a loaded sample, the plugin Stopped and its dots already cleared, so every block publishes an empty snapshot
- When the timer ticks repeatedly
- Then no repaint is requested even though fresh empty frames keep arriving

#### Scenario: Editor open during load [Manual]
- Given the editor is open
- When a sample finishes loading or the state is restored
- Then the view updates without a crash or stale waveform

### R-WD-7 Live lane-dot view
The view MUST show a strip under the waveform (inside the same view, below the peaks area) with one dot slot per pool slot, laid out on a fixed number of rows (the strip height and slot-to-row mapping are design decisions, constant for a given view size). A dot MUST be drawn at the x of the mark's `position` (via `WaveformGeometry`) on the row of its `slot`, with alpha derived from its `level` (louder envelope is more opaque; level 0 is faint or invisible). The dots are a live-only layer: they MUST NOT alter or invalidate the cached static layers.

A JUCE-free `GrainDisplayModel` in `grains_core` MUST decide fresh versus stale: it takes the poll results and counts poll ticks (not wall-clock time), holds the last frame while frames keep arriving, and MUST clear the markers once no fresh frame has arrived for the stale timeout (one named constant of 250 ms, counted in poll ticks as 8 consecutive polls without a fresh frame, about 267 ms effective at 30 Hz; the poll rate is a separate single named constant of 30 Hz). Frames arriving again MUST resume display immediately. The model MUST report a stale transition exactly once per fresh-to-stale change so the view can repaint once to clear. Markers MUST be shown only in the Waveform state (peaks available) and MUST NOT be shown in the "no waveform" or "drop a sample" states. When the host is stopped (no `processBlock` publishes), the markers MUST clear after the same 8 polls (about 267 ms). Grains shorter than one audio block can start and finish between two snapshots and are not guaranteed to be shown (documented limitation; the display samples at block rate, so what is shown is representative, not exhaustive). When the plugin transport is Stopped (`transport-gate`), the processor publishes an empty snapshot (`count == 0`) every block once the grain pool has been reset (`plugin-shell` R-PS-4); the model MUST treat a fresh empty frame like any other fresh frame, so the markers clear at the first poll that receives it (and never later than the stale timeout), the model MUST report that fresh-with-marks to empty change as one repaint, and subsequent fresh empty frames MUST NOT report a repaint. The display MUST NOT require any new publish path, new polling rate, or Play/Stop knowledge.

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

#### Scenario: Empty fresh snapshot clears model with one repaint [Automated]
- Given `GrainDisplayModel` showing marks, then a fresh empty snapshot (`count == 0`)
- When the model is updated
- Then the markers are cleared, one repaint is reported, and subsequent fresh empty snapshots report none

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

#### Scenario: Dots clear on Stop [Manual]
- Given the plugin Playing with lane dots visible
- When Stop is pressed and the fade-out completes
- Then within one poll after the empty snapshot is polled, no dots are drawn (and no later than the stale timeout)

#### Scenario: No dots while stopped after load [Manual]
- Given a sample just loaded (default Stopped)
- When the waveform view is shown
- Then no grain dots are drawn until Play is pressed

#### Scenario: Dots resume on Play [Manual]
- Given the plugin Stopped with no dots
- When Play is pressed
- Then dots appear again as grains start and no new timer or publish path is involved

## Unchanged
R-WD-1, R-WD-2, R-WD-3, R-WD-5, R-WD-6, and the Purpose and Out of scope sections.
