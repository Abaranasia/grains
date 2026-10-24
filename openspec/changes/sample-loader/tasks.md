# Tasks: sample-loader

Specs: `specs/sample-source` (R-SS-1..10), `specs/plugin-shell` (R-PS-4/6/7). Design: `design.md`. Strict TDD: each core task is RED (failing test first, record how it fails) then GREEN.

Commands (build dir `build`, plugin OFF for core):
- Build: `cmake --build build --config Debug`
- Test: `ctest --test-dir build -C Debug --output-on-failure`
- Plugin: `cmake --build build-plugin --target grains_plugin --config Debug`

## Review Workload Forecast

| Field | Value |
|-------|-------|
| Estimated changed lines | ~730 (core ~415, plugin ~315), docs separate |
| 400-line budget risk | High |
| Chained PRs recommended | Yes |
| Suggested split | Single PR, 2 commits (core, plugin) |
| Delivery strategy | single-pr |
| Chain strategy | size-exception |

Decision needed before apply: No (user approved one PR; size:exception recorded, 730 < 800 budget)
Chained PRs recommended: Yes
Chain strategy: size-exception
400-line budget risk: High

Shrink order if over: stress test ~35 lines, table-driven builder cases, whole-file read, minimal editor.

### Suggested Work Units

| Unit | Goal | Likely PR | Focused test command | Runtime harness | Rollback boundary |
|------|------|-----------|----------------------|-----------------|-------------------|
| 1 | Core exchange, builder, tests | PR 1 (commit 1) | `ctest --test-dir build -C Debug --output-on-failure -R "exchange|builder|realtime"` | Catch2 stress and realtime tests | Revert commit 1 (plugin commit depends on it) |
| 2 | Plugin loader, wiring, editor | PR 1 (commit 2) | `cmake --build build-plugin --target grains_plugin --config Debug` | Manual Standalone task 2.8 | Revert commit 2 only |

## Commit 1: `feat(core): add SourceExchange and sample builder`

Tasks 1.1 to 1.12 are sequential except 1.1/1.2 (different files). Baseline tests must stay green after each task.

### Phase 1: Test support
- [x] 1.1 RED: in `tests/test_engine_realtime.cpp` (or a scratch test) call `AllocationScope::frees()`; record compile failure (member missing).
- [x] 1.2 GREEN: add `gFrees` and `AllocationScope::frees()` in `tests/support/AllocationCounter.{h,cpp}`, incremented in `plainFree`/`alignedFree` for non-null pointers while a scope is active. Build green, baseline passes. (R-SS-3)

### Phase 2: SourceExchange
- [x] 1.3 RED: create `tests/test_source_exchange.cpp` with `Tracked` type (live, destruction and thread-id counters) and cases: empty acquire, publish then acquire, second acquire unchanged (R-SS-1); register in `tests/CMakeLists.txt` (+`find_package(Threads)`, link). Fails: `grains/SourceExchange.h` missing.
- [x] 1.4 GREEN: create `Source/core/include/grains/SourceExchange.h` per design contract (pending_/retired_ atomics, Box, publish, collect, acquire, dtor, lock-free static_assert). Cases pass.
- [x] 1.5 RED: add cases publish A,B so A is freed on publish and B delivered (R-SS-1, R-SS-2); swap keeps A alive until `collect`; clear returns `{nullptr, changed}`; record failures against any missing behavior (characterization if already passing).
- [x] 1.6 RED/GREEN: destructor test `publish(A); acquire; publish(B); acquire; publish(C); destroy` gives `live == 0` and exact counts at each step (R-SS-2). Fix header only if it fails.
- [x] 1.7 RED/GREEN: 2-thread stress test (ids 0..4999, every 7th a clear, last non-null; consumer 10 s deadline sets `done`): ids strictly increasing, last id seen, `live == 0`, `consumerFrees == 0` (R-SS-3).
- [x] 1.8 RED/GREEN: realtime test in `tests/test_engine_realtime.cpp`: before the scope `publish(A); acquire; publish(B)`; inside `AllocationScope` acquire + `setSource` + 50 blocks gives `count()==0` and `frees()==0`; second case with `publish(nullptr)` clear (R-SS-3).
- [x] 1.9 RED/GREEN: engine swap test in `tests/test_source_exchange.cpp`: noise A active > 0, acquire zero buffer B gives `activeGrainCount()==0` and exact zeros; clear gives zeros.

