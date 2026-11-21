# Verify Report: waveform-position-ui (PR 1 core + PR 2 plugin)

Mode: hybrid, Strict TDD active. Branch feat/waveform-plugin-view (2a16087), base of PR 1 = 736a9a6.
Verdict: **PASS WITH WARNINGS** (0 CRITICAL, 4 WARNING, 5 SUGGESTION). Manual scenarios (tasks 6.1-6.3) are carried as PENDING by user decision.

## Completeness
Tasks 1.1-5.4 are all [x] (14/14 implementation tasks). 6.1-6.3 are unchecked, pending user manual checks (not failed).

## Build and test evidence (executed in this run)
| Command | Result |
|---|---|
| cmake --build build --config Debug --clean-first (core, /W4 /WX) | exit 0, no warnings |
| ctest --test-dir build -C Debug | exit 0, 246/246 passed |
| cmake --build build --config Release --clean-first | exit 0, no warnings |
| ctest --test-dir build -C Release | exit 0, 251/251 passed (includes alloc-failure and realtime suites) |
| grains_alloc_failure_tests.exe (Release) | 7 test cases, 232 assertions, all passed (includes the peaks nth-allocation-failure sweep) |
| ctest -R waveform (Release) | 28/28 passed |
| cmake --build build-plugin --config Debug --target GrainsPlugin_Standalone GrainsPlugin_VST3 | exit 0; Debug Standalone (Grains.exe) and Debug VST3 (Grains.vst3) both built |
| same, --config Release | exit 0; Release Standalone and VST3 built |
| ctest --test-dir build-plugin -C Release and -C Debug | 251/251 each (see W4) |

Coverage and linter: not configured in this project (skipped, not a failure).

## Strict TDD compliance
| Check | Result |
|---|---|
| TDD evidence reported | Partial: apply-progress is prose (RED = missing header / C2039, alloc-sweep mutation check), not a TDD Cycle Evidence table (W1) |
| Test files exist | 3/3 (test_waveform_geometry.cpp 5 cases, test_waveform_peaks.cpp 21 cases, alloc sweep 1 case) |
| GREEN re-run now | all pass |
| Triangulation | Adequate: several N and frame counts, companion non-empty cases, state transitions triangulated |
| PR 2 tests | None: JUCE glue with no testable seam; covered by the manual checklist (W2) |

Assertion quality: no tautologies, no ghost loops (loops iterate over REQUIRE-checked non-empty collections), no smoke-only tests. The empty-input test has a non-empty companion. 0 CRITICAL, 0 WARNING.
Test layer: all Unit (Catch2), 27 test cases across 3 files; no integration/E2E tooling in the project.

## Compliance matrix
Legend: A = Automated (ran and passed), M = Manual (pending), S = also verified by static reading in this run.

### R-WD-1 Peaks contract
| Scenario | Tag | Evidence |
|---|---|---|
| Mixed mono min/max | A | peaks: stereo +1/-1 mixes to silence in every bucket (N=1,7,64); mixed mono is the arithmetic mean |
| Bucket count and coverage | A | single spike lands in exactly one of 10 buckets; buckets cover every frame with no gaps or overlaps |
| Empty input | A | empty input gives a non-null result with no buckets (0 frames, no channels, N=0, null data) |
| Fewer frames than buckets | A | fewer frames than buckets still yields N finite ordered buckets |
| NaN and Inf | A | NaN and Inf samples count as 0 |
| Determinism | A | identical input and N give bit-identical output |
| Mono identity (requirement text) | A | mono input is the channel itself |

### R-WD-2 Geometry
| Scenario | Tag | Evidence |
|---|---|---|
| x to position and back | A | geometry: x to position and back |
| Clamp outside view | A | geometry: clamps outside the view |
| Zero width | A | geometry: zero or negative width does not divide by zero |
| Jitter band clamped | A | geometry: jitter band is clamped to [0, 1] |
| Non-finite input | A | geometry: non-finite and out-of-range input stays finite in [0, 1] |

### R-WD-3 Delivery and lifecycle (Manual per spec; static evidence added)
| Scenario | Tag | Static evidence / status |
|---|---|---|
| Peaks arrive with sample | M pending | SampleLoader.cpp publishes post(..., std::move(peaks)); WaveformState covered by the loaded-result test |
| Stale load does not replace peaks | M pending, S | Aborted loads never post; results applied in commit order in onLoadResult before the label filter; core test results applied in commit order supersede earlier ones |
| Failed later load keeps waveform | M pending, S | Error posts use loaded=false, cleared=false, so apply() is a no-op; core test a failed later load keeps the current peaks |
| Restore missing clears | M pending, S | cleared=true only on the two accepted-clear posts (No sample, Missing: name), after commitGated succeeds; core test a cleared result empties the display |
| Rate change redelivers | M pending, S | Reload runs the same loadImpl publish path and posts peaks |
| Peaks failure still loads | M pending, S | null peaks still go through stage, commit and post(loaded=true); core null-peaks NoWaveform test and the alloc sweep return null |
| Late-opened editor | M pending, S | GrainsEditor constructor calls view_.setWaveform(proc_.waveform()) |
| Audio output unchanged | M pending, S | git diff 736a9a6 HEAD shows no change to SourceExchange, GranularEngine, ParamSpec or any core DSP file; the PluginProcessor.cpp diff is one hunk in onLoadResult (processBlock untouched). Peaks run on the loader thread |

