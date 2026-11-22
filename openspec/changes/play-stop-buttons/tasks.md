# Tasks: Play and Stop Buttons

Change: `play-stop-buttons`. Strict TDD is on: every implementation task is RED (write the failing test, see it fail for the right reason) then GREEN (minimal code), then refactor. Tests land in the same task as the code they drive.

## Conventions

- **Naming**: code uses the design's names, `TransportGate::State { Stopped, Starting, Playing, Stopping }`. The `transport-gate` delta spec uses the same names (Starting = fade-in ramp, Stopping = fade-out ramp; see its naming note), so code, tests and specs agree.
- **Spec refs**: R-TG-n are in `specs/transport-gate`. R-PS-4 and R-PS-6 are in `specs/plugin-shell`. R-WD-7 is in `specs/waveform-display`. Design decisions are D1 to D10. R-GR-6 and R-GR-10 come from `grain-rendering`.
- **Line counts** are approximate and include test code. Total is about 585 code lines in one PR, under the 800-line review budget. Spec files are a separate docs commit and do not count.
- **Parallelism**: tasks in the same phase marked `[P]` can run in parallel. Others are sequential. Phases are strictly ordered unless noted.

## Prerequisites (branch and base)

- [x] **T0. Branch base check** (0 lines, gate before T1)
  - The base MUST contain commit `85ab354` (`feat/live-grain-view`, owns `publishGrains`, `snapshotGrains` and the processBlock if/else chain). Current branch `feat/waveform-core` does not guarantee it. Verify with `git merge-base --is-ancestor 85ab354 HEAD`. If it is missing, base on `feat/live-grain-view` or on main after live-grain-display merges.
  - The core edits must be written against the `snapshotGrains` version of `GranularEngine`.
  - The R-WD-7 and R-PS-4 text in the delta specs is reconstructed. Re-apply it onto the merged wording before the spec sync (see Phase 7).
  - Refs: design Migration / Rollout; plugin-shell delta header; waveform-display delta header.

## Phase 1. Core `TransportGate` (JUCE-free, header-only)

- [x] **T1.1 Gate skeleton, default state, ramp length (RED then GREEN)** (~50 lines test, ~35 code)
  - Files: create `Source/core/include/grains/TransportGate.h`, `tests/test_transport_gate.cpp`; modify `tests/CMakeLists.txt` (+3).
  - RED: tests for default state (`Stopped`, `gain() == 0.0f`), `rampLength() == max(1, lround(0.020 * sr))` at 44.1, 48 and 96 kHz, `prepare` resets to Stopped and recomputes `len`, `prepare` discards an in-progress ramp, and `kRampSeconds == 0.020`. A `len` of 1 corner case at a tiny rate.
  - GREEN: class with `prepare`, `forcePlaying`, `forceStopped`, `state`, `steady`, `stopped`, `rampLength`, `gain`, integer `pos_` in `[0, len_]` (D1).
  - Refs: R-TG-1 (default state, ramp length), R-TG-8, R-TG-9 (`noexcept`, no allocation).

- [x] **T1.2 Fade-in and fade-out sequences (RED then GREEN)** (~45 test, ~30 code)
  - Files: `tests/test_transport_gate.cpp`, `TransportGate.h`.
  - RED: `request(true)` from Stopped returns true and moves to `Starting`. The first gain is exactly 0.0. Gain reaches exactly 1.0 after `len` and the state becomes `Playing` (apply `len + 1` samples). Fade-out is monotonic from 1.0 to exactly 0.0, ending `Stopped`. Every step is at most `1/len` plus 1 ulp. Sample rule: `Starting` multiplies by `pos/len` then increments, `Stopping` decrements then multiplies. A zero gain writes `+0.0f` (bitwise). A request while already `Playing` or `Starting` returns false (no restart).
  - GREEN: `request` and `apply(l, r, n)`, including the `steady()` skip while `Playing` (D2).
  - Refs: R-TG-1 (endpoints), R-TG-2 (gate not modifying steady signal), R-TG-3 (+0 zeros), R-TG-9.

- [x] **T1.3 Reversal and latch (RED then GREEN)** (~30 test, ~15 code)
  - Files: `tests/test_transport_gate.cpp`, `TransportGate.h`.
  - RED: Stop mid fade-in reverses from the current `pos_` with a step at most `1/len` (D4). Play mid fade-out does not reverse: it stays `Stopping` until 0 and the next `request` after completion returns true to signal a restart (D3). Stop while `Stopped` is a no-op.
  - GREEN: transition table in `request`/`apply`, with the latch for a Play during `Stopping`.
  - Refs: R-TG-1 (reversal), R-TG-4 (latch).
  - Depends on: T1.2.

## Phase 2. Engine integration

