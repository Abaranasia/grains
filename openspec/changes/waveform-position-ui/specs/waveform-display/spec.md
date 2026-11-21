# Waveform Display Specification (delta: new capability)

## Purpose

Show the loaded sample as a waveform with the `position` marker and a read-only jitter band, and let click/drag set `position`. Pure logic (peaks, geometry) lives in `grains_core` (JUCE-free, Catch2-tested); the plugin view only paints and forwards mouse input through the geometry. Live active-grain display is out of scope.

Test-layer legend: **[Automated]** = Catch2 on `grains_core` (host-free). **[Manual]** = manual verification (JUCE glue, not covered by Catch2).

## ADDED Requirements

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
The editor MUST show a `WaveformView` above the generic parameter editor, displaying the full sample (no zoom or scroll) as mixed mono min/max peaks, a `position` marker, and a read-only jitter band derived from `position` and `positionJitter` via `WaveformGeometry`. Mouse down in the view MUST set `position` to the clicked x (absolute jump) and subsequent drag MUST continue to update `position` from the pointer x (clamped to [0, 1]). The drag MUST drive the `position` parameter through a `juce::ParameterAttachment`, calling begin-change-gesture on mouse down and end-change-gesture on mouse up so host automation records the move. The view MUST NOT change any parameter other than `position` (the jitter band is not draggable). The marker and band MUST follow `position` and `positionJitter` changes from automation or the generic sliders. When no peaks are available, the view MUST show a "drop a sample" hint if no sample is loaded, and a "no waveform" indication if a sample is loaded without peaks. The view MUST repaint only on change (parameter, peaks, or resize) and MUST NOT use a timer. The editor window size MUST be fixed (not resizable), taller than before to fit the view; all existing editor elements remain.

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
- Then the marker follows without any timer-driven repaint

#### Scenario: Empty state hint [Manual]
- Given no sample loaded
- When the editor is shown
- Then the view shows a "drop a sample" hint instead of a waveform

#### Scenario: No timer and fixed size [Manual]
- Given the view source and the running editor
- When reviewed and when the user tries to resize the window
- Then no `Timer` is used for the view and the window does not resize

#### Scenario: Editor open during load [Manual]
- Given the editor is open
- When a sample finishes loading or the state is restored
- Then the view updates without a crash or stale waveform

## Out of scope (explicit)
Live active-grain display, jitter dragging, new parameters, zoom/scroll, stacked stereo display, playhead, theming, WebView UI.
