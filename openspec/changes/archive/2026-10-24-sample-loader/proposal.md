# Proposal: Sample Loader (RT-safe source publish + load UI)

## Intent

The plugin shell builds and loads but is silent: nothing ever calls `GranularEngine::setSource`. This change lets the user load an audio file (button or drag-and-drop), hands it to the engine without real-time hazards, and restores it on session reload. It is the next roadmap item after `plugin-target`.

## Scope

### In Scope
- Core (JUCE-free): `SourceExchange` lock-free pending/retire mailbox; wait-free, allocation-free consumer; no frees on the audio thread; destructor frees everything. Strict TDD (ownership counters, publish twice, retire, 2-thread stress, allocation-free acquire+setSource+process).
- Core (JUCE-free): sample builder from planar data: >2 channels keep first two, cap length at 5 min at file rate with truncation notice, reject empty data or bad rate.
- Plugin: `SampleLoader` background thread (latest request wins, guarded `callAsync`); `processBlock` acquires and calls `setSource` only on change.
- State: absolute path as a property on `apvts_.state`; async reload after `replaceState`. Missing file: silent, path kept, label "Missing: <name>". Decode failure: keep previous source, show error. Never publish an empty buffer.
- Editor: custom editor embedding `GenericAudioProcessorEditor`, Load button (async `FileChooser`, wav/aiff/aif), drag-and-drop, file/status label.
- Spec text: document the ±4 output ceiling and the always-on drone (no MIDI gating).

### Out of Scope
- Waveform display, custom parameter UI, recent files, relative-path lookup, embedded audio in state.
- Load-time resampling (engine already scales by source/host rate), MIDI control, soft limiter.
- Plugin CI job, JUCE FetchContent fallback (separate change), plugin identity codes, VST3-in-DAW checks.

## Capabilities

### New Capabilities
- `sample-source`: exchange ownership/RT rules, builder rules (channels, length cap, rejection), loader/state/missing-file/error behaviour.

### Modified Capabilities
- `plugin-shell`: R-PS-4 (silent only with no sample; renders once loaded; ±4 ceiling, no gating), R-PS-6 (custom editor embedding the generic editor plus load UI), R-PS-7 (sample path persisted and reloaded), Purpose and out-of-scope text.

## Approach

Explore recommendation A2+B2: all ownership logic in a JUCE-free core class unit-tested with Catch2; JUCE glue (decode, thread, chooser, drag-drop) stays thin and is verified manually. `atomic<shared_ptr>` and fixed double buffers rejected (audio-thread frees / not lock-free on MSVC).

## Affected Areas

| Area | Impact | Description |
|------|--------|-------------|
| `Source/core/include/grains/`, `Source/core/src/` | New | `SourceExchange`, sample builder |
| `Source/plugin/` | Modified/New | Processor wiring, `SampleLoader`, custom editor |
| `tests/` | Modified | Exchange, builder, realtime tests |
| `openspec/specs/plugin-shell/` | Modified | Deltas above |

## Risks

| Risk | Likelihood | Mitigation |
|------|------------|------------|
| Mailbox memory-ordering bug (no TSan on MSVC) | Med | Simple acquire/release protocol, stress test, review |
| Destructor vs `callAsync`/loader races | Med | Stop thread before members die; weak/guarded callbacks |
| Loud unlimited output (±4), drone starts on load | Med | Documented; limiter is a follow-up |
| Pool reset dropout on swap | Low | Accepted; documented |
| Absolute path leaks user layout in state | Low | Documented; relative lookup deferred |
| Budget ~745/800 lines | High | Shrink stress test first; spec text kept terse |

## Rollback Plan

Revert the PR (commits core, plugin, docs). Plugin stays opt-in behind `GRAINS_BUILD_PLUGIN`; default build and existing tests unaffected.

## Dependencies

- `feat/plugin-target` (stacked base); local JUCE 9.0.0 via `GRAINS_JUCE_DIR`.

## Success Criteria

- [ ] Default build JUCE-free; all existing plus new core tests pass, including allocation-free acquire path.
- [ ] Standalone: load via button and drag-drop produces sound; replace while playing has no crash.
- [ ] State reload restores the file; missing file shows "Missing: <name>" and stays silent.
- [ ] One PR under 800 changed lines.

## Proposal question round

User away (auto mode); defaults accepted, open for review:
1. Is 5 min the right length cap, and is truncation (vs rejection) acceptable?
2. Should the drone stay always-on until MIDI gating, given loud ±4 output?
3. Is an absolute path in saved state acceptable for sharing projects?
