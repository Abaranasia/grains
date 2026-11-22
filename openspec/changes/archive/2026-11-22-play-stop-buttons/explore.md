# Exploration: play-stop-buttons

Mirror of engram `sdd/play-stop-buttons/explore` (#729). Verified (V) vs from memory (M) as stated by the explorer; the live-display interaction was verified afterwards by the orchestrator against branch `feat/live-grain-view`.

## Question
Add Play and Stop buttons to the editor. Today the engine renders grains continuously once a sample is loaded (plugin-shell R-PS-4: always-on drone, no MIDI gating).

## Current state
- (V) `GranularEngine::process` returns zeros without a source; otherwise `scheduler_.advance`, render grains, per-sample `softLimit`. `setSource` resets the pool; `prepare` resets pool, scheduler and rng. The scheduler handles density 0 without a burst on resume.
- (V) `processBlock` ignores MIDI and reads the 9 raw atomics once per block. The sample reference is stored as ValueTree properties on the APVTS state, not as parameters; a `playing` property could follow that pattern.
- (V, orchestrator, on `feat/live-grain-view`) `processBlock` publishes one grain snapshot per block after the render on every render path (the zero-channel early return aside), so an emptied pool publishes `count == 0` each block and the dots clear within one poll. A stopped design must keep that publish.
- (V) Editor top bar: 40 px, 100 px Load button, status label.

## Approaches
1. **Atomic `playing` flag plus JUCE-free `TransportGate`** (per-sample linear ramp ~20 ms before `softLimit`). Stop fades out, then resets pool and scheduler; Play fades in. Default Playing. Zero CPU while stopped; every Play is a deterministic restart. Low-Medium, ~400 lines. (recommended)
2. Fade then freeze (pause): scheduler not advanced, no rng draws while stopped; seamless resume, but frozen grains stay in the pool so the display must clear explicitly. Medium.
3. APVTS bool parameter: automatable and recallable, but breaks R-PS-3 (exactly 9 parameters), needs state migration. Medium-High.
Rejected: output gate only (burns CPU); natural tail only (up to a full grain length to go quiet, feels laggy).

## Design notes
- The ramp multiplies the grain sum before `softLimit` (keeps |y| < 1 and the NaN/denormal flush exact). While steadily playing the multiply is skipped so output stays bit-identical to today.
- Toggles are quantised to block boundaries; the R-GR-6 block-split test must toggle at the same sample in both runs. `prepare` recomputes the ramp length and resets the gate.
- Following the host transport is deferred (Standalone has none); it would need a fallback to Playing without a playhead.
- UI: Load (100 px), Play (~64 px), Stop (~64 px), status label in the 40 px bar; two `TextButton`s in a radio group with `setClickingTogglesState`; no global shortcuts. Sync state from the existing 30 Hz poll or the change broadcaster.

## Testability
Catch2: ramp endpoints exactly 0 and 1 with bounded steps; Playing bit-identical to today; exact zeros after Stop; deterministic restart; block-split invariance across a toggle; no rng draws while stopped; |y| < 1 during the ramp; zero-allocation realtime test. Manual: buttons, DAW transport, state restore, display.

## Spec impact
plugin-shell R-PS-4 ("continuous render" becomes "while Playing, default Playing"), R-PS-6 (buttons added; amend the custom-UI rule), out-of-scope wording; R-PS-7 only if state is persisted; R-PS-3 unchanged; a `transport-gate` capability or a `grain-rendering` requirement; waveform-display R-WD-7 (markers clear when stopped).

## Size
~400 lines (core ~100, tests ~150, plugin ~50, specs ~120): one PR; split core+tests+specs then plugin only if the display work grows.

## Risks
Spec wording around the always-on drone changes in several places; block-quantised transition against R-GR-6 needs a careful test; sample-rate change must recompute the ramp; persisting Stopped could make a reopened project silent.

## Open product questions (ask one at a time)
1. What should Stop do to the sound: fade then restart from scratch on Play, fade and resume where it left off, or let grains ring out?
2. Default at load: Playing (today) or Stopped?
3. Persist Play/Stop in the plugin state, or always start at the default?
4. Follow the host transport later (not v1)?
5. Is a ~20 ms fade acceptable; disable the buttons until a sample is loaded?
6. Reseed the rng on each Play so every restart is identical?
