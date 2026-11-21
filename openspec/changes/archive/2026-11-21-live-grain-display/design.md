# Design: Live Grain Display

## Technical Approach

Explore approach 1. After each render the audio thread copies the active pool into a fixed `GrainSnapshot` through a const engine call and publishes it with a generic wait-free triple buffer in core. A 30 Hz timer on the message thread polls the buffer and feeds a JUCE-free `GrainDisplayModel`, which decides about fresh, stale and repaint. `WaveformView` then draws lane dots in a strip under a cached waveform image. Specs: `waveform-display` (R-WD-4 modified, R-WD-5..7 new), `plugin-shell` (R-PS-4, R-PS-5), `grain-rendering` (R-GR-11). These IDs are free: the current specs end at R-WD-4, R-PS-8 and R-GR-10.

## Architecture Decisions

| # | Choice | Rejected | Rationale |
|---|--------|----------|-----------|
| D1 | `GrainMark{float position, progress, level; uint16 slot, reserved}`, 16 bytes (static_assert). `GrainSnapshot{uint32 version = kVersion (1); int32 count; array<GrainMark,128> marks}`, about 2 KB, natural alignment | Pointers to grains; storing raw `pos`; a 12-byte mark without `slot` | The view needs the slot for lanes. `reserved` can hold roadmap #8 direction later. New fields are appended and bump `kVersion`, which a future WebView serialiser reads. The size stays fixed, so there is no allocation |
| D2 | `GrainPool::forEachActiveSlot(F) const`, which calls `f(int slot, const Grain&)` in index order | A const overload of `forEachActive` with the same arity; `slotOf()` pointer arithmetic | The name makes the different callback shape explicit. Const makes "no pool mutation" a compile-time fact |
| D3 | `GranularEngine::snapshotGrains(GrainSnapshot&) const noexcept` (see Engine snapshot rules) | A snapshot that `process()` takes internally; the UI reading the engine | The method is a pure reader of `pool_`, `source_` and `window_`, so render, RNG and pool cannot change. The engine stays thread-agnostic |
| D4 | `TripleBuffer<T>`, a wait-free latest-value mailbox (see Triple buffer) | A relaxed atomic array, a seqlock, an SPSC queue | Neither side ever tears or waits. Plain structs. It has a natural fresh flag. It is generic, so future meters or a scope can reuse it |
| D5 | `processBlock` publishes once after any render path. `prepareToPlay` publishes once after `engine_.prepare` (the pool is empty, so the snapshot is empty) | A publish per mono chunk; publishing from the engine | One publish per block keeps the snapshot consistent with the end of the block. The empty frame clears markers at once after a re-prepare |
| D6 | `GrainDisplayModel` in core. `kPollHz = 30` is the single rate constant. `kStaleMs = 250`. `kStaleTicks = ceil(kStaleMs*kPollHz/1000) = 8` (about 267 ms) | Wall-clock time; freezing the last frame | Tick counting is deterministic and testable without time. The product decision is to clear |
| D7 | Lanes: `kLaneRows = 8`, `row = slot % 8`, row pitch 6 px, dot diameter 4 px, strip height 48 px under a 140 px waveform. The view becomes 188 px and the editor stays fixed-size and 48 px taller | `slot / 16`; 128 rows; random rows | The pool takes the lowest free slot, so at low overlap consecutive grains fall on different rows. A grain keeps its row for its whole life. 48 px is enough for 16 dots per row at saturation without dwarfing the waveform |
| D8 | Dot alpha is `0.25 + 0.75 * level`. Colour mint `0xff7ee0a1`, separate from the wave blue and the marker amber. Paint order: cached image, band, dots, marker. Band and marker span the full view height | Ticks over the waveform; full-alpha dots | Grains that just spawned have level 0 and still need to show. The full-height marker makes the dots line up with the position |
| D9 | The view polls through `std::function<const GrainSnapshot*()>`, set by the editor and backed by the new processor method `pollGrains()` | Passing the processor or the mailbox to the view | The view stays decoupled and has a single consumer. Assigning the function allocates once, at construction, on the message thread |

