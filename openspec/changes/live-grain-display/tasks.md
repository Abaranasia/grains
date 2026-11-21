# Tasks: Live Grain Display (roadmap #3)

## Review Workload Forecast

| Field | Value |
|-------|-------|
| Review budget | 800 lines (additions + deletions) per PR |
| PR 1 (core + tests) | ~705 lines (88% of budget, ~95 lines of headroom) |
| PR 2 (plugin glue) | ~165 lines |
| Docs commit (openspec artifacts) | ~450 lines, documentation-only, separate commit |
| Budget verdict | No PR exceeds 800. PR 1 is the tightest: a >13% overrun of the test estimate would break the budget. If PR 1 passes ~800 during apply, split Phase 5 (GrainDisplayModel) into its own PR between PR 1 and PR 2. |
| Delivery strategy | chained, two stacked PRs |
| Chain strategy | stacked-to-main (PR 2 branches from PR 1; both target main in order) |
| Strict TDD | RED then GREEN per unit |

Docs are NOT counted in either PR. If the docs were folded into PR 1 it would be ~1155 lines and over budget, which is why they stay a separate documentation-only commit.

### Lines per phase

| Phase | PR | Code | Tests | Total |
|-------|----|------|-------|-------|
| 1 TripleBuffer | 1 | 80 | 150 | ~230 |
| 2 GrainSnapshot, pool iteration, snapshotGrains | 1 | 90 | 90 | ~180 |
| 3 Bit-identical render | 1 | 0 | 80 | ~80 |
| 4 Zero-alloc realtime test | 1 | 0 | 40 | ~40 |
| 5 GrainDisplayModel | 1 | 70 | 100 | ~170 |
| 6 CMake registration | 1 | 5 | 0 | ~5 |
| **PR 1 total** | | 245 | 460 | **~705** |
| 7 Processor mailbox and processBlock | 2 | 35 | 0 | ~35 |
| 8 WaveformView | 2 | 120 | 0 | ~120 |
| 9 Editor height | 2 | 10 | 0 | ~10 |
| 10 Manual verification | 2 | 0 | 0 | 0 |
| **PR 2 total** | | 165 | 0 | **~165** |

### Suggested Work Units

| Unit | Goal | Likely PR | Focused test command | Runtime harness | Rollback boundary |
|------|------|-----------|----------------------|-----------------|-------------------|
| 1 | Core mailbox, snapshot, engine reader, display model + tests | PR 1 | `ctest -R "triple_buffer\|grain_snapshot\|grain_display_model"` on `grains_tests`, plus `grains_realtime_tests` | N/A: JUCE-free, host-free | Core headers/sources, tests, CMake lists |
| 2 | Processor wiring, view, editor | PR 2 (base = PR 1 branch) | Full `grains_tests` + Release build of plugin | Standalone + DAW manual checklist (Phase 10) | Plugin files; core stays unused |
| 3 | Openspec docs | Separate commit | N/A | N/A | `openspec/changes/live-grain-display/` |

### Orchestrator parallelisation note

Do NOT parallelise any apply task. Single writer, strictly in order:

- Phase 1 before everything: Phases 2 and 7 depend on `TripleBuffer`, and 1.3 to 1.4 must follow 1.1 to 1.2 (RED then GREEN on the same files).
- Phase 2 before Phases 3, 4, 7: all use `GrainSnapshot` and `snapshotGrains`.
- Phase 5 touches `Source/core/CMakeLists.txt` and `tests/CMakeLists.txt`, the same files as Phases 1, 2 and 6. Concurrent edits there conflict.
- Phase 7 edits `PluginProcessor.h/.cpp`; Phase 8 depends on `pollGrains()` from Phase 7; Phase 9 depends on the view height from Phase 8.
- PR 2 cannot start until PR 1 is merged or at least pushed as the stack base.
- Phase 10 manual items need the PR 2 build; they cannot run in parallel with any code task.
- The only safe parallel work is reading or reviewing, never writing.

## Phase 1: TripleBuffer (R-WD-6)

- [x] 1.1 RED: `tests/test_triple_buffer.cpp` single-thread cases: initial `front()` is `T{}`; `update()` false before any publish; publish then update true then update false; latest of several publishes wins; the three indices stay pairwise distinct across many publish/update interleavings; `front()` is stable across later publishes until the next `update()`. Must fail to compile.
- [x] 1.2 GREEN: `Source/core/include/grains/TripleBuffer.h` header-only `TripleBuffer<T>` per design D4 (`back`, `publish`, `update`, `front`; 64-byte aligned slots and control words; `is_always_lock_free` static_assert; acq_rel exchange).
- [x] 1.3 RED: two-thread stress test in the same file: 100k publishes of a 512-byte struct whose words all equal the sequence number; consumer asserts every observed frame is internally consistent, sequence strictly increases, and the last frame is eventually seen. Bounded loops, no sleeps, no timing asserts. Runs against the 1.2 header (written to fail first with a deliberately torn publish if a true RED is wanted, then fixed).
- [x] 1.4 GREEN: stress test passes reliably (run it repeatedly, for example 20 times, before moving on). Fix the header only if it fails.

