# Exploration: loader-hardening

Source: follow-ups in `openspec/changes/archive/2026-10-24-juce-fetch-license/archive-report.md` and `openspec/changes/archive/2026-10-24-sample-loader-fixes/archive-report.md`. Engram mirror: `sdd/loader-hardening/explore` (obs #596).

All ten items are confirmed present on `feat/loader-hardening` (branched from `feat/juce-fetch-license` @411b8a1).

## Findings

### W2: forced kill on stop (bug)

- `SampleLoader::stop()` (`Source/plugin/SampleLoader.cpp:31-37`) calls `stopThread(4000)`.
- `load()` checks `isCurrent()` (which includes `threadShouldExit()`) only between 65536-frame chunks (L127-131).
- `file.existsAsFile()` (L104), `createReaderFor` (L112) and `reader->read` (L130) cannot be interrupted. A stalled network share can block any of them for longer than 4 s.
- On timeout, JUCE `Thread::stopThread` (`juce_Thread.cpp:248-280`) hits `jassertfalse`, logs "killing thread by force" and calls `killThread()`:
  - Windows: `TerminateThread`.
  - Linux and macOS: `pthread_cancel`.
- A killed thread can hold the gate mutex or the heap lock. Destroying `gate_` and `exchange_` afterwards is then undefined behaviour, or a process-wide deadlock. The reader and the buffer also leak.
- JUCE's own `Thread::~Thread` uses `stopThread(-1)`.
- `post()` only calls `callAsync` and never blocks the message thread, so an unbounded wait in `stop()` cannot deadlock.
- No tests cover `stop()`. The loader lives in the plugin target, which Catch2 cannot test.

| Approach | Pros | Cons | Effort |
|---|---|---|---|
| A. `stopThread(-1)` plus a comment | Removes the forced kill and the torn state. Matches JUCE's destructor. About 3 lines. | The host may show "not responding" while a stalled share times out. | Low |
| B. Extract the chunk loop into `grains_core` with a cancel predicate | Cancellation between chunks becomes unit-testable. | About +80 lines. Does not help with a blocking syscall. | Medium |
| C. Bounded wait, then detach | Faster host close. | Needs a shared-ownership refactor. A JUCE `Thread` cannot be detached, and an orphan thread during DLL unload crashes. | High |
| D. `CancelSynchronousIo` | Interrupts a stalled read. | Windows-only, untestable, and outside the JUCE API. | Medium |

**Recommendation:** A. B is an optional add-on for testability.

**Spec impact:**
- R-SS-7 MODIFIED: stop waits for in-flight work and never terminates the thread forcibly.
- New [Manual] scenario: shutdown during a stalled read.

### W-A: label when clear allocation fails (OOM)

- `SampleLoader.cpp:78` posts "Error: No sample" when `stageClear()` fails.
- L95 has the same unspecified case: a failed clear during restore of a missing file posts "Error: <name>".
- In both cases the previous source stays active.
- **Recommendation:** specify this in R-SS-9, with no code change. If the clear fails to allocate:
  - the previous source stays active and nothing is cleared;
  - an empty path shows the label "Error: No sample";
  - a missing file shows the label "Error: <name>".
- **Spec impact:** R-SS-9 MODIFIED, plus a [Manual] scenario.

### S-A: G5 stress test can pass vacuously

- `tests/test_generation_gate.cpp:62-82`. L61 says the commit count is deliberately not asserted.
- Asserting `commits > 0` directly would be flaky. Make it deterministic instead:
  - Before the bump loop starts, the main thread waits, with a deadline, until `commits > 0`.
  - Then assert `commits > 0`.
- About +8 lines. No spec change.

### S1: `frees()` assertions can be optimized away

- Only the self-test at `tests/test_engine_realtime.cpp:102-120` is at risk. Under C++14 rules the compiler may elide its `new int` / `delete` pair.
- **Fix:** make the pointer escape through a volatile sink.
- About +8 lines, tests only.

### S2: `maxSourceFrames` branches untested

- `Source/core/SampleBuilder.cpp:13-18`. The current tests cover only rate 10 and rate 0.
- Add these cases:
  - Floor: `10.0013` → 3000.
  - Floor to zero: a rate below 1/300 → 0, and the builder returns `BadSampleRate`.
  - Clamp: `1e8` → `INT_MAX`.
  - Overflow: `DBL_MAX` → `INT_MAX`.
- About +15 lines of characterization tests.
- R-SS-5 does not mention the `INT_MAX` clamp (optional MODIFIED).

### S3: collect-after-publish only documented

- Production code never calls `publish()` directly. `commitGated` already collects (`SampleLoader.cpp:62-69`), and `run()` collects on every tick (L44).
- Only tests call `publish()`, and some deliberately skip `collect()`.
- **Recommendation:** add no new API. Update the `SourceExchange.h` class comment (L14-15) to name `commitGated` as the place that enforces the rule.

### S4: unused include

- Remove `<cmath>` from `tests/test_sample_builder.cpp:6`.

### S5: `kMaxSourceSeconds` rationale

- Add a comment at `Source/core/SampleBuilder.h:11` explaining the cap:
  - 300 s at 48 kHz stereo is about 115 MB of float samples.
  - Peak memory is about twice that (decode buffer plus the builder's copy).
  - The cap keeps the frame count under `juce::AudioBuffer`'s `INT_MAX` limit.

### V1 + V5: single JUCE pin file and CI cache key

- The JUCE commit SHA appears three times: `CMakeLists.txt:38`, `:43` and `.github/workflows/ci.yml:74`.
- "JUCE 9.0.0" is repeated at L31, L37 and L42. `URL_HASH` is at L44.
- V5 is effectively met already: the cache key hashes `CMakeLists.txt`, which contains `URL_HASH`. The problem is the opposite: unrelated edits to `CMakeLists.txt` also bust the cache.
- **Recommendation:**
  - Add `cmake/JucePin.cmake` defining `GRAINS_JUCE_VERSION`, `GRAINS_JUCE_SHA` and `GRAINS_JUCE_SHA256`.
  - `CMakeLists.txt` builds the URL, `URL_HASH` and the status message from those variables.
  - The CI cache key becomes `${{ runner.os }}-juce-${{ hashFiles('cmake/JucePin.cmake') }}`.
- **Spec impact:** R-BH-5 MODIFIED (cache-key wording and the "Plugin job is cached and bounded" scenario).

## Likely work units

| Unit | Items | Est. lines | Spec impact |
|---|---|---|---|
| WU1 `fix(plugin): wait for loader thread at shutdown` | W2, W-A | ~25 | R-SS-7, R-SS-9 MODIFIED |
| WU2 `test: harden stress, frees and cap tests` | S-A, S1, S2, S4 | ~35 | optional R-SS-5 |
| WU3 `docs(core): document cap rationale and collect rule` | S3, S5 | ~7 | none |
| WU4 `build: single JUCE pin file and CI cache key` | V1, V5 | ~25 | R-BH-5 MODIFIED |

The total is about 130-150 changed lines, well under the 800-line budget. Approach B would add about 80.

## Risks

- WU4 depends on the unmerged `feat/juce-fetch-license`. Its PR stacks on that branch, as the user decided.
- `stopThread(-1)` trades a possible host hang on a stalled share for correctness. A bounded close needs an ownership refactor, which is out of scope.
- W2 and W-A can only be verified manually unless approach B is taken.
- The S-A wait needs a deadline, so that a failure reports instead of hanging.
- Out of scope, noted only: stage-then-build holds two copies of the data, about 0.9 GB peak at 192 kHz stereo.

## Ready for proposal

Yes. Open decision: whether to add approach B on top of A.
