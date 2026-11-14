# Design: Loader hardening and follow-up cleanup

## Technical Approach

Approach A from the exploration. Change one call (`stopThread(-1)`) and write down why it is safe. The other items are test hardening, comments, and moving the JUCE pin into one file. There is no new API and `grains_core` behaviour does not change. Four independent commits go in one PR on `feat/loader-hardening`.

End-of-line rule: keep each file's current EOL. The following files are CRLF:
- `CMakeLists.txt`
- `SampleLoader.cpp`
- `SampleBuilder.h`
- `SourceExchange.h`
- `test_sample_builder.cpp`
- `test_engine_realtime.cpp`

These files are LF:
- `test_generation_gate.cpp`
- `ci.yml`
- `cmake/GrainsWarnings.cmake`

Create the new `cmake/JucePin.cmake` as LF, to match its sibling.

## Architecture Decisions

| Item | Option | Tradeoff | Decision |
|---|---|---|---|
| W2 | `stopThread(-1)` | The host can hang on a stalled share, but there is no forced kill. Matches `juce::Thread::~Thread` (`juce_Thread.cpp:57`). | **Chosen** |
| W2 | Approaches B, C and D | B is testable but does not unblock a blocked syscall. C needs an ownership refactor. D is Windows-only. | Rejected (out of scope) |
| S-A | Wait with a deadline, then `CHECK(commits > 0)` after `join()` | Deterministic. A failure reports instead of hanging. | **Chosen** |
| S-A | `REQUIRE` before `join()` | A `REQUIRE` that throws while a `std::thread` is still joinable calls `std::terminate`. | Rejected |
| S1 | Volatile pointer sink | Portable to MSVC, clang and gcc. Needs no intrinsics. | **Chosen** |
| S1 | `benchmark::DoNotOptimize` or inline asm | Adds a dependency, or does not work on MSVC x64. | Rejected |
| V1 | `include(JucePin)` with three variables | Keeps the pin in one place. The CI key hashes only that file. | **Chosen** |
| V1 | Key on `CMakeLists.txt` (status quo) | Unrelated edits to `CMakeLists.txt` invalidate the cache. | Rejected |

### W2: deadlock analysis (`Source/plugin/SampleLoader.cpp:31-37`)

`stop()` has two callers:
- `~GrainsAudioProcessor` (`PluginProcessor.h:22`). It is the first statement of the destructor body, so `exchange_`, `engine_` and the loader are all still alive at that point.
- `~SampleLoader` (`SampleLoader.h:32`). This second call does nothing, because `stopThread` on a thread that has already exited returns immediately.

No other code calls `stop()` or `stopThread`. The editor never calls it.

An unbounded wait cannot deadlock, for these reasons:
1. The loader never blocks on the message thread. `post()` only queues work with `callAsync` (L57).
2. `stop()` holds no lock while it waits. The loader takes `lock_` and then the gate mutex. The message thread takes them in the same order in `request()`, and only briefly.
3. `exchange_.collect()` is lock-free.

The `callAsync` lambda captures `this`, but it is guarded by `alive_`. `stop()` sets `alive_` to false on the message thread before it waits.

```cpp
void SampleLoader::stop()
{
    alive_->store(false);
    signalThreadShouldExit();
    notify();
    // No timeout, as in juce::Thread::~Thread: on timeout JUCE kills the thread by force, possibly
    // while it holds the gate mutex or the heap lock, just before gate_ and exchange_ are destroyed.
    // A read stalled on a network share can delay shutdown but never corrupt it. Cannot deadlock:
    // the loader never waits on the message thread (post() only queues callAsync) and we hold no lock.
    stopThread(-1);
}
```

In `SampleLoader.h:38`, change the comment to: `// Message thread only. Idempotent; blocks until the loader thread exits. Call before the exchange is destroyed.`

W-A needs no code change. The behaviour at L77-79 and L94-96 goes into the R-SS-9 spec delta.

### S-A (`tests/test_generation_gate.cpp:60-82`)

Change the comment at L60-61 to:

