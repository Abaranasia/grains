# Design: Sample Loader (RT-safe source publish + load UI)

## Technical Approach

All ownership and validation logic lives in JUCE-free `grains_core` (strict TDD, Catch2): a header-only `SourceExchange<T>` mailbox and a `buildSampleBuffer` function. JUCE glue is thin and verified manually: a `SampleLoader` background thread decodes, builds and publishes; `processBlock` acquires once per block; a custom editor embeds `GenericAudioProcessorEditor` plus Load button, drag-and-drop and a status label. Specs: `specs/sample-source/spec.md` (R-SS-1..10), `specs/plugin-shell/spec.md` (R-PS-4/6/7).

Verified engine facts: `setSource` is non-owning and resets the pool (`GranularEngine.cpp:29-33`); output is clamped per sample to ±`kOutputCeiling` = 4.0f (`GranularEngine.h:35`, applied by `finalize()` at `GranularEngine.cpp:11-17` over every sample in `process()` at lines 56-59); the engine already scales by source/host rate, so no resampling.

## Architecture Decisions

| Topic | Choice | Rejected | Rationale |
|---|---|---|---|
| Hand-off | `SourceExchange<T>`: atomic `pending_` and `retired_` pointers to a heap `Box{unique_ptr<T>}`; consumer-owned `current_` | `atomic<shared_ptr>`; double buffer; mutex | `atomic<shared_ptr>` is not lock-free on MSVC and frees on the last-owner thread (audio). Mutex blocks |
| Clear (restore of missing/empty path) | `publish(nullptr)` enqueues a Box with an empty value; acquire reports `changed`, source `nullptr` | Sentinel pointer; separate clear flag | The Box keeps "nothing pending" (`pending_ == nullptr`) distinct from "pending clear". One protocol, no extra atomics |
| Template | `SourceExchange<T>`, used as `SourceExchange<SampleBuffer>` | Concrete class | Tests use a `Tracked` type with destruction and thread-id counters |
| Retire | Single slot. If full, acquire defers adoption (keeps current, pending stays) | Fixed N-slot ring; consumer frees | Never frees, allocates or leaks on the audio thread. N slots have the same full case with more code |
| Builder input | `ChannelView{const float*, size_t frames}` array + rate | `vector<vector<float>>`; JUCE buffer | JUCE-free, no copy, can check mismatched lengths (R-SS-6) |
| Builder errors | `noexcept`, returns `BuildResult{buffer, truncated, error}`; `bad_alloc` caught as `OutOfMemory` | Exceptions; `optional` | Distinguishable, never throws (orchestrator decision 3) |
| Cap | `maxSourceFrames(rate) = floor(300 * rate)`, clamped to `INT_MAX`; `0` for invalid rates | Fixed frame cap; rejection | Spec R-SS-5. Loader reads only the first `cap` frames, ORs `truncated` |
| Loader thread | One `juce::Thread`; latest request wins via generation counter; chunked read (65536 frames) aborts on exit or newer request | `ThreadPool`; `callAsync` decode | Single producer by construction; bounded shutdown latency |
| UI notify | `callAsync` lambda checks a `shared_ptr<atomic<bool>> alive` flag, cleared in `SampleLoader::stop()` on the message thread; processor stores status and fires a `ChangeBroadcaster` | `WeakReference`; editor callbacks from the loader | Check and destruction both run on the message thread, so there is no TOCTOU. The editor can come and go |
| State | Property `samplePath` (absolute path string) on `apvts_.state` | Separate XML child; embedded audio | Round-trips with `copyState`/`replaceState` without new code |

## SourceExchange Contract

```cpp
template <typename T> class SourceExchange {           // Source/core/include/grains/SourceExchange.h
public:
    struct Acquired { const T* source; bool changed; };
    ~SourceExchange();                                  // deletes pending_, retired_, current_
    bool publish(std::unique_ptr<T> next) noexcept;     // producer; nullptr = clear; false only if Box alloc fails (state unchanged)
    void collect() noexcept;                            // producer; deletes the retired Box. MUST follow every publish
    Acquired acquire() noexcept;                        // consumer; wait-free, no alloc/free
private:
    struct Box { std::unique_ptr<T> value; };
    std::atomic<Box*> pending_{nullptr}, retired_{nullptr};
    Box* current_ = nullptr;                            // consumer-only
};  // non-copyable, non-movable; static_assert(std::atomic<Box*>::is_always_lock_free)
```

| Op | Code | Order | Why |
|---|---|---|---|
| publish | `old = pending_.exchange(box, acq_rel); delete old;` (exchange only, no internal collect) | acq_rel | Release publishes Box and T contents. Acquire covers a returned unseen Box |
| collect | `delete retired_.exchange(nullptr, acquire)` | acquire | Pairs with the consumer's release store: every audio-thread read of the old samples happens-before the delete |
| acquire 1 | `if (retired_.load(relaxed)) return {cur, false}` | relaxed | Only the consumer writes non-null. A stale non-null only defers. A null read touches no data |
| acquire 2 | `next = pending_.exchange(nullptr, acquire)`; null → `{cur, false}` | acquire | Pairs with the publish release, so the Box and T are fully visible |
| acquire 3 | `if (current_) retired_.store(current_, release)`; `current_ = next` | release | Orders the consumer's earlier reads of the old buffer before the producer's acquire and delete |

