# Verify Report: sample-loader

Branch feat/sample-loader, HEAD cb493a0 (3b50ffb core, 63756b9 plugin, cb493a0 docs). Strict TDD. Mode: hybrid.

## Verdict: PASS WITH WARNINGS (0 CRITICAL, 2 WARNING, 5 SUGGESTION)

## Execution evidence
- Fresh build `build/verify-sl` (MSVC Debug, plugin OFF): success, no errors.
- `ctest`: 111/111 passed (expected 111).
- `cmake --build build-plugin --target grains_plugin --config Debug`: success (VST3 + Standalone linked).
- Tasks: all of 1.1-1.13, 2.1-2.8, 3.1 marked [x] and consistent with code and commits.

## Spec compliance matrix
| Req | Evidence | Result |
|---|---|---|
| R-SS-1 latest wins, at most once | test_source_exchange.cpp cases + 2-thread stress | OK |
| R-SS-2 freed exactly once | Tracked counters, destructor test, publish/collect tests | OK |
| R-SS-3 RT-safe consumer | realtime tests (count()==0, frees()==0 over acquire+setSource+50 blocks, clear case); acquire at top of processBlock | OK |
| R-SS-4 channels | test_sample_builder.cpp mono/stereo/6ch | OK |
| R-SS-5 cap | 10 Hz 3001 -> 3000 truncated; 3000 not truncated | OK (floor/INT_MAX branches untested, S2) |
| R-SS-6 rejection | table-driven NoChannels/NoFrames/BadRate/Mismatch, noexcept | OK |
| R-SS-7 loader | SampleLoader.cpp: background thread, generation check, builder, publish, no empty publish, alive flag, stop() in dtor; manual items 1,4,5,6,9 | OK with W1 (narrow race) |
| R-SS-8 path persistence | samplePath property on apvts state, Restore request async; manual item 7 | OK |
| R-SS-9 missing file | Restore failure publish(nullptr), "Missing: name", path kept; manual item 8 | OK |
| R-SS-10 editor UI | Load button, drag-drop with ext filter, label; manual items 1,2,3,4 | OK |
| R-PS-4 (mod) | acquire/setSource-on-change, silence w/o sample, peak<=4; manual items 10,11,12 | OK |
| R-PS-6 (mod) | GenericAudioProcessorEditor + loader UI; manual | OK |
| R-PS-7 (mod) | state includes path, async reload, bad state ignored; manual item 7 | OK |

Manual evidence: user confirmed all 12 items of task 2.8 in Debug Standalone on 2026-10-24. VST3-in-DAW checks waived (plugin-target follow-up).

## Findings

### WARNING
- W1 (R3-P1) Race in SampleLoader::load between `isCurrent()` and `exchange_.publish(...)`: a request superseded in that window can still publish. Assessment: this narrowly violates the letter of R-SS-7 ("earlier unfinished requests MUST NOT publish"). Impact is bounded: the loader is a single thread processing serially, so the newer request publishes afterwards and wins in the normal case. Problematic only if the newer USER load then fails: the stale source stays active while onLoadResult drops the stale generation, so the stored path/label refer to the previous file (state/audio mismatch). Not CRITICAL (window of microseconds, no memory-safety or RT impact, manual test 5 passes). Follow-up: re-check generation under the same lock as publish, or make publish conditional on generation.
- W2 `stopThread(4000)` in `SampleLoader::stop()` can force-kill the thread if a slow read/decode of a chunk blocks beyond 4 s (network drive), risking leaks or torn state at shutdown. Follow-up: longer/unbounded wait or cancellable reads.

### SUGGESTION
- S1 `frees()` test assertions could be elided by an optimizing compiler (Release); make the frees path observable (volatile/escape).
- S2 `maxSourceFrames` floor and INT_MAX clamp branches have no tests.
- S3 Collect-after-publish rule (`publish; collect;`) documented only in a class comment; consider a helper that enforces it.
- S4 Unused `<cmath>` include in tests/test_sample_builder.cpp.
- S5 Add rationale comment for `kMaxSourceSeconds` (5 min cap).

## Delivery notes
Single PR, size:exception approved by user (~1010 changed code+test lines vs 800 budget; forecast in tasks.md of ~730 is stale). No juce includes in Source/core (gate 1.13).

## Recommendation
sdd-archive (no blockers). Track W1, W2 and S1-S5 as follow-ups.