## Engine snapshot rules (D3)

`out.version = kVersion` and `count = 0`. If `source_` is null, the method returns at this point. The pool is always empty then: `setSource` and `prepare` both reset it. For each active slot, in index order:
- `position = clamp01(pos / (frames - 1))` when `frames > 1`, else 0. `spawn` keeps `pos <= frames - 1` because it shrinks or moves back grains near the end. The clamp only absorbs floating-point rounding.
- `progress = float(age) / float(length)` when `length > 0`, else 0. This is the same expression as `renderGrain`, so `level = window_.at(progress)` equals the window of the next sample. Finished grains are released inside `renderSegment`, so after `process()` every remaining grain has `progress` in [0, 1).
- The invariant `count == activeGrainCount()` holds. Grains shorter than a block are never seen (accepted).

## Triple buffer (D4)

```cpp
template <class T> class TripleBuffer {      // T nothrow-default-constructible
public:
    T& back() noexcept;                      // producer: slot to fill in place
    void publish() noexcept;                 // producer
    bool update() noexcept;                  // consumer: true if a newer T was adopted
    const T& front() const noexcept;         // consumer: stable until the next update()
private:
    struct alignas(64) Slot { T value{}; };
    Slot slots_[3];
    alignas(64) std::atomic<std::uint8_t> middle_{1};  // bits 0-1 index, bit 2 fresh
    alignas(64) std::uint8_t back_ = 2;                // producer-owned
    alignas(64) std::uint8_t front_ = 0;               // consumer-owned
};
```
- `publish`: `back_ = middle_.exchange(back_ | kFresh, acq_rel) & kIndex`. The release half publishes the slot writes. The acquire half orders the consumer's earlier reads of the returned slot before the producer's next writes to it.
- `update`: a relaxed load first. If the fresh bit is clear, it returns false. A stale view of the flag only delays the frame by one poll. Otherwise `front_ = middle_.exchange(front_, acq_rel) & kIndex` and it returns true. The acquire half sees the producer's writes. The release half hands back the consumer's reads.
- Alignment: each slot and each control word sits on its own 64-byte line, so there is no false sharing. A static_assert checks `atomic<uint8_t>::is_always_lock_free`.
- Threading: one producer and one consumer, as in `SourceExchange`. The producer role may move between threads only when the host serialises them (`prepareToPlay` against `processBlock`, the JUCE contract).

## processBlock (D5)

The two early returns become one if / else-if / else chain, followed by the publish. The engine calls and their order do not change, so the audio output stays bit-identical.
```cpp
if (stereo) engine_.process(left, right, numSamples);
else if (scratchSize_ <= 0 || scratch_ == nullptr) buffer.clear();
else for (int done = 0; done < numSamples;) { /* unchanged chunk loop */ }
publishGrains();   // engine_.snapshotGrains(grainMailbox_.back()); grainMailbox_.publish();
```
The `getNumChannels() <= 0` return stays before this chain: there is no render, so there is no publish.

## UI model and view (D6-D9)

`bool tick(const GrainSnapshot* fresh)` behaves as follows:
- With a fresh frame: copy it into `shown_`, reset the idle counter, and return true only if the old or the new `count` is greater than 0. Empty-to-empty frames, for example a running transport with no grains, do not repaint.
- Without a fresh frame: increment the idle counter. When it reaches `kStaleTicks` and marks are shown, set `count = 0` and return true once.

`reset()` clears the marks.

Processor API (message thread, single consumer): `const grains::GrainSnapshot* pollGrains() noexcept` returns `&grainMailbox_.front()` when `update()` is true, else nullptr. `grainMailbox_` is a `grains::TripleBuffer<grains::GrainSnapshot>` member (about 6.3 KB inline, no heap).