Liveness: the producer MUST call `collect()` after every `publish` (the loader runs `publish(...); collect();`). Because collect runs after the new Box is pending, when it returns either `retired_` is empty or the pending Box has already been adopted. A deferral never strands a pending Box. The opposite order (collect, then publish) can strand it. The loader also calls `collect()` on each 250 ms wake so a retired buffer (up to ~460 MB at 192 kHz stereo) is released promptly. Keeping `publish` exchange-only makes every slot state reachable from a single thread: for example `publish(A); acquire; publish(B); acquire; publish(C); destroy` leaves A retired, B current and C pending.

Threading invariants: (1) one producer at a time (loader thread only); (2) one consumer at a time. The host serializes `processBlock`; if calls come from different threads, the host must order them (happens-before between calls), as JUCE hosts do; (3) the consumer calls `engine.setSource(r.source)` whenever `r.changed`, before any `process`; (4) the audio thread never deletes a `T` or Box; (5) the destructor runs only with no producer and no consumer active.

## Builder Contract

```cpp
namespace grains {                                     // SampleBuilder.h / src/SampleBuilder.cpp
inline constexpr double kMaxSourceSeconds = 300.0;
struct ChannelView { const float* data; std::size_t frames; };
enum class BuildError { None, NoChannels, NoFrames, BadSampleRate, MismatchedLengths, OutOfMemory };
struct BuildResult { std::unique_ptr<SampleBuffer> buffer; bool truncated = false; BuildError error = BuildError::None; };
std::size_t maxSourceFrames(double sampleRate) noexcept;
BuildResult buildSampleBuffer(const ChannelView* ch, std::size_t numChannels, double sampleRate) noexcept;
}
```
Rules: `numChannels == 0` or a null `data` → NoChannels. Rate not finite or `<= 0` → BadSampleRate. Lengths differ (any channel) → MismatchedLengths. 0 frames → NoFrames. Keep channels 0..1 and truncate to `maxSourceFrames`.

## Plugin

- **SampleLoader** (`Source/plugin/SampleLoader.{h,cpp}`, private `juce::Thread`). `request(File, Origin{User, Restore})` can be called from any thread: lock, store, `++gen_`, `notify()`. `run()`: `wait(250)`, `collect()`, take the request. An empty file with Restore publishes `nullptr` (status "No sample"). Otherwise post Loading, then decode: `AudioFormatManager::registerBasicFormats`, `createReaderFor`, `AudioBuffer(min(ch,2), min(len,cap))` in try/catch, chunked `read`, aborting on `threadShouldExit()` or `gen_ != g`. Then build. On success, if the job is still current, `publish(buf); collect();`. If `publish` returns false (Box allocation failed): error status, previous source kept. On failure: User keeps the previous source ("Error: name"). Any Restore failure (missing, unreadable, corrupt, or a non-empty path that is not absolute) runs `publish(nullptr); collect();` and shows "Missing: name"; the path in state stays unchanged. Never publish an empty buffer. Every posted result carries its generation `g`; `onLoadResult` drops results whose `g` is older than the latest request. `stop()`: `alive=false`, `signalThreadShouldExit`, `notify`, `stopThread(4000)`.
- **processBlock**: at the top, after `ScopedNoDenormals` and before the channel-count, mono-scratch and pre-prepare guards: `auto r = exchange_.acquire(); if (r.changed) engine_.setSource(r.source);`. Swaps are never stuck behind an early return.
- **State**: on a successful User result (message thread), `apvts_.state.setProperty("samplePath", fullPath)`. `setStateInformation` calls `loader_.request(..., Restore)` only inside the existing valid-XML and tag-match branch, right after `replaceState` (`PluginProcessor.cpp:91-93`). Invalid data leaves both the source and the parameters unchanged. An empty `samplePath` requests a clear. A non-empty path where `juce::File::isAbsolutePath(path)` is false is treated as missing and never passed to `juce::File` (that constructor asserts on relative paths). The request is async and never blocks.
- **Members/shutdown**: declaration order `apvts_, raw_, exchange_, engine_, scratch_, status_, broadcaster_, loader_`. `~GrainsAudioProcessor() { loader_.stop(); }`. Reverse destruction then removes the loader before `exchange_`, and `engine_` (dangling non-owning pointer, never dereferenced) before `exchange_`.
- **Editor** (`PluginEditor.{h,cpp}`): `AudioProcessorEditor`, `FileDragAndDropTarget`, `ChangeListener`. Children: `GenericAudioProcessorEditor generic_{proc}`, a Load `TextButton`, a status `Label`. A 40 px top bar holds the button and label. `FileChooser("*.wav;*.aiff;*.aif").launchAsync` with a `SafePointer` capture. Drops: the first file whose extension matches case-insensitively; others are ignored. The dtor removes the change listener.

