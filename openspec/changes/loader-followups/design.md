# Design: Loader follow-ups (items 2-5)

## Technical Approach

Move the loader's request bookkeeping out of `SampleLoader` into a JUCE-free `grains::LoadPlanner` in `grains_core`. The planner owns the `GenerationGate` and all state that `lock_` guards today. `SampleLoader` keeps the thread, the JUCE file I/O, staging, `commitGated` and `post`. It converts `juce::String` to and from UTF-8 `std::string` at its edges. The planner's `settle` holds the R4-001 rule, so the rule runs under `[loadplan]` tests in `grains_tests`. PR-B adds the hidden perf cases and the OOM coverage: a huge-ratio test that needs no hooks, and a fail-nth-allocation hook in a separate executable.

## Architecture Decisions

| Decision | Options | Tradeoff | Choice |
|---|---|---|---|
| Seam | template `SampleLoader` on an IO policy; JUCE-linked test executable; core planner | A template still needs JUCE to test. A JUCE executable is heavy in CI | Core `LoadPlanner`. Only the state machine is extracted |
| Gate ownership | the loader owns the gate and passes a reference; the planner owns it | A reference depends on member declaration order | The planner owns it and exposes `gate()` for `commitIf` and `current` |
| Lock | `juce::CriticalSection`; `std::mutex` in the planner | | `std::mutex mutex_` in the planner. Order: `mutex_`, then the gate mutex. Nothing calls the planner from inside a `commitIf` callback |
| Failure model | throw; `noexcept` with no reissue on `bad_alloc` | `settle` and `retarget` run on the loader thread and the prepare thread | `request` may throw `bad_alloc` with the strong guarantee: copies are made before the lock. `retarget`, `take`, `settle`, `published` and `cleared` are `noexcept`. If a copy fails, the call issues nothing and leaves state unchanged |
| R4-001 rule | processor-side retry; retry any failure; retry only a latest non-Reload failure | Retrying a failed Reload loops forever on a deleted file | `settle(g, Failed)` issues one Reload only when: `g` is still latest, its origin is not `Reload`, `target_ > 0`, and `published_` exists with `rate != target_`. The reissued Reload carries the failed request's status line as its `label` (`settle(g, o, failureStatus)`), because its generation makes the failed request's own post stale in `onLoadResult`; it reposts that line on success or failure, so the error status survives (R-SS-15). Rejected: not bumping the gate (breaks supersede/abort of an in-flight older run and the commit gate); a processor-side status-neutral flag (touches `PluginProcessor`, not unit-testable); settle returning info for the adapter to re-post (same effect, more plumbing) |
| `Origin` | keep the JUCE-side enum; alias it | | `using Origin = grains::LoadOrigin;` Enumerator names are unchanged, so `PluginProcessor` is untouched |
| Fault-injection home | `grains_tests`; `grains_realtime_tests`; a new executable | The replaced global `new` must not reach `grains_tests`. Mixing it with R-GR-8 counts couples two concerns | A new `grains_alloc_failure_tests` that reuses `support/AllocationCounter.cpp` |
| Perf fix (only if the 96k to 44.1k case exceeds 6 s) | skip the lerp row for small `alpha`; a denser table; per-channel threads | The first two change the output and the accuracy tests | Per-channel `std::thread`. Output stays bit-exact. This would be a separate slice |

## Interfaces / Contracts

```cpp
// grains/LoadPlanner.h  (JUCE-free; includes GenerationGate.h)
enum class LoadOrigin { User, Restore, Reload };
enum class LoadOutcome { Published, Cleared, Failed, Aborted };
struct LoadRequest { std::vector<std::string> candidates; std::string original, displayName;
                     LoadOrigin origin = LoadOrigin::User; std::uint64_t generation = 0;
                     double targetRate = 0.0; std::string label; /* Reload only */ };
struct PublishedSource { std::string path, original, displayName, label; double rate = 0.0; };

class LoadPlanner {
public:
    std::uint64_t request(LoadRequest r);          // stamps gen + target_, sets latest_/pending_
    bool retarget(double rate) noexcept;           // true when a request was issued
    std::optional<LoadRequest> take() noexcept;    // loader thread: moves out pending_
    void published(PublishedSource p) noexcept;    // after an accepted commit, outside the gate
    void cleared() noexcept;                       // after an accepted clear, outside the gate
    bool settle(std::uint64_t gen, LoadOutcome o, std::string failureStatus = {}) noexcept; // settled_ = gen; R4-001; true => notify
    GenerationGate& gate() noexcept;
    double target() const noexcept;                // test/diagnostic snapshot
private:
    mutable std::mutex mutex_; GenerationGate gate_;
    double target_ = 0.0; std::optional<LoadRequest> latest_, pending_;
    std::uint64_t settled_ = 0; std::optional<PublishedSource> published_;
};
```

