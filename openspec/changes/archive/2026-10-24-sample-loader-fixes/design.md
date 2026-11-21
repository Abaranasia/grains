# Design: Sample Loader Fixes

## Technical Approach

Close W1 by making "is the request still current?" and "change the exchange" one critical section with respect to `request()` bumping the generation. Two small JUCE-free additions in `grains` core:

1. `GenerationGate`: owns the generation counter and a `std::mutex`. `next()` bumps the generation under the mutex. `commitIf(gen, fn)` runs `fn` under the mutex only when `gen` is still current.
2. `SourceExchange` stage/commit split:
   - `stage()` allocates the `Box` outside any lock.
   - `commit()` only exchanges the `pending_` pointer and **returns** the displaced unseen Box instead of deleting it. The caller frees that Box after the gate unlocks, on the loader thread.
   - `publish()` becomes `stage()` plus `commit()`, so the existing tests and contract stay unchanged.

`SampleLoader` sends all three exchange-mutation sites through the gate: the empty-path restore clear (line 65), the restore-failure clear in `fail()` (line 80), and the publish after decode (line 124). The audio-thread code (`acquire`) does not change.

## Architecture Decisions

| Option | Tradeoff | Decision |
|---|---|---|
| Gate plus stage/commit split (core, testable) | Two small API additions | **Chosen** |
| Check `gen_`, then publish without a lock | A time-of-check/time-of-use (TOCTOU) window remains | Rejected |
| Hold `lock_` (juce::CriticalSection) around `publish()` | Allocation and free happen while the message thread can block in `request()`. The logic cannot be tested without JUCE | Rejected |
| `compare_exchange` on a "committed generation" | Still races unless `request()` takes part | Rejected |
| Pre-allocate a clear Box | Adds state for a rare out-of-memory case | Rejected (see C1 row) |

| Question | Decision | Rationale |
|---|---|---|
| Does the gate replace `gen_`? | Yes. `latestGeneration()` returns `gate_.current()` | One source of truth |
| Does the gate replace `lock_`? | No. `lock_` still guards the `pending_` request mailbox | It protects a different invariant |
| Lock order | `request()` takes `lock_`, then the gate mutex: `pending_ = Request{path, origin, gate_.next()}`. The loader never holds both | Each request is stored together with its generation even with several concurrent callers. There is no deadlock |
| What runs inside `commitIf` | Only `exchange_.commit()`. `fn` MUST NOT re-enter the gate (`std::mutex` is non-recursive) | `request()` blocks for nanoseconds at most |
| Stage failure (C1) | `Staged` is default-constructible and empty. `commit(empty)` is a no-op that returns an empty `Staged` and never exchanges a null pointer into `pending_`. Publish site: an empty `stage()` result skips the gate and posts "Error: <name>". Restore-clear site: an empty `stageClear()` result posts "Error: <name>" and keeps the previous source | Out-of-memory only. This is a **known deviation** from R-SS-9 silence |
| `collect()` | Called after an accepted commit returns, outside the lock | Collect must follow every accepted commit/publish |
| Shutdown | Keep the advisory `threadShouldExit()` check before `stage()`. A commit that races `stop()` is safe, because the exchange destructor frees all Boxes | No extra synchronisation needed |

## Data Flow

    message thread                    loader thread
    request(): lock_ -> gate.next()   s = stage(buf)        [alloc, no lock; empty -> post Error]
               pending_ = req         ok = gate.commitIf(gen, [&]{ displaced = x.commit(std::move(s)); })
                                      -- unlock --
                                      ~displaced / ~s       [free, no lock]
                                      if ok: collect(); post()
    audio thread: acquire()           (unchanged: wait-free, never frees)

## File Changes

| File | Action | Description |
|---|---|---|
| `Source/core/include/grains/GenerationGate.h` | Create | Gate (~40 lines) |
| `Source/core/include/grains/SourceExchange.h` | Modify | Adds `Staged`, `stage`, `stageClear`, `commit`. `publish` delegates to them. Update the contract comment: collect must follow every **accepted** commit/publish, and `commit` returns the displaced Box instead of freeing it (~35 lines) |
| `Source/plugin/SampleLoader.h` | Modify | `gate_` replaces `gen_` (~5 lines) |
| `Source/plugin/SampleLoader.cpp` | Modify | `request()` uses `next()`. A `commitGated(r, Staged)` helper serves all three sites (~40 lines) |
| `tests/test_generation_gate.cpp` | Create | G1–G3, G5, G7 (~90 lines) |
| `tests/test_source_exchange.cpp` | Modify | X1–X3 and the gated tests G4, G6, which reuse `Tracked` (~90 lines) |
| `tests/test_engine_realtime.cpp` | Modify | R1 (~30 lines) |
| `tests/CMakeLists.txt` | Modify | Register the new file in `grains_tests` |
| `openspec/changes/sample-loader-fixes/specs/sample-source/spec.md` | Modify | R-SS-9: an empty restore path is silent with the label "No sample". "Missing: <name>" applies only to a non-empty path that fails |

