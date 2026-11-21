# Exploration: live-grain-display

> **Superseded by design**: the numbers in this document are superseded. See `design.md` (Proposal Corrections) for the corrected values: clear after 8 non-fresh ticks (~267 ms, not 250 ms), 16-byte `GrainMark` (not 12), flicker only for blocks above ~233 ms (~10k samples at 44.1 kHz, not 8192).

Mirror of engram `sdd/live-grain-display/explore` (#718).

## Current state
- `GrainPool` has 128 slots; the only introspection is `activeGrainCount()`; `forEachActive` is non-const.
- `Grain` holds `pos` (double, frames of the resampled buffer), `rate` (> 0 today), `length`, `age`. `spawn()` already shrinks or moves back grains near the buffer end, so `pos` is final.
- `process()` releases finished grains, so after a block the pool holds only grains alive at block end; grains shorter than a block never show (statistically representative).
- `setSource` and `prepare` reset the pool, so markers clear on a swap, clear or rate-change reload.
- `processBlock`: the stereo branch returns early, so a hook at the end needs a small restructure; the mono path loops over chunks.
- `WaveformView` repaints only on change. CI: Windows plus Linux clang-18 ASan+UBSan; no TSAN job (ASan and TSAN cannot share a build).

## What a display needs per grain
Normalised head position `clamp01(pos / (frames - 1))`, window progress `age / length`, envelope level `window.at(progress)` for brightness. Direction and extent can be added later for roadmap #8. Types: `GrainMark{position, progress, level}` (12 bytes), `GrainSnapshot{seq, count <= 128, marks[128]}` (~1.5 KB). Engine call: `snapshotGrains(GrainSnapshot&) const noexcept`, which needs a const `forEachActive` overload.

## Publication options
| Option | Tearing | Notes |
|---|---|---|
| Relaxed atomic array plus count | frames can mix | ~30 lines, no clean fresh/stale signal |
| Seqlock | none | reader retries, per-element atomics |
| **Triple buffer** | none | wait-free both sides, plain structs, TSAN-clean, `fresh` flag; ~70 lines, same style as `SourceExchange` |
| SPSC queue / event ring | n/a | overkill / intrudes in `spawn()` |

Cost: at most 128 iterations per block (~1-2 us), so publish unconditionally. Do not read the engine from the UI (`pool_` and `source_` are mutated and may be freed on the audio thread).

## Hook placement and proof it is inert
Publish after render on both paths, and an empty snapshot after `engine_.prepare()`. Tests: bit-identical render with and without the hook (same seed, keep rendering to prove RNG did not diverge); zero-alloc snapshot plus publish in `grains_realtime_tests`, asserting `count == activeGrainCount()`.

## UI side
`juce::Timer` at 30 Hz, running only while the view is showing; repaint only on a fresh frame or a stale transition. A core `GrainDisplayModel` clears markers after ~8 non-fresh ticks (~250 ms). Draw markers only in the Waveform state. Cache the waveform in a `juce::Image`. A one-poll mismatch on source swap is accepted.

## Visual design (clean functional)
Thin 1-1.5 px vertical ticks at the head x, alpha from `level`, colour distinct from the waveform and the position marker. Order: waveform, band, ticks, position marker on top.

## Testability
Automated (Catch2): snapshot count, normalisation, clamping, empty source, density 0, full pool, determinism, empty after `setSource`; display model fresh/stale; mailbox single-thread semantics plus a two-thread tearing stress test; bit-identical render; zero-alloc. Optional TSAN job (`GRAINS_SANITIZE_THREAD`, ~40 lines). Manual only: paint, timer lifecycle, DAW behaviour with transport stopped, resize/DPI, CPU with several instances.

## Spec impact
- `waveform-display`: modify R-WD-4 (a 30 Hz poll timer only while showing; static layers stay change-driven) and its scenarios; add R-WD-5 (snapshot contract), R-WD-6 (publication), R-WD-7 (live view); remove live grains from Purpose and out-of-scope.
- `plugin-shell`: R-PS-4 mentions the publish; R-PS-5 states it is lock-free.
- `grain-rendering`: new R-GR-11, introspection must not alter output, RNG or pool and must not allocate.
- No parameter changes; R-PS-3 stays at 9.

## Approaches
1. **Triple-buffered snapshot, 30 Hz Timer poll, core `GrainDisplayModel` (recommended).** Tear-free, wait-free, race-free; Medium effort.
2. Relaxed atomic array plus seq. Minimal; frame mixing and weak tests. Low-Medium.
3. Seqlock with per-element atomics. Tear-free but fiddly, no advantage over 1. Medium.

## Size forecast
About 800-1000 authored lines plus 150-200 of docs (core source ~250, core tests ~420, plugin glue 120-150, optional TSAN ~40). Suggested split: two stacked PRs (core ~650-700; plugin ~150-250), TSAN as an optional micro-PR.

## Risks
R-WD-4 "no timer" was an explicit archived decision; the `processBlock` restructure has no plugin-level automated test; the stale threshold flickers above ~8192-sample blocks; sub-block grains are invisible at high density; a one-poll mismatch on swap; no TSAN job today; roadmap #8 needs consistent head normalisation.

## Open product questions
1. Amend R-WD-4 to allow a 30 Hz poll timer that runs only while the view is showing?
2. Marker style: envelope-brightness ticks, lane dots, or extent bars?
3. Host stopped: clear markers after ~250 ms or freeze the last frame?
4. Markers in the "no waveform" state?
5. TSAN stress job here, separate micro-PR, or skip?
6. Two stacked PRs or one with a size exception?
