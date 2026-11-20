# Verification Report: loader-followups PR-B (Phases 4-6)

Engram: `sdd/loader-followups/verify-report-prb` (observation 700). Mode: Strict TDD. Scope: R-RS-9, R-RS-10, R-BH-6 and planner OOM scenarios. PR-A was verified earlier (`verify-report.md`).

**Verdict: PASS WITH WARNINGS** (0 CRITICAL, 2 WARNING, 3 SUGGESTION; W2 resolved).

## Evidence (fresh runs)

| Check | Result |
|---|---|
| Clean Debug and Release builds, MSVC /W4 /WX | exit 0, no warnings |
| ctest Debug | 220/220 (after remediation) |
| ctest Release | 224/224 |
| `grains_alloc_failure_tests` direct | Debug 2 cases / 5 assertions; Release 6 cases / 224 assertions |
| Perf, Release, `grains_tests "[perf]"` | 96k to 44.1k 5.42 s (REQUIRE under 6 s); 384k to 44.1k 20.09 s (WARN only); 96k to 48k 2.70 s (under 3 s) |

## Checks

1. Hook isolation confirmed: `grains_tests` does not compile `support/AllocationCounter.cpp`; a binary scan of `grains_tests.exe` finds no `armFailure`/`failureFired` symbols.
2. The `GRAINS_ALLOC_SWEEPS` gate (`_MSC_VER && _DEBUG`) is documented in the test file and design.md; Release runs all 6 cases, Debug runs 2.
3. Assertion quality sound: sweep runs n from 1 until no failure fires, bounded at 1000, requires OutOfMemory and a null buffer on every fired run, leak check `allocs == frees` on every run. Mutation 1 (OutOfMemory mapped to Aborted in `SampleBuilder.cpp`) fails the sweep; mutation 2 (wrong catch type) terminates the process. Both reverted, byte-identical.
4. The resampler OOM case is now a hook-based sweep in `grains_alloc_failure_tests` (the 1e12 test was removed); no huge allocation under ASan (W2 resolved).
5. Perf margin about 10% (W3).
6. Production code untouched: `git diff HEAD -- Source` empty.
7. About 350 changed lines, within the 800 budget.

## Issues

**WARNING**
- W1: Windows CI builds Debug only, where the sweeps are compiled out, so R-RS-10 builder scenarios and planner OOM sweeps run only in the Linux ASan job and local Release runs. Recommendation: add a Windows Release build with `ctest -C Release -R allocfail`, or document Linux as the sole gate. **Comment fixed:** the stale `tests/CMakeLists.txt` note now states that Windows CI builds Debug only (sweeps, including the resampler OOM sweep, compiled out) and the sweeps run in the Linux ASan job and local Release runs. The recommendation to add a Windows Release build remains open.
- W2 (RESOLVED): the 1e12 resampler OOM test requested about 4 PB, and ASan's throwing `operator new` is fatal even with `allocator_may_return_null=1`. The test was replaced by a hook-based sweep in `grains_alloc_failure_tests` (replaced `operator new` overrides ASan's) and the ci.yml change was reverted (`git diff HEAD -- .github` empty).
- W3: The 6 s budget leaves about 10% margin. The case is hidden and outside the CI gate; keep it informational.

**SUGGESTION**
- S1: ctest discovery follows the last-built configuration (220 vs 224 tests).
- S2: The settle sweep reuses one generation across iterations; add a comment.
- S3: The R-BH-5 sanitizer-exclusion fallback for `grains_alloc_failure_tests` is not wired and not needed so far.

## Pass 2 (after the hook-based rewrite)

Engram `sdd/loader-followups/verify-report-prb` (obs 700, updated). Verdict: PASS WITH WARNINGS (0 CRITICAL, 3 WARNING, 3 SUGGESTION).

- Fresh runs: Debug 220/220, Release 224/224; `grains_alloc_failure_tests` Debug 2 cases / 5 assertions, Release 6 cases / 224 assertions; perf 96k to 48k 2.82 s (3 s), 96k to 44.1k 5.76 s (6 s), 384k to 44.1k 20.9 s (WARN only).
- `armFailure` symbols are absent from `grains_tests`; `git diff HEAD -- Source` and `-- .github` are empty.
- Mutations in `Resampler.cpp` (status mapping, extra channel on failure, wrong catch type) are all caught; file restored byte-identical.
- W-A (stale CMake comment: it claimed the resampler sweep runs on Windows Debug and said three sweeps): fixed; the comment now says all four sweeps are compiled out there and only the `armFailure` self-tests run.
- W1 (no sweeps on Windows CI) stays open. W3 margin is now about 4% (5.76 s vs 6 s); the case is hidden and outside the CI gate.
- Unverified risk: the Linux ASan/UBSan job with the replaced `operator new` has not run; confirm on the first CI run.