Estimate: about 330 lines. This fits in a single PR.

## Interfaces / Contracts

```cpp
// SourceExchange<T>
class Staged { public: Staged() noexcept = default; explicit operator bool() const noexcept;
               private: std::unique_ptr<Box> box_; };     // move-only
Staged stage(std::unique_ptr<T> next) noexcept; // new(nothrow) Box; empty on failure
Staged stageClear() noexcept;                   // stage(nullptr)
Staged commit(Staged&& s) noexcept;   // empty s: no-op, returns empty; else pending_.exchange(acq_rel), returns displaced
bool publish(std::unique_ptr<T> next) noexcept; // stage + commit; false if stage failed

// GenerationGate
std::uint64_t next() noexcept;    // lock; v = gen_.load(relaxed) + 1; gen_.store(v, relaxed); return v
std::uint64_t current() const noexcept; // relaxed load; advisory
template <class F> bool commitIf(std::uint64_t gen, F&& f) noexcept;
  // static_assert(std::is_nothrow_invocable_v<F&>); lock; gen == gen_ ? (f(), true) : false
```

Memory orderings for `pending_`, `retired_`, and `acquire()` are unchanged. The gate's atomic is written only under the mutex, and the authoritative comparison happens under the same mutex, so relaxed ordering is enough.

Threading invariants:
- There is a single producer, the loader thread.
- The consumer never frees.
- Nothing is allocated or freed while the gate mutex is held.
- A rejected or displaced `Staged` is destroyed on the loader thread after the unlock.

## Testing Strategy

The tests follow Strict TDD: each test is written RED first, then made GREEN with minimal code.

| # | Test |
|---|---|
| G1 | `next()` returns 1, 2, 3, and `current()` matches |
| G2 | `commitIf(current())` runs `fn` once and returns true |
| G3 | After `next()`, `commitIf(old)` returns false and `fn` is not run |
| G4 | Publish and acquire A. `stage(B)`, `next()`, `commitIf(old)` is false. `acquire` still returns A with `changed == false`. B stays alive until the caller drops the `Staged`, then it is destroyed exactly once |
| G5 | Bounded stress: 100k calls to `next()` on one thread while another thread loops `commitIf(current(), fn)`. `fn` increments a `mismatches` counter if `current() != gen`. `REQUIRE(mismatches == 0)`. After join, `REQUIRE(commitIf(current()))`. Never assert the number of commits during the run |
| G6 | Superseded clear: publish and acquire A, `stageClear()`, `next()`, `commitIf(old)` is false. `acquire` returns A with `changed == false`. `live == 0` after destruction |
| G7 | Deterministic exclusion (the real RED): inside `fn`, start a thread that calls `next()`, sleep about 20 ms, and check that `current()` is unchanged. After `commitIf` returns, join the thread and check `current() == old + 1` |
| X1 | `stage()` plus `commit()` delivers the value once through `acquire` |
| X2 | Gated: an unseen pending Box exists. Inside `fn`, `Tracked::destroyed` is unchanged. `live == 2` after `commitIf`. `destroyed == 1` after the displaced `Staged` drops. Also check that `commit(empty)` leaves the pending Box in place |
| X3 | Gated `stageClear()` plus commit clears the source (`nullptr`, `changed`). `live == 0` after the exchange is destroyed |
| X4 | The existing exchange tests pass unchanged |
| R1 | Realtime: the `Staged` is built before the `AllocationScope`. `commit()` and `acquire()` inside the scope count 0 allocations and 0 frees. The displaced Box is dropped after the scope |
| R2 | The existing realtime tests stay at 0 |

Manual checks (Standalone):
1. Load a large WAV, then within about 100 ms load a corrupt or missing file. The status ends at "Error: ..." and the previous source keeps playing. The large file is never activated.
2. Two quick valid loads: only the second file plays and shows "Loaded".
3. Restore a session whose file is missing: the status shows "Missing: <name>" and output is silent. Restore a session with an empty path: the status shows "No sample" and output is silent.
4. Restore a session whose file is missing, then immediately load a valid file: the valid file plays.
5. Load a file longer than 5 minutes: the status shows "Loaded: <name> (truncated to 5 min)". Load a short file: the status shows "Loaded: <name>" without that text.

## Threat Matrix

N/A: this change has no routing, shell, subprocess, VCS/PR automation, executable-file classification, or process-integration boundary.

## Migration / Rollout

No migration required.

## Open Questions

- [ ] None blocking. Assumption: a `std::mutex::lock` failure inside a `noexcept` function terminates the program, which is acceptable. The out-of-memory deviation on restore clear is accepted (C1).
