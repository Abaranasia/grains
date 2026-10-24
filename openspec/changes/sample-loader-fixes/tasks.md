# Tasks: sample-loader-fixes

Specs: `specs/sample-source` (R-SS-7, R-SS-9, R-SS-10, R-SS-11), `specs/plugin-shell` (R-PS-2, wording only). Design: `design.md`. Strict TDD: each core task is RED (failing test first, record the failure mode) then GREEN (minimal code).

Commands (build dir `build/apply-sl`, plugin OFF for core):
- Build: `cmake --build build/apply-sl --config Debug`
- Test: `ctest --test-dir build/apply-sl -C Debug --output-on-failure` (baseline 111 tests, must stay green after each task)
- Plugin: `cmake --build build-plugin --target grains_plugin --config Debug`

## Review Workload Forecast

| Field | Value |
|-------|-------|
| Estimated changed lines | ~340 (commit 1 core ~285, commit 2 plugin ~55, docs 0) |
| Review budget | 800 lines; forecast is about 42% of budget |
| 400-line budget risk | Low |
| Chained PRs recommended | No |
| Delivery strategy | single-pr, 2 commits |

Decision needed before apply: No.
Chained PRs recommended: No
400-line budget risk: Low

Per-commit estimate:

| Commit | Files | Lines |
|---|---|---|
| 1 `fix(core)` | GenerationGate.h ~40, SourceExchange.h ~30, test_generation_gate.cpp ~90, test_source_exchange.cpp ~90, test_engine_realtime.cpp ~30, tests/CMakeLists.txt ~1 | ~281 |
| 2 `fix(plugin)` | SampleLoader.h ~5, SampleLoader.cpp ~40, SourceExchange.h contract comment ~6 | ~51 |
| 3 docs | none (delta specs live in the change folder and are merged at archive) | 0 |

### Suggested Work Units

| Unit | Goal | Likely PR | Focused test command | Runtime harness | Rollback boundary |
|------|------|-----------|----------------------|-----------------|-------------------|
| 1 | Gate, stage/commit split, tests | PR 1 (commit 1) | `ctest --test-dir build/apply-sl -C Debug --output-on-failure -R "gate|exchange|realtime"` | Catch2 stress, exclusion and realtime tests | Revert commit 1 (plugin commit depends on it) |
| 2 | Plugin loader wiring | PR 1 (commit 2) | `cmake --build build-plugin --target grains_plugin --config Debug` | Manual Standalone task 2.5 | Revert commit 2 only |

## Commit 1: `fix(core): add GenerationGate and staged SourceExchange commit`

Tasks 1.1 to 1.14 are sequential (shared files). Each RED step records the failure mode (compile error or assertion) in the task note or commit body.

### Phase 1: GenerationGate
- [x] 1.0 Baseline check before edits: build and run ctest on `build/apply-sl`, confirm 111/111 and zero warnings.
- [x] 1.1 RED: create `tests/test_generation_gate.cpp` with G1 (`next()` gives 1, 2, 3 and `current()` matches); register it in `tests/CMakeLists.txt`. Expected failure: `grains/GenerationGate.h` missing (compile error). (R-SS-11)
- [x] 1.2 GREEN: create `Source/core/include/grains/GenerationGate.h` with `next()`, `current()` and `commitIf` (mutex plus relaxed atomic, `static_assert` nothrow-invocable). G1 passes. (R-SS-11)
- [x] 1.3 RED/GREEN: add G2 (`commitIf(current())` runs `fn` once, returns true) and G3 (after `next()`, `commitIf(old)` returns false, `fn` not run). Record whether each fails or is a characterization pass. (R-SS-11)
- [x] 1.4 RED/GREEN: add G7 deterministic exclusion: inside `fn` start a thread calling `next()`, sleep about 20 ms, check `current()` unchanged; after `commitIf` returns, join and check `current() == old + 1`. RED: temporarily make `commitIf` run `fn` without holding the mutex (or compare outside the lock) and record the assertion failure; then restore the locked implementation for GREEN. (R-SS-7, R-SS-11)
- [x] 1.5 RED/GREEN: add G5 bounded stress: 100k `next()` calls on one thread while another loops `commitIf(current(), fn)`; `fn` increments `mismatches` when `current() != gen`; `REQUIRE(mismatches == 0)`; after join `REQUIRE(commitIf(current()))`; never assert a commit count. (R-SS-11)