The view runs its timer (`startTimerHz(kPollHz)`) only while `isShowing()`, display is `Waveform` and a poll function is set. It re-evaluates this in `visibilityChanged`, `parentHierarchyChanged` and `setWaveform`. When the timer stops it calls `reset()`. Live updates repaint only the strip bounds. The waveform image (background and peaks, or the hint) is rebuilt lazily in `paint` after `setWaveform` or `resized`, or when the physical pixel scale changes. Outside the Waveform state no dots are drawn.

Lifetime: the mailbox is a processor member. JUCE destroys the editor before the processor, and `juce::Timer` stops in the view destructor on the message thread. So no poll runs after destruction. Two editors on the message thread would only take frames from each other, with no data race.

## Data Flow

    audio: acquire -> setParams -> render -> snapshotGrains(back) -> publish
                                              TripleBuffer (fresh bit)
    message (30 Hz, showing): pollGrains() -> model.tick() -> repaint(strip)?

## File Changes

| PR | File | Action | ~Lines |
|----|------|--------|--------|
| 1 | `Source/core/include/grains/TripleBuffer.h` | Create | 80 |
| 1 | `Source/core/include/grains/GrainSnapshot.h` | Create | 45 |
| 1 | `Source/core/include/grains/GrainDisplayModel.h`, `src/GrainDisplayModel.cpp` | Create | 70 |
| 1 | `GrainPool.h`, `GranularEngine.h`, `src/GranularEngine.cpp` | Modify | 45 |
| 1 | `Source/core/CMakeLists.txt`, `tests/CMakeLists.txt` | Modify | 5 |
| 1 | `tests/test_triple_buffer.cpp`, `test_grain_snapshot.cpp`, `test_grain_display_model.cpp` | Create | 420 |
| 1 | `tests/test_engine_realtime.cpp` | Modify | 40 |
| 2 | `Source/plugin/PluginProcessor.h/.cpp` | Modify | 35 |
| 2 | `Source/plugin/WaveformView.h/.cpp` | Modify | 120 |
| 2 | `Source/plugin/PluginEditor.h/.cpp` | Modify | 10 |

PR 1 has about 705 code lines and PR 2 about 165, both under the 800 budget. The openspec docs (about 450 lines) go in a separate pure-documentation commit.

## Testing Strategy

| Layer | What | Approach |
|-------|------|----------|
| Automated (`grains_tests`) | Snapshot: count equals active, normalisation, end-of-buffer clamp, null or empty source, density 0, full pool (128), version, slot order, empty after `setSource`/`prepare`. Bit-identity: engine A renders; engine B renders and calls `snapshotGrains` after every block; same seed, 400 blocks, compare bitwise and compare active counts. Mailbox: initial `T{}`, fresh then not fresh, latest wins, the three indices always distinct, front stable across publishes. Stress: 100k publishes of a 512-byte struct with every word equal to its sequence number; the consumer checks consistency and that the sequence increases until it sees the last frame. Bounded, no sleeps, no timing asserts. Model: thresholds, a single repaint signal, empty-to-empty, reset | Catch2, RED first |
| Automated (`grains_realtime_tests`) | Process, then snapshot, publish and update inside `AllocationScope`: 0 allocations, `count == activeGrainCount()` | Existing counter |
| Manual | Painting and DPI, timer starts and stops (hide, minimise, close), DAW with transport stopped (clears after about 267 ms), no dots in the no-waveform state, CPU with 4+ instances, A/B audio | Standalone and a DAW |

## Threat Matrix

N/A. There is no routing, shell, subprocess, VCS/PR automation, executable-file classification or process-integration boundary.

## Migration / Rollout

No migration is required. There are no state or parameter changes.

## Proposal Corrections

- The marks clear after 8 ticks, about 267 ms, not exactly 250 ms.
- Flicker starts with blocks longer than about 233 ms (about 10k samples at 44.1 kHz), not at 8192 samples.
- The mono path also has an unprepared early return. D5 covers it.
- With `slot` added, a mark is 16 bytes, not the 12 bytes in the exploration.
- The spec phase must rewrite the two R-WD-4 scenarios that require no timer, and the "Live active-grain display" out-of-scope text in R-WD Purpose.

## Open Questions

- None blocking.
