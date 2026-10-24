# Apply Progress: sample-loader

Mode: Strict TDD. Delivery: single PR, size:exception recorded. Build dir `build/apply-sl` (MSVC, plugin OFF).
Group 1 (tasks 1.1-1.12) DONE. 1.13 gate checks done, commit pending (orchestrator). Group 2 (plugin) not started.

## TDD Cycle Evidence

| Task | RED (how it failed) | GREEN | Notes |
|------|---------------------|-------|-------|
| 1.1/1.2 | Test "allocation counter counts non-null frees": MSVC C2039 `frees` is not a member of AllocationScope | Added `gFrees`, `noteFree` in plainFree/alignedFree, `frees()`; 96/96 | Test also covers null delete and out-of-scope free |
| 1.3/1.4 | New test_source_exchange.cpp: C1083 cannot open `grains/SourceExchange.h` | Header per design contract; empty/publish-acquire/unchanged pass | |
| 1.5 | Characterization (passed immediately after 1.4) | n/a | Sabotage proved fail: S1 no delete of superseded pending (3 failures), S3 no deferral check (3 failures), S4 collect not deleting (3 failures). Reverted |
| 1.6 | Characterization | n/a | S2 destructor not deleting current_ (4 failures). Reverted |
| 1.7 | Characterization (stress) | n/a | S5 consumer deletes current_ instead of retiring: `consumerFrees == 3869`. Reverted |
| 1.8 | Characterization (2 realtime cases in test_engine_realtime.cpp) | n/a | S7 consumer free: `frees == 6`; S8 `new int` in acquire: `allocs == 1`. Reverted |
| 1.9 | Characterization (engine swap) | n/a | S6 removed `pool_.reset()` in setSource: `activeGrainCount() == 9`. Reverted |
| 1.10/1.11 | test_sample_builder.cpp: C1083 cannot open `grains/SampleBuilder.h` | SampleBuilder.h/.cpp + CMake line; 3 cases pass | |
| 1.12 | Characterization (table-driven, 9 rejection cases) | n/a | S9 isfinite removed (NaN/Inf fail), S10 truncated flag removed, S11 mismatch check disabled. Reverted |

REFACTOR: none needed beyond a clarified comment. `git diff` shows no sabotage (tracked sources diffed against backups after each run).

## Work Unit Evidence

| Evidence | Value |
|---|---|
| Focused test | `build/apply-sl/tests/Debug/grains_tests.exe "[exchange],[builder]"` and `grains_realtime_tests.exe "[exchange]"`: all pass |
| Full suite | `ctest --test-dir build/apply-sl -C Debug`: 111/111 pass (baseline 95) |
| Runtime harness | Catch2 stress (2 threads, 5000 publishes) and realtime AllocationScope tests |
| Rollback boundary | Revert commit 1 (core + tests); plugin commit depends on it |

## Metrics
Group-1 changed lines ~638 (tracked diff 102 + new files 536) vs forecast ~415; over by ~220 (tests are ~460 of it). Shrink order not applied; total with plugin forecast (~315) would reach ~950 > 800 budget. Orchestrator should decide (trim tests or accept).

Warnings: MSVC /W4 /WX clean. clang/gcc not installed; reviewed manually for -Wall -Wextra -Wpedantic.