### Phase 3: Sample builder
- [x] 1.10 RED: create `tests/test_sample_builder.cpp` (register in CMake): mono/stereo unchanged, 6 ch gives ch 0,1 (R-SS-4); rate 10 Hz 3001 frames gives 3000 + truncated, 3000 full (R-SS-5). Fails: `grains/SampleBuilder.h` missing.
- [x] 1.11 GREEN: create `Source/core/include/grains/SampleBuilder.h` and `Source/core/src/SampleBuilder.cpp` (+1 line `Source/core/CMakeLists.txt`): `maxSourceFrames`, `buildSampleBuffer` noexcept, `bad_alloc` as OutOfMemory.
- [x] 1.12 RED/GREEN: table-driven rejection cases NoChannels (0 ch, null data), NoFrames, BadSampleRate (0, -44100, NaN, Inf), MismatchedLengths (R-SS-6); no throw.

### Phase 4: Gate
- [ ] 1.13 Commit gate (checks done: Debug build zero warnings, ctest 111/111, no juce includes in Source/core; COMMIT PENDING, not done by apply): full Debug build with zero warnings (`/WX`), `ctest` all green; `rg -n "juce" Source/core` has no include matches. Commit `feat(core): add SourceExchange and sample builder` (no AI attribution).

## Commit 2: `feat(plugin): add sample loader, state restore and editor`

JUCE glue has no unit tests; RED is a failing build or verification step. Tasks 2.1 to 2.5 are sequential; 2.6 depends on 2.3.

- [ ] 2.1 `Source/plugin/SampleLoader.{h,cpp}` + `Source/plugin/CMakeLists.txt`: `juce::Thread`, `request(File, Origin)`, generation counter, chunked read (65536), build, `publish; collect;`, Restore failure `publish(nullptr)` "Missing: name", `alive` flag, `stop()`. (R-SS-7, R-SS-9) Verify: plugin build.
- [ ] 2.2 `Source/plugin/PluginProcessor.{h,cpp}`: members in order `apvts_, raw_, exchange_, engine_, scratch_, status_, broadcaster_, loader_`; `~GrainsAudioProcessor(){ loader_.stop(); }`; `onLoadResult` drops stale generation, sets status, `samplePath` property on User success. (R-SS-7, R-SS-8)
- [ ] 2.3 `processBlock`: `acquire()` at top after `ScopedNoDenormals`, before guards; `setSource` only when `changed`. (R-PS-4, R-SS-3)
- [ ] 2.4 `setStateInformation`: request Restore only in valid-XML + tag-match branch after `replaceState`; empty path clears; non-absolute path treated as missing (`juce::File::isAbsolutePath`). (R-PS-7, R-SS-8, R-SS-9)
- [ ] 2.5 `Source/plugin/PluginEditor.{h,cpp}`: `GenericAudioProcessorEditor` + Load button (`FileChooser` `*.wav;*.aiff;*.aif`, SafePointer) + status label, `FileDragAndDropTarget` (case-insensitive ext, ignore others), `ChangeListener` removed in dtor. (R-PS-6, R-SS-10)
- [ ] 2.6 Automated verification: plugin OFF build + `ctest` still green; `cmake --build build-plugin --target grains_plugin --config Debug` succeeds with no new warnings.
- [ ] 2.7 Commit gate: 2.6 passes. Commit `feat(plugin): add sample loader, state restore and editor`.

### 2.8 Manual verification in Standalone (human required) - PENDING USER

- [ ] 2.8 MANUAL (pending user): (1) Load button loads wav, label shows name; (2) drag aiff loads; (3) drop .txt ignored; (4) replace while playing, no crash; (5) two quick loads, second wins; (6) corrupt file keeps previous source, error status; (7) state round trip restores path and 9 params, reloads async; (8) moved file gives silence, "Missing: name", path kept on re-save; (9) close during large load, no crash or hang; (10) peak <= 4; (11) 44.1/48/96 kHz and mono layout; (12) silence with no sample for 30 s.

## Commit 3: docs (only if needed)

- [ ] 3.1 Add a short README/build note on loading samples and the absolute-path state caveat, only if existing docs reference the silent shell. Commit `docs: describe sample loader`. Skip otherwise.

## Requirement Traceability

| Requirement | Tasks |
|---|---|
| R-SS-1 publish/acquire, latest wins | 1.3, 1.4, 1.5 |
| R-SS-2 ownership, freed once | 1.5, 1.6 |
| R-SS-3 RT-safe consumer | 1.2, 1.7, 1.8, 1.9, 2.3 |
| R-SS-4 channels | 1.10, 1.11 |
| R-SS-5 length cap | 1.10, 1.11 |
| R-SS-6 rejection | 1.12 |
| R-SS-7 loader behaviour | 2.1, 2.2, 2.8 |
| R-SS-8 path persistence | 2.2, 2.4, 2.8 |
| R-SS-9 missing file | 2.1, 2.4, 2.8 |
| R-SS-10 editor load UI | 2.5, 2.8 |
| R-PS-4 processing and output | 1.8, 1.9, 2.3, 2.8 |
| R-PS-6 editor | 2.5, 2.8 |
| R-PS-7 state persistence | 2.4, 2.8 |
