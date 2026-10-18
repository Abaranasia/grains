# Proposal: Granular Synth Foundation (CMake + Catch2 + JUCE-free DSP core)

## Intent

The repo is an empty Projucer `guiapp` (cannot build VST3) with no tests, so Strict TDD is blocked. This change delivers the tested, real-time-safe granular engine and the harness every later change depends on.

## Scope

### In Scope
- Root `CMakeLists.txt`; option `GRAINS_BUILD_PLUGIN=OFF` (core/tests configure with no JUCE).
- `grains_core` static lib (C++17, zero JUCE): `SampleBuffer` (immutable, mono/stereo), Hann LUT `WindowTable`, `Grain`, fixed `GrainPool` (128, skip-on-full), `GrainScheduler` (density gr/s, fractional accumulator, in-block offsets, seeded injectable RNG, periodic+jitter), `GranularEngine` (instance-based, overlap gain normalization, boundary-safe reads, `Interpolator` interface with linear impl).
- Catch2 v3 `grains_tests` via `ctest`: window, scheduler drift/block-split invariance, bounds, clamping, same-seed bit-identical output, no NaN/Inf, extremes, no allocation in `process()`.

### Out of Scope (roadmap)
1. Plugin shell: `juce_add_plugin` VST3+Standalone, APVTS, MIDI gate/pitch, sample loader, minimal native UI; delete `Grains.jucer`, `Builds/`, `JuceLibraryCode/`.
2. Waveform/position UI. 3. Per-note voices, more windows, Hermite. 4. Modulation/effects. 5. Optional WebView UI. 6. Presets/relink. 7. pluginval, CI, AU/macOS.
8. Selectable grain playback modes (later change, open-ended): forward (current), wrap/loop from the start at buffer end, reverse playback along the sample, random position, etc. Not designed here; this change only keeps the boundary-safe read/interpolator interface open for them.

## Capabilities

### New Capabilities
- `build-harness`: CMake layout, JUCE-free core target, Catch2/ctest.
- `grain-scheduling`: spawn timing, jitter, RNG determinism, pool overflow.
- `grain-rendering`: windowing, interpolation, boundary safety, gain normalization, RT-safety.

### Modified Capabilities
None.

## Approach

- **Plugin shell deferred**: core+tests forecast ~600-800 lines; the shell would exceed 800.
- **Projucer files kept**, deleted in roadmap #1 together with their replacement (one rollback unit, no unbuildable gap).
- **JUCE reference (roadmap #1)**: `GRAINS_JUCE_DIR` → `add_subdirectory`, default relative `../../Projucer` (local 9.0.0), else FetchContent pinned to the 9.0.0 tag. Tradeoff: offline local reuse vs. CI reproducibility.
- Catch2 via FetchContent pinned v3 tag.

## Affected Areas

| Area | Impact | Description |
|------|--------|-------------|
| `CMakeLists.txt`, `cmake/` | New | Build + deps |
| `Source/core/` | New | DSP core |
| `tests/` | New | Catch2 suite |
| `openspec/config.yaml` | Modified | Runner = ctest |

## Risks

| Risk | Likelihood | Mitigation |
|------|------------|------------|
| Budget overrun | Med | Interpolator: linear only; chain PRs |
| Allocation detection on MSVC | Med | Counting `operator new` in test binary |
| Golden fragility | Med | Seeded invariants + tolerances |

## Rollback Plan

Additive; revert merge commit(s). Projucer files untouched.

## Dependencies

- CMake >= 3.25, MSVC (VS2026), C++17; Catch2 v3 (network on first configure).

## Success Criteria

- [ ] Fresh configure+build of `grains_tests` without JUCE; `ctest` green.
- [ ] Identical seed/params => bit-identical output.
- [ ] Zero allocations in `process()`, including pool full.
- [ ] No clicks at sample edges; no NaN/Inf at extremes.

## Proposal question round

All assumptions confirmed by the user (2026-10-17):
1. Pool full: skip new grains (not steal oldest). — CONFIRMED
2. Pitch is playback rate; position scan speed deferred to roadmap #1. — CONFIRMED
3. Output normalized so dense clouds ≈ sparse loudness. — CONFIRMED
4. Removing the Projucer workflow in roadmap #1 is acceptable. — CONFIRMED