The `retarget` logic moves unchanged, with the same three branches. Rates that are not finite or not positive, and a rate equal to `target_`, return false. A latest request that has not settled is reissued with a new generation at the new rate. Otherwise, if `published_->rate != rate`, the planner issues a `Reload` built from `published_`.

```cpp
// tests/support/AllocationCounter.h additions (namespace grains_test)
void armFailure(std::size_t n) noexcept; // the n-th allocation (1-based, any new variant) from now fails once
void disarmFailure() noexcept;
bool failureFired() noexcept;            // true if the armed failure has fired
```

`plainAlloc` and `alignedAlloc` decrement an atomic countdown. When it reaches zero they return `nullptr`: the throwing forms raise `bad_alloc` through `orThrow` and the nothrow forms return null.

## Data Flow

    request/retarget (any thread) --mutex_--> gate.next() --> latest_, pending_ --> notify
    run: take() -> loadImpl(r) -> outcome
         loadImpl: resolve -> decode -> build -> stage -> commitGated (gate mutex only)
                   -> published()/cleared() (mutex_) -> post
    load: settle(gen, outcome) --mutex_--> [R4-001: gate.next(), Reload -> pending_] -> notify

`loadImpl` returns:
- `Published` on an accepted commit.
- `Cleared` on an accepted clear.
- `Failed` from `fail()` while still current, or when staging fails.
- `Aborted` from every other silent return.

## File Changes

| PR | File | Action | Lines |
|---|---|---|---|
| A | `Source/core/include/grains/LoadPlanner.h`, `Source/core/src/LoadPlanner.cpp` | Create | ~150 |
| A | `Source/core/CMakeLists.txt`, `tests/CMakeLists.txt` | Add sources | 2 |
| A | `Source/plugin/SampleLoader.{h,cpp}` | Delegate to the planner; remove `lock_` and the state; `loadImpl` returns an outcome | ~90 |
| A | `tests/test_load_planner.cpp` | Create | ~220 |
| B | `tests/test_resampler.cpp` | Perf cases and the huge-ratio OOM case | ~50 |
| B | `tests/support/AllocationCounter.{h,cpp}` | `armFailure` | ~35 |
| B | `tests/test_alloc_failure.cpp`, `tests/CMakeLists.txt` | New executable and sweeps | ~110 |

PR-A is about 460 lines. PR-B is about 195 lines and is stacked on PR-A, because its planner OOM cases use `LoadPlanner`.

## Testing Strategy

Strict TDD. Each row starts as a failing test.

| Area | Cases |
|---|---|
| `[loadplan]` request/take | Generations increase and match `gate().current()`. The request copies the target. A newer request replaces the pending one. `take` returns a request once, then `nullopt` |
| retarget | Invalid rates and the same rate are no-ops. A pending request is replaced at the new rate. An in-flight request (taken, not settled) is reissued with the same fields. After settling with a different published rate, the Reload carries `path`, `original`, `displayName`, `label` and `rate`. An equal rate, or nothing published after `cleared()`, issues nothing. If `published()` runs before `settle`, the request still counts as in flight and is reissued |
| R4-001 | Red first: `retarget(B)` issues Reload R. `take` R, then `request` U. `settle(R, Aborted)` issues nothing. `take` U, then `settle(U, Failed)` issues a Reload at B. These issue no Reload: a failed Reload, a reissued Reload that fails (only one attempt), a superseded failure, `target_ == 0`, a published rate equal to the target, nothing published, and `Aborted` while latest |
| Concurrency | Two threads run `request`/`retarget` and `take`/`commitIf`/`published`/`settle` 10k times. The test finishes, which shows no deadlock, and the last request wins |
| 5a `[resampler]` | 1000 frames, `src = 1` and `dst = 1e12`, so the output is 1e15 frames (4 PB, above the 128 TB virtual address limit and below `max_size`). Expect `OutOfMemory` and no channels |
| 5b `[allocfail]` | For n = 1, 2, …: arm n, call the builder (passthrough, 48k to 44.1k, mono and stereo, 1000 frames), then disarm. If the failure fired, expect `OutOfMemory` and a null buffer. If it did not fire, expect `None` and stop. Planner: with a failure armed, `retarget` returns false and `target()` is unchanged. `settle` with a failure armed issues nothing and does not throw |
| 4 `[.][perf]` | "300 s stereo 96k to 44.1k within 6 s" (`REQUIRE`). 384k to 44.1k only reports the time (`WARN`), with no budget. Run both manually in Release and record the times in R-RS-9 |

Plugin parity: manual smoke D.1-D.5 (load, restore, rate change, missing file).

## Threat Matrix

N/A: no routing, shell, subprocess, VCS/PR automation, executable-file classification or process-integration boundary.

## Migration / Rollout

No migration is needed. Saved state and the processor API are unchanged.

## Open Questions

- [ ] MSVC Debug: the iterator-debug proxy allocation inside a `noexcept` move (when NRVO is not applied) could `terminate` under the 5b sweep. Verify in apply. If it occurs, run `[allocfail]` in Release only.