## Phase 2: GrainSnapshot, pool iteration, snapshotGrains (R-WD-5, R-GR-11)

- [x] 2.1 RED: `tests/test_grain_snapshot.cpp`: count equals `activeGrainCount()`; marks normalised and finite (`position`, `progress`, `level` in [0, 1], no NaN); end-of-buffer clamp; slots distinct and in index order and stable across blocks; null source and empty source give count 0; density 0 gives count 0; saturated pool (128); `version == kVersion` (1); empty after `setSource` and after `prepare`; `static_assert(sizeof(GrainMark) == 16)`; method is `const noexcept` (compile-time check). Must fail to compile.
- [x] 2.2 GREEN: `Source/core/include/grains/GrainSnapshot.h` (`GrainMark`, `GrainSnapshot`, `kVersion`, 128 marks).
- [x] 2.3 GREEN: `GrainPool::forEachActiveSlot(F) const` calling `f(int slot, const Grain&)` in index order (`GrainPool.h`).
- [x] 2.4 GREEN: `GranularEngine::snapshotGrains(GrainSnapshot&) const noexcept` in `GranularEngine.h` and `src/GranularEngine.cpp` per the Engine snapshot rules (D3); `level = window_.at(progress)`.
- [x] 2.5 Gate: `grains_tests` green for existing and new snapshot tests.

## Phase 3: Bit-identical render (R-GR-11)

- [x] 3.1 RED: in `test_grain_snapshot.cpp` add engine A (render only) vs engine B (render and `snapshotGrains` after every block): same seed, same params, 400 blocks, compare outputs bitwise (`memcmp`) and compare `activeGrainCount()` per block.
- [x] 3.2 RED: add "snapshot does not alter pool or RNG": call `snapshotGrains` repeatedly between blocks and confirm subsequent output equals the no-snapshot run.
- [x] 3.3 GREEN: both pass against the Phase 2 implementation (they are expected to pass immediately because the method is a pure const reader; if one fails, fix the engine, never the test).

## Phase 4: Zero-alloc realtime test (R-GR-11, R-PS-5, R-WD-6)

- [x] 4.1 RED: in `tests/test_engine_realtime.cpp` (`grains_realtime_tests`), inside an `AllocationScope`: `process`, `snapshotGrains`, mailbox `back()`, `publish`, `update`; assert 0 allocations and `snapshot.count == engine.activeGrainCount()`.
- [x] 4.2 GREEN: passes using the existing allocation counter; fix any hidden allocation in the new code if it fails.

## Phase 5: GrainDisplayModel (R-WD-7)

- [x] 5.1 RED: `tests/test_grain_display_model.cpp`: fresh frame displays and returns true when old or new count > 0; marks held for fewer than `kStaleTicks` idle ticks (7 ticks no repaint); the 8th idle tick (about 267 ms) clears and returns true once, later idle ticks return false; empty-to-empty fresh frame returns false; fresh frame resumes after stale; `reset()` clears marks; `kPollHz == 30`, `kStaleMs == 250`, `kStaleTicks == 8` and the constants are distinct. Must fail to compile.
- [x] 5.2 GREEN: `Source/core/include/grains/GrainDisplayModel.h` and `src/GrainDisplayModel.cpp`: `tick(const GrainSnapshot*)`, `reset()`, `shown()`, constants per D6.

## Phase 6: CMake registration (PR 1 gate)

- [x] 6.1 Register `GrainDisplayModel.cpp` in `Source/core/CMakeLists.txt` and the three new test files in `tests/CMakeLists.txt` (added incrementally as each RED step needs them; this task is the final check that nothing is unregistered).
- [ ] 6.2 Gate: `grains_tests`, `grains_realtime_tests` and the JUCE-free core check (R-BH-1) green; `git diff` confirms `render` output path unchanged. Open PR 1.

## Phase 7: Processor owns the mailbox (R-PS-4, R-PS-5, R-WD-6)

Base: PR 1 branch. No automated test exists for the processor (JUCE-dependent); correctness is guarded by the Phase 3 and 4 core tests plus Phase 10.