- [x] **T2.1 `InitialTransport`, `prepare` overload, and updating existing call sites** (~30 lines across ~30 existing sites, plus ~5 new)
  - Files: `Source/core/include/grains/GranularEngine.h`, `src/GranularEngine.cpp` (enum, new `prepare` signature, store `seed_`, hold a `TransportGate gate_`); modify `tests/test_engine_*.cpp`, `tests/test_grain_render*.cpp`, `tests/test_soft_limiter*.cpp`, `tests/test_engine_realtime.cpp` fixtures.
  - RED: first add the one-line `InitialTransport::Playing` argument at every existing `prepare(...)` call that expects sound, so the build fails to compile against the old signature. Assertions MUST NOT change (D6, success criterion "Playing output bit-identical to today").
  - GREEN: add `enum class InitialTransport { Stopped, Playing }`; `prepare(sr, seed, InitialTransport = Stopped)` calls `gate_.prepare(sr)`, `forcePlaying()` if `Playing`, and clears pool and scheduler as today. Default is Stopped. The full existing suite MUST pass unchanged. A sweep (`rg "\.prepare\("` in `tests/`) confirms that none of the sites is missed.
  - Refs: R-TG-1 (explicit initial Playing; prepare defaults to Stopped), R-TG-2, R-TG-8.
  - Depends on: T1.1. Blocks everything in Phase 2 and 3.

- [x] **T2.2 `setPlaying`, level-triggered target, restart and stop-complete reset (RED then GREEN)** (~70 test, ~30 code)
  - Files: create `tests/test_engine_transport.cpp` (+ register in `tests/CMakeLists.txt`); modify `GranularEngine.h/.cpp`.
  - RED, in `test_engine_transport.cpp`: (a) Prepare defaults to Stopped, so the output is exact zeros before any Play. (b) Stopped engine: rng state unchanged and zeros over many blocks, with no scheduling (R-TG-5). (c) Play from Stopped: the first sample is exactly 0, ramp length matches, and no source means output stays zeros and the ramp does not advance. (d) Stop: after the ramp every sample is bitwise `+0`, active count is 0, and the pool and scheduler reset exactly once for that Stop. (e) Redundant Play while Playing/Starting: output bit-identical to the run without it, with no reseed. (f) Play during Stopping: fade completes, then a fresh restart on the next call.
  - GREEN: `setPlaying(bool)` sets the target. `process()` evaluates the target once per call. On a Stopped-to-Starting transition at the start of the call it does `rng_.seed(seed_)`, `pool_.reset()`, `scheduler_.reset()`. When Stopped it returns early before scheduling and zero-fills. At the end of a call in which the gate reached Stopped it does `pool_.reset()`, `scheduler_.reset()` (D5). Gate applies to the raw sum before the `softLimit` loop (D2). `publishGrains` / `snapshotGrains` MUST still be called every block, including when stopped (empty after reset). Mono-path chunking sees the same target and restarts only once (design Data Flow).
  - Refs: R-TG-1, R-TG-3, R-TG-4, R-TG-5, R-TG-6, R-TG-9.
  - Depends on: T1.3, T2.1.

- [x] **T2.3 Steady bit-identity, deterministic restart, bounds (RED then GREEN, likely already green)** (~55 test, ~0 to 5 code)
  - Files: `tests/test_engine_transport.cpp`.
  - Tests: (a) Engine A `prepare(Playing)` vs engine B `prepare(Stopped)` + Play: bitwise equal for `i >= len`, and `abs(B) <= abs(A)` before that (R-TG-2). (b) Play, Stop, Play after arbitrary history equals the first Play bitwise for the first N samples, including the Play latched during `Stopping` (R-TG-4). (c) Every sample satisfies `abs(y) < 1` and is finite through repeated Play/Stop with the DC worst case (density 200, size 1000, gain 2) (R-TG-6).
  - If any test is green on first run, state it in the apply notes, then break the code deliberately (mutation check) to prove the test can fail.
  - Refs: R-TG-2, R-TG-4, R-TG-6.
  - Depends on: T2.2.

- [x] **T2.4 `stopNow` and `setSource` neutrality (RED then GREEN)** (~25 test, ~10 code)
  - Files: `tests/test_engine_transport.cpp`; `GranularEngine.h/.cpp`.
  - RED: from Playing, `stopNow()` then the next block is entirely `+0`, active count 0, `transportState() == Stopped`, target false. `setSource` on a Playing engine leaves `transportState()` unchanged (D7). `transportState()` accessor exists.
  - GREEN: `stopNow()` does `gate_.forceStopped()`, clear target, `pool_.reset()`, `scheduler_.reset()`. Add `transportState()`.
  - Refs: R-TG-10.
  - Depends on: T2.2. `[P]` with T2.3.

