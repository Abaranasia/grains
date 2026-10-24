# Proposal: Plugin Target (silent VST3 + Standalone shell)

## Intent

The JUCE-free engine (`grains_core`) is tested but cannot run in a host. This change adds an opt-in JUCE plugin build so the engine and its 9 parameters can be loaded, automated and smoke-tested in a DAW or Standalone. It is roadmap item #1 from the archived `granular-synth` proposal.

## Scope

### In Scope
- CMake: `GRAINS_JUCE_DIR` cache path (default `../../Projucer`) and `add_subdirectory(... EXCLUDE_FROM_ALL)` only when `GRAINS_BUILD_PLUGIN=ON`; clear error if JUCE is missing. Replaces the current FATAL_ERROR guard.
- JUCE-free parameter layer: spec table (id, name, range, default, skew) for all 9 `EngineParams` fields built from `grains::limits`, mapping to `EngineParams`, normalised/plain conversion. Catch2 tests, strict TDD.
- `Source/plugin/`: `PluginProcessor` (APVTS from the spec table, RT-safe `processBlock`, silence without a sample) and `juce::GenericAudioProcessorEditor`.
- Delta specs plus a short build note.

### Out of Scope
- Sample loader with RT-safe publish/retire (next change), MIDI note handling, custom UI, AU, presets.
- Soft limiter (user decision).
- FetchContent JUCE fallback; plugin CI job; Projucer file deletion.

## Capabilities

### New Capabilities
- `plugin-shell`: VST3 + Standalone target, APVTS parameters with stable IDs, RT-safe processing, generic editor.
- `parameter-mapping`: JUCE-free parameter spec table and mapping/conversion rules.

### Modified Capabilities
- `build-harness`: R-BH-3 changes from "no plugin targets" to "plugin targets only behind `GRAINS_BUILD_PLUGIN` (default OFF); default build stays JUCE-free; Projucer files untouched". R-BH-5 CI text stays plugin-OFF.

## Approach

Plugin links `grains_core`, never the reverse; `grains_assert_juce_free` keeps guarding the core. Parameter logic lives in the JUCE-free layer so it is unit-tested; JUCE glue stays thin and is verified manually (build + Standalone launch).

## Affected Areas

| Area | Impact | Description |
|------|--------|-------------|
| `CMakeLists.txt` | Modified | Plugin branch, `GRAINS_JUCE_DIR` |
| `Source/core/include/grains/` | New | Parameter spec/mapping header |
| `Source/plugin/` | New | Processor, editor, CMake |
| `tests/` | Modified | Parameter tests |
| `openspec/specs/build-harness/` | Modified | R-BH-3 delta |

## Risks

| Risk | Likelihood | Mitigation |
|------|------------|------------|
| JUCE glue has no automated coverage | High | Keep glue thin; manual Standalone check in verify |
| Local JUCE path machine-specific | Med | Overridable cache var, clear error |
| Default build regresses | Low | CI keeps plugin OFF; 84/84 must stay green |
| Budget overrun | Low | Estimate ~550-650 of 800 lines; loader excluded |

## Rollback Plan

Revert the PR. Plugin code is isolated behind an OFF-by-default option; core, tests, CI and Projucer files are otherwise unchanged.

## Dependencies

- Local JUCE 9.0.0 at `../../Projucer` (or `GRAINS_JUCE_DIR`).

## Success Criteria

- [ ] Default configure is JUCE-free; 84/84 existing tests plus new parameter tests pass.
- [ ] `-DGRAINS_BUILD_PLUGIN=ON` builds VST3 and Standalone locally.
- [ ] Standalone launches, shows 9 parameters, outputs silence without glitches.
- [ ] Projucer files and CI workflow unchanged; diff under 800 lines.

## Assumptions pending user confirmation

1. Silent shell; no sample loader in this PR.
2. JUCE from local path only (`GRAINS_JUCE_DIR`, default `../../Projucer`); no FetchContent.
3. Projucer files untouched; deletion is a follow-up.
4. Soft limiter out of scope, undecided.
5. R-BH-3 modified as above.
6. Metadata placeholders (company "Grains", 4-char manufacturer/plugin codes) to be confirmed. JUCE AGPLv3 vs commercial licensing is the user's distribution decision.
7. CI unchanged; plugin CI job deferred.

## Proposal question round

User was away; these remain open:
1. Should the first host-loadable build be useful (needs loader) or only a parameter/automation smoke test?
2. Final plugin identity (company, codes, bundle id) before any distribution?
3. Distribution intent: AGPLv3 open source or commercial JUCE licence?