`// R-SS-11: bounded stress. The callback must never observe a generation other than the one it was admitted under. Bumping starts only after a first commit (10 s deadline), so the test cannot pass without committing.`

Then make these code changes:

```cpp
std::atomic<int> commits{0};
// committer: if (g.commitIf(gen, [&]() noexcept { ... })) commits.fetch_add(1);
const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
while (commits.load() == 0 && std::chrono::steady_clock::now() < deadline) std::this_thread::yield();
for (int i = 0; i < kBumps; ++i) g.next();
done = true;
committer.join();
CHECK(commits.load() > 0); // first assertion after join
```

The deadline is 10 s. The first commit normally lands within microseconds, so 10 s leaves a large margin for sanitizer builds on a busy CI runner. If no commit lands in time, the bump loop still runs and the thread is still joined. The test then fails with `commits == 0` and never hangs.

### S1 (`tests/test_engine_realtime.cpp:102-120`)

Add this to the existing anonymous namespace:

```cpp
// Writing to and reading back a volatile makes the pointer escape, so the compiler cannot elide
// the new/delete pair (allowed since C++14) and the free counter really sees the delete.
int* volatile g_sink = nullptr;
int* escape(int* p) noexcept { g_sink = p; return g_sink; }
```

In the test, use `auto* outside = escape(new int(1));` and `auto* q = escape(new int(2));`. The null-delete line stays as it is.

### S2 (`tests/test_sample_builder.cpp`). Values checked against `SampleBuilder.cpp:13-18`

```cpp
TEST_CASE("builder: maxSourceFrames floors, rejects sub-frame caps and clamps to INT_MAX", "[builder]")
{
    constexpr auto kIntMax = static_cast<std::size_t>(std::numeric_limits<int>::max());
    CHECK(grains::maxSourceFrames(10.0013) == 3000); // 3000.39 floors
    CHECK(grains::maxSourceFrames(0.003) == 0);      // 0.9 floors to zero (rate < 1/300)
    CHECK(grains::maxSourceFrames(1e8) == kIntMax);  // 3e10 clamps
    CHECK(grains::maxSourceFrames(std::numeric_limits<double>::max()) == kIntMax); // product is +inf
    const auto a = ramp(10, 0.0f);
    const ChannelView v[] = {{a.data(), a.size()}};
    const auto r = buildSampleBuffer(v, 1, 0.003);
    CHECK(r.buffer == nullptr);
    CHECK(r.error == BuildError::BadSampleRate);
}
```

The `DBL_MAX` case and the 1e8 case both hit the clamp branch before any cast. They are therefore clean under UBSan's `float-cast-overflow` check.

S4: remove `#include <cmath>` at L6. The new test needs only `<limits>`, which is already included.

### S3 and S5 (comments only)

`SourceExchange.h`: after L15, add:
```
//           The type does not enforce this. In the plugin, SampleLoader::commitGated does: it
//           collects after every accepted commit, and run() also collects on every tick.
```

`SampleBuilder.h:11`: add above the constant:
```
// Longest source kept. 300 s of 48 kHz stereo float is about 115 MB (peak about twice that while the
// decode buffer and the builder's copy coexist), and it keeps frame counts far below
// juce::AudioBuffer's INT_MAX limit. maxSourceFrames still clamps to INT_MAX for extreme rates.
```

### V1 and V5

New file `cmake/JucePin.cmake` (LF):
```cmake
# SPDX-License-Identifier: AGPL-3.0-or-later
# Single source of the pinned JUCE fetch (used when GRAINS_JUCE_DIR has no JUCE). The CI JUCE cache
# key hashes only this file, so changing the pin here invalidates the cached download.
set(GRAINS_JUCE_VERSION "9.0.0")
set(GRAINS_JUCE_SHA "f8f8864172464b9adf9eba6101e1f784838d1597")
set(GRAINS_JUCE_SHA256 "0bef95e4d4e6e8e1aaa98dfa9aae7529235152e140a7710525bc38b03f8c93a9")
```