### Phase 2: SourceExchange stage/commit
- [x] 1.6 RED: in `tests/test_source_exchange.cpp` add X1 (`stage()` plus `commit()` delivers the value once through `acquire`). Expected failure: `stage`/`commit`/`Staged` do not exist (compile error). (R-SS-11)
- [x] 1.7 GREEN: in `Source/core/include/grains/SourceExchange.h` add move-only `Staged`, `stage`, `stageClear`, `commit` (returns displaced Box, empty `Staged` is a no-op, never exchanges a null Box); make `publish` delegate to `stage` plus `commit`. X1 passes and all existing exchange tests pass unchanged (X4, R2). (R-SS-1, R-SS-2, R-SS-11)
- [x] 1.8 RED/GREEN: add X2: with an unseen pending Box, inside the `commitIf` fn `Tracked::destroyed` is unchanged; `live == 2` after `commitIf`; `destroyed == 1` after the displaced `Staged` drops; `commit(empty)` leaves the pending Box in place. RED: temporarily let `commit` delete the displaced Box and record the failing `destroyed` check; restore for GREEN. (R-SS-11)
- [x] 1.9 RED/GREEN: add X3: gated `stageClear()` plus commit yields `nullptr` with `changed`; no empty buffer published; `live == 0` after the exchange is destroyed. (R-SS-9, R-SS-11)
- [x] 1.10 RED/GREEN: add G4: publish and acquire A; `stage(B)`, `next()`, `commitIf(old)` is false; `acquire` still returns A with `changed == false`; B alive until the caller drops its `Staged`, then destroyed exactly once. RED: record failure if the staged Box is leaked or the exchange changed (for example by temporarily ignoring the `commitIf` result and committing anyway). (R-SS-7, R-SS-11)
- [x] 1.11 RED/GREEN: add G6: publish and acquire A; `stageClear()`, `next()`, `commitIf(old)` is false; `acquire` returns A with `changed == false`; `live == 0` after destruction. RED: same technique as 1.10 (commit regardless of the gate result) and record the failure. (R-SS-7, R-SS-9, R-SS-11)

### Phase 3: Realtime
- [x] 1.12 RED/GREEN: add R1 in `tests/test_engine_realtime.cpp`: build the `Staged` before the `AllocationScope`; inside the scope `commit()` and `acquire()` count 0 allocations and 0 frees; drop the displaced Box after the scope. RED: record the failure from a missing symbol (compile error) or a nonzero count if `commit` frees. Existing realtime tests stay at 0 (R2). (R-SS-3, R-SS-11)

### Phase 4: Gate
- [x] 1.13 Commit gate: full Debug build with zero warnings (`/WX`), `ctest --test-dir build/apply-sl -C Debug --output-on-failure` all green (111 plus new tests), and `rg -n "juce" Source/core` has no include matches. Run the focused command `-R "gate|exchange|realtime"` too.
- [ ] 1.14 (PENDING ORCHESTRATOR: gates passed, commit not made by apply) Commit `fix(core): add GenerationGate and staged SourceExchange commit` (conventional commit, no AI attribution). Stage reviewed paths without changing content before any pre-commit receipt validation.

## Commit 2: `fix(plugin): gate SampleLoader exchange mutations`

JUCE glue has no unit tests; RED is a failing or inconsistent build or verification step. Tasks 2.1 to 2.3 are sequential.