## Data Flow

    Editor(Load/drop) -> proc.loadFile -> loader.request --+
    setStateInformation -> replaceState -> loader.request -+-> [loader thread] decode -> build -> exchange.publish
    processBlock: exchange.acquire -> (changed) engine.setSource -> process
    loader --callAsync(alive?)--> proc.onLoadResult -> status + samplePath -> ChangeBroadcaster -> editor label

## File Changes (~lines, code+tests+CMake; spec deltas are counted separately as docs, as in plugin-target)

| File | Action | ~Lines |
|---|---|---|
| `Source/core/include/grains/SourceExchange.h` | Create | 80 |
| `Source/core/include/grains/SampleBuilder.h`, `src/SampleBuilder.cpp` | Create | 75 |
| `Source/core/CMakeLists.txt` | Add source | 1 |
| `tests/test_source_exchange.cpp` | Create: semantics, stress, engine swap | 150 |
| `tests/test_sample_builder.cpp` | Create | 65 |
| `tests/test_engine_realtime.cpp` | Add 2 cases | 30 |
| `tests/CMakeLists.txt` | 2 sources, `find_package(Threads)`, link | 4 |
| `tests/support/AllocationCounter.{h,cpp}` | Add `gFrees` (incremented in `plainFree`/`alignedFree` for non-null pointers while a scope is active) and `AllocationScope::frees()` | 10 |
| `Source/plugin/SampleLoader.{h,cpp}` | Create | 145 |
| `Source/plugin/PluginProcessor.{h,cpp}` | Modify | 70 |
| `Source/plugin/PluginEditor.{h,cpp}` | Create | 95 |
| `Source/plugin/CMakeLists.txt` | Add sources | 4 |

Total is about 730 of 800. Shrink order if it runs over: stress test to about 35 lines; table-driven builder rejection cases; drop the chunked abort (whole-file read, accept a slower shutdown); a minimal editor layout.

## Testing Strategy (Strict TDD: each RED compiles and fails before its GREEN)

| Layer | Test | Pass criteria |
|---|---|---|
| Unit `grains_tests` | Empty acquire; publish then acquire; second acquire unchanged; publish A,B → A freed on publish, B delivered; swap keeps A alive until `collect`; clear → `{nullptr, changed}`; dtor after `publish(A); acquire; publish(B); acquire; publish(C)` (A retired, B current, C pending) | `Tracked::live` exact at each step, 0 at the end |
| Stress | Producer runs `publish; collect` for ids 0..4999 (every 7th a clear, the last non-null), then loops `collect()` until a shared `done` flag is set. The consumer acquires with a 10 s deadline and sets `done` on both success and timeout, so the producer loop always ends | Ids strictly increasing (no duplicates); `magic` is alive on every read; the last id is seen; after join and dtor `live == 0`; `consumerFrees == 0` (dtor thread id ≠ consumer) |
| Engine swap | Noise A active>0 → acquire zero buffer B → `activeGrainCount()==0`, output exact zeros; clear → zeros | Exact |
| Builder | Mono/stereo unchanged; 6 ch → ch 0,1; rate 10 Hz: 3001 frames → 3000 + truncated, 3000 → full; NoChannels/NoFrames/BadSampleRate(0,-44100,NaN,Inf)/Mismatched | Exact |
| Realtime `grains_realtime_tests` | Before the scope: `publish(A); acquire; publish(B)` (publish allocates the Box). Inside `AllocationScope`: acquire (swap A→B, retire) + `setSource` + 50 blocks. Separately: `publish(nullptr)` before the scope, then acquire clear + process inside it | `count() == 0` and `frees() == 0` |
| Manual (Standalone) | Load button; drag aiff; drop .txt ignored; replace while playing; two quick loads; corrupt file keeps previous; state round trip; moved file → "Missing: name", silent, path kept on re-save; close during a large load; peak ≤ 4; 44.1/48/96 kHz, mono layout | Verify report |

## Threat Matrix

N/A: no routing, shell, subprocess, VCS/PR automation, executable-file classification, or process-integration boundary. File input is decoded by JUCE readers only.

## Migration / Rollout

No migration. The plugin stays opt-in (`GRAINS_BUILD_PLUGIN`). Old states without `samplePath` restore as "no sample". Commits: core, plugin, docs.

## Risks / Known Limitations

- Output can reach ±4 (no limiter); the drone is always on once loaded. Documented in R-PS-4.
- Swap resets the grain pool: a brief dropout or click.
- An absolute path in state leaks the user's directory layout and breaks across machines.
- Peak memory during a load is about 3x the source (decode buffer, built buffer, retired) and up to about 460 MB per buffer at 192 kHz stereo 5 min.
- No TSan on MSVC: correctness rests on the stress test, the ordering table, and review.
- `setProperty` on the message thread vs a host calling `getStateInformation`/`setStateInformation` off-thread is the existing APVTS pattern, not hardened.
- Assumes the processor is destroyed on the message thread.
- `stopThread(4000)` force-kills the loader if one read chunk blocks longer than that (slow or network files). This can leak or corrupt state at shutdown. Accepted; the chunked abort keeps it unlikely.

## Open Questions

- [ ] None blocking. Cap (5 min, truncate) and path policy follow the orchestrator's defaults.