`CMakeLists.txt` L31-45 (CRLF). `CMAKE_MODULE_PATH` already includes `cmake/` (L9).
```cmake
include(JucePin) # GRAINS_JUCE_VERSION, GRAINS_JUCE_SHA, GRAINS_JUCE_SHA256
option(GRAINS_JUCE_FETCH "Download pinned JUCE ${GRAINS_JUCE_VERSION} when GRAINS_JUCE_DIR has no JUCE" ON)
...
    message(STATUS "Grains: no JUCE at '${GRAINS_JUCE_DIR}'; fetching JUCE ${GRAINS_JUCE_VERSION} "
      "(${GRAINS_JUCE_SHA}). On failure set -DGRAINS_JUCE_DIR=<JUCE checkout> "
      "or -DGRAINS_JUCE_FETCH=OFF.")
    include(FetchContent)
    FetchContent_Declare(juce
      URL https://github.com/juce-framework/JUCE/archive/${GRAINS_JUCE_SHA}.tar.gz
      URL_HASH SHA256=${GRAINS_JUCE_SHA256}
      SOURCE_SUBDIR grains-download-only) # no CMakeLists.txt there: download only, no add_subdirectory
```

The `# JUCE tag 9.0.0` comment at L42 is removed, because the version now lives in the variable.

`ci.yml:74`:
```yaml
          key: ${{ runner.os }}-juce-${{ hashFiles('cmake/JucePin.cmake') }}
```

## Data Flow

```
cmake/JucePin.cmake --include--> CMakeLists.txt --> FetchContent URL / URL_HASH / status message
        \--hashFiles--> ci.yml cache key
```

## File Changes

| File | Action | WU |
|---|---|---|
| `Source/plugin/SampleLoader.cpp`, `SampleLoader.h` | Modify | 1 |
| `tests/test_generation_gate.cpp`, `test_engine_realtime.cpp`, `test_sample_builder.cpp` | Modify | 2 |
| `Source/core/include/grains/SourceExchange.h`, `SampleBuilder.h` | Modify (comments) | 3 |
| `cmake/JucePin.cmake` | Create | 4 |
| `CMakeLists.txt`, `.github/workflows/ci.yml` | Modify | 4 |

## Commit Plan

1. WU1: `fix(plugin): wait for loader thread at shutdown`. Covers W2 and W-A, plus the R-SS-7 and R-SS-9 deltas. About 25 lines.
2. WU2: `test: harden stress, frees and cap tests`. Covers S-A, S1, S2 and S4, plus the R-SS-5 delta. About 40 lines.
3. WU3: `docs(core): document cap rationale and collect rule`. Covers S3 and S5. About 7 lines.
4. WU4: `build: single JUCE pin file and CI cache key`. Covers V1 and V5, plus the R-BH-5 delta. About 25 lines.

## Testing Strategy

| Item | Layer | TDD note |
|---|---|---|
| S-A | Catch2 | RED: a one-off local mutation (the committer calls `commitIf(gen + 1, ...)`) must fail with `commits == 0` within about 10 s. Revert the mutation, then GREEN. Do not commit the mutation. |
| S2 | Catch2 | Characterization tests. They are expected to pass immediately. Record them as such, not as RED. |
| S1 | Catch2 | Tests the test itself. Elision depends on the optimizer, so there is no deterministic RED. Running it on a Release build is optional. |
| W2, W-A | Manual | [Manual] scenarios: shut down during a stalled share (no "killing thread by force"), and a failed clear shows the documented label. |
| V1, V5 | CI | The plugin job stays green. Its first run is a one-time cache miss, and later runs hit the cache. Outside `openspec/`, `rg f8f88641` matches only `cmake/JucePin.cmake`. |

## Threat Matrix

N/A. The change adds no routing, shell or subprocess code, no VCS or PR automation, and no executable-file classification. The CI edit changes only the string inside the cache key. It adds no new commands or permissions.

## Migration / Rollout

No migration is required. The CI cache misses once.

## Open Questions

None blocking.
