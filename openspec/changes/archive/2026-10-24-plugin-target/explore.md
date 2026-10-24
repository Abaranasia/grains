# Explore: plugin-target (roadmap #1)

Full exploration is mirrored in Engram (`sdd/plugin-target/explore`).

## Roadmap source

- `openspec/changes/archive/2026-10-18-granular-synth/proposal.md`, Out of Scope #1: plugin shell (`juce_add_plugin` VST3 + Standalone), APVTS, MIDI, sample loader, minimal UI, Projucer file removal. JUCE via `GRAINS_JUCE_DIR` -> `add_subdirectory`, default `../../Projucer` (local JUCE 9.0.0), else FetchContent pinned to 9.0.0.
- `design.md` (same folder): FATAL_ERROR guard; engine takes a non-owning `const SampleBuffer*` (atomic publish + deferred free left to #1); soft limiter deferred to the plugin stage.
- Spec conflict: `openspec/specs/build-harness/spec.md` R-BH-3 forbids plugin targets and touching Projucer files; this change needs a MODIFIED R-BH-3.

## Current build state

- `CMakeLists.txt`: `GRAINS_BUILD_PLUGIN` (OFF) hits FATAL_ERROR when ON.
- `Source/core/CMakeLists.txt`: `grains_core` static lib with deferred `grains_assert_juce_free` (R-BH-1). The plugin must link core, never the reverse.
- `tests/CMakeLists.txt`: explicit source lists for `grains_tests` and `grains_realtime_tests`.
- `.github/workflows/ci.yml`: both jobs pass `-DGRAINS_BUILD_PLUGIN=OFF`. A plugin CI job needs JUCE on the runner plus Linux GUI/audio dev packages; defer.
- No `Source/*.cpp|h` outside `Source/core`; plugin code is new. `Grains.jucer` is a `guiapp` and cannot build VST3.

## Obtaining JUCE

- Local JUCE 9.0.0 at `../../Projucer` relative to the repo root (referenced by `Grains.jucer`), with a top-level `CMakeLists.txt`.
- Recommended: cache path `GRAINS_JUCE_DIR` (default `../../Projucer`); when `GRAINS_BUILD_PLUGIN=ON`, `add_subdirectory(${GRAINS_JUCE_DIR} juce EXCLUDE_FROM_ALL)`; clear error if missing. No FetchContent in this PR.
- Default plugin-OFF configure must not touch JUCE; 84/84 tests unchanged.
- JUCE 9 is AGPLv3 or commercial: a distribution decision for the user, not a build blocker.

## Minimal design

- `Source/plugin/` added only inside the `GRAINS_BUILD_PLUGIN` branch: `juce_add_plugin(GrainsPlugin FORMATS VST3 Standalone IS_SYNTH TRUE NEEDS_MIDI_INPUT TRUE ...)`, linking `grains_core` and `juce::juce_audio_utils`.
- JUCE-free parameter layer (testable): spec table (id, name, min/max/default/skew) for the 9 `EngineParams` fields built from `grains::limits`; mapping to `EngineParams`; normalised <-> plain conversion. Header-only in `Source/core/include/grains/` or a small JUCE-free lib.
- `PluginProcessor`: APVTS from the spec table with stable IDs; `prepareToPlay` -> `engine.prepare`; `processBlock` with `ScopedNoDenormals`, read atomics, `setParams`, `engine.process`, mono bus copies L; no allocation.
- Editor: `juce::GenericAudioProcessorEditor`.
- Deferred: sample loader (RT-safe publish/retire), MIDI, custom UI, AU, presets, CI plugin job, Projucer file deletion.

## Testability

- Catch2 (JUCE-free): parameter specs, mapping, conversion, clamping.
- JUCE-only (manual): APVTS construction, processBlock glue, editor. Verify by building locally and loading the Standalone; record in verify report.

## Estimate

CMake ~60, param layer ~120, tests ~150, processor + editor ~180, specs/docs ~80: ~550-650 lines. With a minimal loader ~750-850 (budget risk).

## Open questions (user)

1. Silent shell vs. minimal sample loader in this PR.
2. JUCE source: local only, or also FetchContent fallback.
3. Delete Projucer files here or in a follow-up (recommended: follow-up).
4. Soft limiter: user decision, out of scope.
5. Modifying R-BH-3.
6. Plugin metadata (manufacturer code, plugin code, company, bundle id).

## Risks

- JUCE glue has no automated coverage.
- Local JUCE path is machine-specific; other machines must pass `GRAINS_JUCE_DIR`.
- Plugin configure is heavy (juceaide builds at configure time).
- `grains_assert_juce_free` must still pass with the plugin directory present.