- [x] 7.1 `Source/plugin/PluginProcessor.h/.cpp`: add `grains::TripleBuffer<grains::GrainSnapshot> grainMailbox_`, private `publishGrains()` (`engine_.snapshotGrains(grainMailbox_.back()); grainMailbox_.publish();`), public `const grains::GrainSnapshot* pollGrains() noexcept` (returns `&front()` when `update()` is true, else nullptr).
- [x] 7.2 `processBlock`: restructure the two early returns into one if / else-if / else chain (stereo; mono with unprepared or null scratch clears the buffer; mono chunk loop unchanged) followed by a single `publishGrains()`. The `getNumChannels() <= 0` return stays before the chain. Engine calls and order unchanged.
- [x] 7.3 `prepareToPlay`: call `publishGrains()` once after `engine_.prepare` so the empty snapshot clears markers.
- [x] 7.4 Diff check: `SourceExchange`, `GranularEngine` render path and parameter handling untouched; Release plugin build clean.

## Phase 8: WaveformView (R-WD-4, R-WD-7)

- [x] 8.1 `Source/plugin/WaveformView.h/.cpp`: add `std::function<const grains::GrainSnapshot*()> pollGrains` setter and a `GrainDisplayModel` member.
- [x] 8.2 Timer lifecycle: `startTimerHz(kPollHz)` only while `isShowing()`, display is `Waveform` and a poll function is set; re-evaluate in `visibilityChanged`, `parentHierarchyChanged` and `setWaveform`; on stop call `model.reset()`. `timerCallback` calls `model.tick(poll())` and repaints only the strip bounds when it returns true.
- [x] 8.3 Cached waveform image (background and peaks, or hint) rebuilt lazily in `paint` after `setWaveform` or `resized` or a physical pixel scale change; paint order: image, jitter band, dots, marker. Band and marker span the full view height (188 px).
- [x] 8.4 Lane-dot strip: 48 px under the 140 px waveform, `kLaneRows = 8`, `row = slot % 8`, row pitch 6 px, dot diameter 4 px, x from `position`, alpha `0.25 + 0.75 * level`, mint `0xff7ee0a1`. Draw no dots in the Empty and NoWaveform states.

## Phase 9: Editor (R-WD-4)

- [x] 9.1 `Source/plugin/PluginEditor.h/.cpp`: view height 140 to 188 (editor fixed size grows by 48 px), assign the view's poll function to `proc_.pollGrains()` once in the constructor. Release build clean. Open PR 2.

## Phase 10: Manual verification (Standalone and DAW)

Run in the Standalone (build-plugin Release `Grains.exe`) and a DAW with the VST3. Use a short WAV with a clear transient plus a long one. Tick each sub-item as it passes.

- [ ] 10.1 Audio A/B (R-GR-11, R-PS-4)
  - [ ] Mono: render a bounce with the PR 1 base build and with PR 2 build, same seed, same parameters; bit-for-bit identical or null test result is silence.
  - [ ] Stereo: same comparison on a stereo host layout.
- [ ] 10.2 Lane dots (R-WD-7)
  - [ ] Dots appear in the strip under the waveform and line up with the position marker; rows are used by slot.
  - [ ] Geometry: strip is 48 px tall under the 140 px waveform (editor 48 px taller than before), 8 rows at 6 px pitch, 4 px mint dots, newer (level 0) dots dimmer than mid-life ones; band and marker span the full 188 px.
  - [ ] No dots in the Empty ("Drop a sample") and NoWaveform states.
  - [ ] Host stopped: with the transport stopped, markers clear after about 267 ms.
  - [ ] Idle with a loaded sample and no grains: no repaint churn (no visible flicker).
- [ ] 10.3 Timer lifecycle (R-WD-4)
  - [ ] Minimised window: timer stops (markers reset), resumes on restore.
  - [ ] Editor closed and reopened: no crash, view resumes.
  - [ ] Editor open during source swap (load a new file and restore state): view updates, no crash, no stale dots from the old source.
- [ ] 10.4 Performance and layout
  - [ ] Multi-instance CPU: 4 or more instances with editors open; compare CPU against the pre-change baseline, no significant increase.
  - [ ] Resize and DPI: move the window between displays with different scaling; the cached image rebuilds and dots stay crisp; the window remains non-resizable.
  - [ ] Large host blocks (over about 233 ms, around 10k samples at 44.1 kHz): flicker is possible and accepted (documented in design).
- [ ] 10.5 Regression (R-PS-6): the 9 generic parameter controls, Load button, status label and waveform click/drag from the previous change still work.

Spec sync not needed until archive.