### R-WD-4 Waveform view (Manual per spec; static evidence added)
| Scenario | Tag | Static evidence / status |
|---|---|---|
| Click jumps position | M pending, S | mouseDown calls setValueAsPartOfGesture(xToPosition(x, width)); position range 0..1 skew 1, so plain == normalised (ParamSpec) |
| Drag continues and clamps | M pending, S | mouseDrag uses the same path; xToPosition clamps (A: geometry clamp test) |
| Gestures for automation | M pending, S | beginGesture in mouseDown, endGesture in mouseUp guarded by dragging_ (one pair per press) |
| Jitter band read-only | M pending, S | jitterAttachment_ has a callback only and no write call exists; only positionAttachment_ is written |
| Marker follows automation | M pending, S | ParameterAttachment callbacks call repaint(); no Timer anywhere in Source/plugin (rg: no matches) |
| Empty hint | M pending, S | display Empty shows the Drop a sample hint |
| No timer, fixed size | M pending, S | no Timer; setResizable(false, false) |
| Editor open during load | M pending, S | changeListenerCallback pulls status and waveform on the message thread; superseded results still broadcast if the waveform changed |

### plugin-shell R-PS-6 (modified)
| Scenario | Tag | Evidence |
|---|---|---|
| 9 controls, load UI and waveform view | M pending, S | view_ added above generic_ (40 + 140 + generic); generic editor, Load button and status label unchanged |
| Parameter count unchanged | M pending, S | ParamSpec.h has no diff and 9 entries; the view uses apvts_.getParameter(id) and adds none |
| Fixed window size | M pending, S | setResizable(false, false) |

### Other checks
- R-BH-1 JUCE-free core: no JUCE include in Source/core (matches are comments and the CMake guard only); the guard ran during configure and build.
- No absolute local path, username or email in any of the 23 changed files (checked with rg -F for drive-letter paths, user-profile directories, the local account name, the author name and email: no matches).

### Pending user actions (Phase 6)
6.1: load, stale load, failed later load, missing-file restore, rate change, late-opened editor, optional bounce comparison. 6.2: click jump, drag clamp, DAW automation gesture, jitter band, marker follows automation, empty hint, fixed size, open-during-load. 6.3: nine controls, Load button, status label. Peaks failure is covered only by the core alloc sweep.

## Design coherence
| Decision | Result |
|---|---|
| D1 fixed 2048 buckets, columnPeak folding | OK |
| D2 compute from built buffer after isCurrent, before stage | OK |
| D3 noexcept, nullptr on alloc failure, empty gives non-null | OK (try/catch; tests and alloc sweep) |
| D4 Result.peaks and cleared, cleared only on accepted clear | OK (set after commitGated succeeds) |
| D5 apply in commit order before the label filter via WaveformState::apply | OK. The design Open Question is resolved: spec R-WD-3 now states commit order (see S1) |
| D6 reuse statusBroadcaster, no Timer | OK |
| D7 ParameterAttachment, jitter callback only | OK |
| D8 fixed size 40 + 140 + generic, setResizable(false, false) | OK |

## Issues
### CRITICAL
None.

### WARNING
- W1: apply-progress has no TDD Cycle Evidence table (prose only). PR 1 RED evidence cannot be re-verified from the artifact; only the current GREEN state was re-run.
- W2: PR 2 behaviour (loader peaks delivery, processor wiring, view gestures, hints) has no automated test; it rests on the pending Phase 6 manual checks. Not failed, by user decision, but the change is not fully runtime-proven until 6.1-6.3 are ticked.
- W3: Review size. Docs commit 67be0ea (462 added) plus core commit 4117411 (717 added, 1 deleted) is about 1180 lines for PR 1 if the docs ride in PR 1, over the 800 budget (the tasks.md forecast of about 480 undercounted tests). The core commit alone is 718; the plugin commit 2a16087 is 209 added, 22 deleted. Needs size:exception or the docs commit as its own PR.
- W4: Debug ctest in the shared multi-config build-plugin dir reports 251 (the Release test list), while the separate core dir build reports 246 for Debug. Debug alloc sweeps are compiled out by design, so the shared-dir Debug number is not a true Debug count. The authoritative Debug result is 246/246 from build.

### SUGGESTION
- S1: proposal.md still says peaks are generation-gated and the risk row says reuse generation gate; spec and code use commit order. Align the wording and close the design.md Open Question at archive.
- S2 (from PR 1): WaveformState::apply(bool loaded, bool cleared, p) takes two bool flags; a small enum (Loaded / Cleared / Failed) would remove the both-true case and the loaded-wins rule. Non-blocking.
- S3 (from PR 1): jitterBand calls finiteOr0(jitter) twice; bind a local once. Non-blocking.
- S4: mouseDown begins a gesture for any mouse button, so a right click also jumps position. If the editor is destroyed mid-drag, the destructor of ParameterAttachment does not end the gesture. Consider left-button only and ending the gesture in the view destructor when dragging_.
- S5: the editor dereferences the result of p.parameter("position") without a null check; ids are fixed so this is theoretical. An assert would document it.

## Final verdict
PASS WITH WARNINGS. Archive can proceed once the user accepts the pending manual checks (or ticks Phase 6).
