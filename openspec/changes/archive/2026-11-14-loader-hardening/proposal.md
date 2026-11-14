# Proposal: Loader hardening and follow-up cleanup

## Intent

On shutdown, `SampleLoader::stop()` calls `stopThread(4000)`. If a read stalls on a network share for longer than that, JUCE kills the thread. The killed thread can hold the gate mutex or the heap lock, so tearing down `gate_` and `exchange_` afterwards is undefined behaviour or deadlocks the process. The same pass closes nine smaller follow-ups from earlier archives:

- spec-to-code gaps;
- tests that can pass without testing anything;
- the JUCE pin, which is repeated in three places.

## Scope

### In Scope
- W2: change the call to `stopThread(-1)` and add a comment explaining why.
- W-A: specify in R-SS-9 what already happens when the clear allocation fails. No code change.
- S-A: make the G5 stress test deterministic. Wait, with a deadline, for the first commit, then assert `commits > 0`.
- S1: route the self-test pointer in `frees()` through a volatile sink, so the compiler cannot elide the allocation.
- S2: add `maxSourceFrames` tests for floor, floor-to-zero (`BadSampleRate`), the `INT_MAX` clamp and `DBL_MAX`.
- S3: in the `SourceExchange.h` comment, name `commitGated` as the code that enforces collect-after-publish.
- S4: remove the unused `<cmath>` include.
- S5: add a comment explaining the `kMaxSourceSeconds` cap.
- V1+V5: add `cmake/JucePin.cmake` as the single source of the version, SHA and SHA256. The CI cache key hashes only that file.

### Out of Scope
- Loader approach B: moving the chunk loop into core with a cancel predicate. Possible future change.
- Loader approaches C and D: bounded wait plus detach, and `CancelSynchronousIo`.
- P1-P4, P6, privacy, and the 2x peak-memory note.

## Capabilities

### New Capabilities
None.

### Modified Capabilities
- `sample-source`:
  - R-SS-5: mention the `INT_MAX` clamp.
  - R-SS-7: stop waits for in-flight work and never kills the thread by force. Adds a [Manual] scenario for a stalled read.
  - R-SS-9: specify the failed-clear behaviour. The previous source stays active, and the label reads "Error: No sample" or "Error: <name>". Adds a [Manual] scenario.
- `build-harness`:
  - R-BH-5: the cache key hashes `cmake/JucePin.cmake`. Update the "Plugin job is cached and bounded" scenario to match.

## Approach and work units

All four units go in one PR on `feat/loader-hardening`, which targets `feat/juce-fetch-license`.

| WU | Commit | Items | Lines |
|---|---|---|---|
| 1 | `fix(plugin): wait for loader thread at shutdown` | W2, W-A | ~25 |
| 2 | `test: harden stress, frees and cap tests` | S-A, S1, S2, S4 | ~35 |
| 3 | `docs(core): document cap rationale and collect rule` | S3, S5 | ~7 |
| 4 | `build: single JUCE pin file and CI cache key` | V1, V5 | ~25 |

Each unit also carries its spec deltas. The total is about 130-150 lines.

Delivery forecast:
- Decision needed before apply: No
- Chained PRs recommended: No
- 400-line budget risk: Low

## Affected Areas

| Area | Impact |
|---|---|
| `Source/plugin/SampleLoader.cpp` | Modified |
| `Source/core/SourceExchange.h`, `SampleBuilder.h` | Comments |
| `tests/test_generation_gate.cpp`, `test_engine_realtime.cpp`, `test_sample_builder.cpp` | Modified |
| `cmake/JucePin.cmake` | New |
| `CMakeLists.txt`, `.github/workflows/ci.yml` | Modified |

## Risks

| Risk | Likelihood | Mitigation |
|---|---|---|
| A stalled share now hangs the host on close instead of being killed | Low | Accepted tradeoff for correctness. A bounded close needs approach C. |
| W2 and W-A can only be verified manually | Med | [Manual] scenarios |
| The S-A wait hangs if the gate never commits | Low | Deadline, then fail |
| The base PR changes before it merges | Med | Rebase. WU4 is isolated. |

## Rollback Plan

Revert the affected commit. Each work unit is independent. Reverting WU4 restores the inline SHA and the old cache key.

## Success Criteria

- [ ] No `stopThread` call uses a finite timeout. The stalled-read scenario shuts down without "killing thread by force".
- [ ] The new tests pass, and the G5 test fails if no commits occur.
- [ ] The JUCE SHA appears only in `cmake/JucePin.cmake`. The plugin CI job stays green and uses the cache.
- [ ] The spec deltas match the code.

## Proposal question round

Auto mode. These assumptions are for the user to review:
1. Is a possible host hang on a stalled share acceptable on close? Assumed: yes.
2. Should approach B (testable cancellation) be a follow-up change? Assumed: deferred, not scheduled.
3. Should the failed-clear labels stay as they are today? Assumed: yes, documented only.