## Phase 3. Realtime and block-split tests

- [x] **T3.1 Zero allocation across toggles** (~25 lines)
  - Files: modify `tests/test_engine_realtime.cpp`.
  - RED: add Play, Stop, Play (including mid-fade requests and `stopNow`) inside the existing `AllocationScope`; assert 0 allocations. If any allocation appears, fix it in the gate or engine (GREEN).
  - Refs: R-TG-9.
  - Depends on: T2.4.

- [x] **T3.2 Block-split invariance across a toggle (R-GR-6)** (~40 lines)
  - Files: `tests/test_engine_transport.cpp` (or alongside the existing R-GR-6 split test if it is in another file, but then reuse its helper).
  - Test: two block partitions of the same total length, Play at sample 0 and Stop at absolute sample S in both, with block boundaries at S in both partitions; outputs bitwise equal. Add the mono-chunked path case if the helper supports it.
  - Note: toggles are block-quantised (R-TG-7). The test must apply the toggle at the same absolute sample in both runs, never inside a block.
  - Refs: R-TG-7, `grain-rendering` R-GR-6.
  - Depends on: T2.3. `[P]` with T3.1.

## Phase 4. Plugin processor glue

Note: the design's testing strategy is unit layer only (Catch2 on `grains_core`). Check at the start of this phase whether any test target can link the processor. If it cannot, the spec scenarios tagged `[Catch2/Manual]` for the processor (default Stopped after prepare/restore, block-quantised toggle at processor level, new sample stops playback) are covered by the engine tests in Phase 2 plus the manual phase, and this is recorded in the apply notes. Do not add a JUCE test harness in this change.

- [x] **T4.1 `playing_` atomic, accessors, and processBlock wiring** (~35 lines)
  - Files: `Source/plugin/PluginProcessor.h/.cpp` (confirm actual paths, the proposal says `Source/PluginProcessor.*`).
  - Test-first: if a linkable test target exists, write the failing test first (a fresh processor `isPlaying() == false`; `setPlaying(true)` then `prepareToPlay` clears it). Otherwise the RED step is a compile-level change: first add the call sites in the editor test or stub, then implement. Record which was used.
  - Implementation (D8): `std::atomic<bool> playing_{false}`. Message thread: `setPlaying(bool)`, `isPlaying()`. In `prepareToPlay`: `engine_.prepare(sr, seed)` (default Stopped) and `playing_ = false`. In `processBlock`: when `SourceExchange` yields a changed source, `setSource`, `engine_.stopNow()`, `playing_ = false`; then one relaxed load of `playing_`, `engine_.setPlaying(...)`, then the existing parameter mapping and render. Reset `playing_` in `setStateInformation` and `onLoadResult`. Publish the snapshot every block, also while stopped (already done in `85ab354`; verify it is not skipped when the engine early-returns). `sampleLoaded` stays true after a failed load and the state stays Stopped.
  - Not persisted: no APVTS parameter and no ValueTree property. R-PS-3 (9 parameters) and R-PS-7 unchanged.
  - Refs: R-PS-4, R-TG-10 (use of `stopNow`), R-TG-8.
  - Depends on: T2.4. Branch base must contain `85ab354` (T0).

## Phase 5. Editor

- [x] **T5.1 Play/Stop buttons, radio group, layout, and 10 Hz sync** (~55 lines)
  - Files: `Source/plugin/PluginEditor.h/.cpp` (confirm actual paths).
  - Test-first: there is no unit test layer for the editor (JUCE component). RED is a manual-checklist entry written before coding (see Phase 6), and the build must compile with the processor API from T4.1 first. Keep the sync logic in one small function that decides "needs update" so it can be reviewed for the on-change-only rule.
  - Implementation (D9, R-PS-6): top bar 40 px in order Load (100 px), Play (about 64 px), Stop (about 64 px), status label. Two `TextButton`s with `setClickingTogglesState(true)` and one radio group id. Both disabled until a sample has loaded. Play calls `proc.setPlaying(true)`, Stop calls `proc.setPlaying(false)`. `juce::Timer` at `kTransportPollHz = 10`, plus the existing change callback syncing immediately. Toggle and enabled states are set only when they differ and always with `dontSendNotification`, so a sync never triggers a Play/Stop request. After every load or restore Stop is shown selected. No keyboard shortcuts, no pause or other transport controls. Stop the timer in the destructor.
  - Refs: R-PS-6, R-PS-4 (editor resync within 100 ms).
  - Depends on: T4.1.

## Phase 6. Manual verification (carry as pending, not run in apply)

These cannot be automated in the unit layer. Carry them as pending items into verify and the archive report.