- [x] 2.1 `Source/plugin/SampleLoader.h`: `gate_` (GenerationGate) replaces `gen_`; `latestGeneration()` returns `gate_.current()`. `Source/plugin/SampleLoader.cpp`: `request()` stores `Request{path, origin, gate_.next()}` while holding `lock_` (order `lock_` then gate mutex; the loader never holds both). Verify: plugin build compiles. (R-SS-7)
- [x] 2.2 `Source/plugin/SampleLoader.cpp`: add `commitGated(r, Staged)` helper using `gate_.commitIf(r.gen, [&]{ displaced = exchange.commit(std::move(s)); })`; free `displaced` and any rejected `Staged` after unlock; call `collect()` and post status only when accepted. Route all three exchange-mutation sites through it: the empty-path restore clear (about line 65), the restore-failure clear in `fail()` (about line 80), and the publish after decode (about line 124). An empty `stage()` result at the publish site skips the gate and posts "Error: <name>"; an empty `stageClear()` at the restore-clear site posts "Error: <name>" and keeps the previous source. Keep the advisory `threadShouldExit()` check before `stage()`. Empty restore path label reads "No sample"; non-empty failing path reads "Missing: <name>". (R-SS-7, R-SS-9)
- [x] 2.3 (done in core group) `Source/core/include/grains/SourceExchange.h`: update the contract comment: `collect` must follow every accepted commit or publish, and `commit` returns the displaced Box instead of freeing it. Comment only, no behaviour change. (R-SS-11)
- [ ] 2.4 (gates passed; commit PENDING ORCHESTRATOR) Commit gate: `cmake --build build/apply-sl --config Debug` zero warnings and `ctest --test-dir build/apply-sl -C Debug --output-on-failure` all green; `cmake --build build-plugin --target grains_plugin --config Debug` succeeds with no new warnings. Commit `fix(plugin): gate SampleLoader exchange mutations` (conventional commit, no AI attribution).

### 2.5 Manual verification in Standalone (human required, pending user)

- [ ] 2.5 MANUAL (pending user; Debug Standalone built from commit 2): (1) load a large WAV, then within about 100 ms load a corrupt or missing file: status ends at "Error: ...", the previous source keeps playing, the large file is never activated; (2) two quick valid loads: only the second plays and shows "Loaded"; (3) restore a session with a missing file: status "Missing: <name>" and silent output; restore a session with an empty path: status "No sample" and silent output; (4) restore a session with a missing file, then immediately load a valid file: the valid file plays; (5) load a file longer than 5 minutes: status "Loaded: <name> (truncated to 5 min)"; load a short file: status "Loaded: <name>" without that text.

## Commit 3: docs (not needed)

- [x] 3.1 Confirm no separate docs commit is required: delta specs (`specs/sample-source/spec.md`, `specs/plugin-shell/spec.md`) live in the change folder and are merged into main specs at archive, and the README needs no change. Record "no docs commit" as the outcome.

## Requirement Traceability

| Requirement | Tasks |
|---|---|
| R-SS-7 loader behaviour (atomic check and commit, superseded never publishes or clears) | 1.4, 1.10, 1.11, 2.1, 2.2, 2.5 |
| R-SS-9 missing file / empty path, explicit clear, superseded restore does not clear | 1.9, 1.11, 2.2, 2.5 |
| R-SS-10 editor truncation status | 2.5 (wording and status already implemented; verified manually) |
| R-SS-11 generation gate and staged exchange | 1.1 to 1.12, 2.3 |
| R-SS-1, R-SS-2 exchange contract unchanged | 1.7 (X4 existing tests) |
| R-SS-3 RT-safe consumer | 1.12, 1.13 |
| R-PS-2 placeholder metadata (wording only) | 3.1 (no code change) |

Scenario coverage: Current generation commits (1.3), Stale generation does not commit (1.3), Rejected commit leaves exchange unchanged (1.10), Superseded pending buffer returned (1.8), Superseded clear rejected (1.11), Current clear without publishing (1.9), No new generation between check and commit (1.4, 1.5), Allocation-free consumer (1.12).