- [ ] **M1. Buttons**: disabled before any load; enabled with Stop selected after a successful load; radio behaviour (exactly one selected, audio follows); failed load with no sample keeps both disabled; failed load or restore with a sample loaded keeps the old sample and enabled buttons, still Stopped. Refs: R-PS-6.
- [ ] **M2. Audio**: silent after load until Play; Play fades in (about 20 ms, no click, peak at most 1); Stop fades to exact silence with idle-level CPU; a new sample load while Playing hard-stops with no fade and stays silent until Play. Refs: R-PS-4, R-TG-3, R-TG-10.
- [ ] **M3. DAW `prepareToPlay` mid-session**: change sample rate or buffer size, or bypass and offline-render setup, while Playing. Plugin returns to Stopped, output is zeros, editor shows Stop selected within 100 ms, and sound resumes only after a new Play. Confirms the open question on host prepare. Refs: R-PS-4, R-TG-8.
- [ ] **M4. Display dots clear when stopped**: with the lane-dot view visible, press Stop; dots clear after the fade-out and the empty snapshot are polled (and no later than the stale timeout); no dots after a load until Play; dots resume on Play. Refs: R-WD-7, R-PS-4.
- [ ] **M5. State restore**: save a project while Playing, reload it. Plugin is Stopped after restore and the sample reloads; no Play/Stop state is in the saved state. Refs: R-PS-4, R-PS-7.
- [ ] **M6. Play-click race**: press Play immediately after a load completes. Plugin MAY stay Stopped, and the buttons show Stop within 100 ms. Refs: R-PS-4 (accepted race).
- [ ] **M7. Parameter sweep and rates**: sweep all 9 parameters while Stopped and while Playing; run at 44.1, 48 and 96 kHz. No crash or artifact. Refs: R-PS-4.

## Phase 7. Docs and spec sync (archive time, not apply)

- [x] **D1. Specs in a separate docs commit** (~150 lines, not part of the code PR). The delta specs in this change are written, complete and consistent (verified after D2-D4). Main specs (`openspec/specs/transport-gate`, `plugin-shell`, `waveform-display`) are synced when the change is archived, not during apply.
- [x] **D2. Rebase R-WD-7 and R-PS-4 text after live-grain-display merges.** Reconciled against the merged main wording: the R-WD-4, R-WD-7, R-PS-4 and R-PS-6 delta bases match the main text, and only the intended Play/Stop changes differ.
- [x] **D3. Reword spec to design naming.** The `transport-gate` delta uses `Starting`/`Stopping` with a naming note (no FadingIn/FadingOut remain). Open question 3 is reconciled in design.md (R-TG-1 words the default Stopped and the optional initial Playing). Design D8 now matches the `onLoadResult` behaviour (clears only on loaded or cleared).
- [x] **D4. Plugin-shell wording**: the "Out of scope", "Assumptions" and "Purpose" replacements are applied in the delta; "drone", "always-on" and "renders continuously" appear only in the consistency scenario, not as current behaviour. The README statement is also updated.

## Summary

| Phase | Tasks | ~Lines | Spec refs |
|---|---|---|---|
| 0 Prerequisite | T0 | 0 | design Migration |
| 1 Core gate | T1.1 to T1.3 | ~205 | R-TG-1..4, 8, 9 |
| 2 Engine | T2.1 to T2.4 | ~230 | R-TG-1..6, 10 |
| 3 Realtime, split | T3.1, T3.2 | ~65 | R-TG-7, 9, R-GR-6 |
| 4 Processor | T4.1 | ~35 | R-PS-4 |
| 5 Editor | T5.1 | ~55 | R-PS-6 |
| 6 Manual | M1 to M7 | 0 | R-PS-4, 6, R-WD-7, R-TG-3 |
| 7 Docs | D1 to D4 | ~150 (docs only) | all |

Code total is about 590 lines (tests about 400, production about 190) in one PR, under the 800-line budget.

### Split recommendation

One PR is fine at about 590 lines. If review growth pushes it past 800 (for example the processor-side tests need a harness), split at the Phase 3 and Phase 4 boundary: PR A is the core (Phases 1 to 3, about 500 lines, no plugin behaviour change), PR B is plugin glue and editor (Phases 4 and 5, about 90 lines plus tests). PR A is safe to ship alone since the processor still needs `InitialTransport` and the default Stopped, so PR A must either include a temporary `setPlaying(true)` in `prepareToPlay` or PR B must follow immediately. Prefer a single PR.

### Open points

- Processor-level `[Catch2]` scenarios depend on whether a test target can link the processor. Design says unit layer only.
- Exact plugin source paths (`Source/plugin/` per the design, `Source/` per the proposal) must be confirmed at apply.
- Host `prepareToPlay` mid-session resetting to Stopped is accepted per spec but not yet confirmed by the user.
- The Play-click race and the failed-load behaviour are accepted per spec.
