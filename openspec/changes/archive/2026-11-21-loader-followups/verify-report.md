# Verification Report: loader-followups (PR-A, Phases 1-3)

Engram: `sdd/loader-followups/verify-report` (observation 699, condensed). Mode: Strict TDD, scope PR-A only. PR-B items (R-RS-9, R-RS-10, R-BH-6, planner OOM scenarios) are deferred by design.

**Verdict: PASS WITH WARNINGS** (0 CRITICAL, 3 WARNING, 3 SUGGESTION).

## Completeness

- Tasks 1.1-1.5, 2.1-2.4 and 3.1-3.3: 12/12 complete and matching the code.
- Changed lines: 730 touched (608 added, 122 removed); tracked 183 (61/122), new files 547 (`LoadPlanner.h` 79, `LoadPlanner.cpp` 120, `test_load_planner.cpp` 348). Within the 800 budget.

## Build and test evidence

| Command | Exit | Result |
|---|---|---|
| Debug `--clean-first`, MSVC /W4 /WX | 0 | no warnings |
| Release `--clean-first` | 0 | no warnings |
| ctest Debug | 0 | 214/214 |
| ctest Release | 0 | 214/214 |
| `grains_tests "[loadplan]"` Debug and Release | 0 | 21/21 (110 assertions) |
| `GrainsPlugin_Standalone` (`build-plugin`, Debug) | 0 | builds and links; `PluginProcessor` untouched |

`cmake -A x64` fails on the existing `build` dir (generator platform mismatch), so the existing cache was reused. No coverage or linter tools available.

## TDD compliance

- RED 1: 11/11 failed against the stub. RED 2: 2 of 9 R4-001 cases failed. GREEN: 21/21.
- Triangulation adequate: 9 R4-001 cases with different outcomes.
- `SampleLoader.*` has no automated tests (JUCE-bound; parity is manual by spec).

### Mutation check (local, reverted, `cmp` confirmed byte-identical)

| Mutation of `LoadPlanner::settle` | Result |
|---|---|
| Drop latest-generation guard | 1 test fails |
| Drop Reload guard | 2 tests fail |
| Drop `target_ > 0` guard | 1 test fails |
| Drop rate-equal guard | 1 test fails |
| Reissue on Aborted | 1 test fails |
| Reissue on Published | NOT caught (W1) |

The `!published_` guard cannot be mutated safely (null dereference); the "nothing published" test covers it. No tautologies, ghost loops or assertion-free tests.

## Spec compliance (PR-A)

- Compliant: stale-rate recovery after failed superseding User request; failed Restore after clear; no reload loop on deleted file; failed request no longer latest; matching rate; Aborted outcome; no target known; nothing published; retarget no-op / invalid rate / different rate issues one Reload; request captures target; passthrough before any target; latest wins; planner builds without JUCE.
- Partial: "no reissue after a failed Reload" (S1); concurrency (W2).
- Untested: Published outcome does not reissue (W1).
- Deferred: Manual scenarios and loader parity (D.1-D.5); PR-B requirements.

## Design coherence

- Lock order respected: `request`, `retarget`, `settle` take the planner mutex then call `gate_.next()`; the commit callback only calls `exchange_.commit`; `published()`/`cleared()` run after `commitGated` returns.
- R4-001 conditions match the design exactly.
- `SampleLoader` outcome mapping correct (rejected commit / not-current abort = Aborted; Restore failure with accepted clear = Failed; Reload failure keeps its label).
- Deviations (const `gate()` overload, try/catch in `retarget` and `settle`) are harmless.

## Issues

**CRITICAL:** none.

**WARNING**
- W1: No test asserts a Published outcome does not reissue (`tests/test_load_planner.cpp` lines 47, 204 ignore `settle(Published)` return values). Add a stale-published-rate case where a latest User request settles Published; CHECK_FALSE the settle and an empty `take()`.
- W2: Concurrency test asserts only no deadlock and last request survives; invariants (settled <= latest, pending not older than latest) are private and unasserted; no sanitizer run.
- W3: `settled_ = gen` is unconditional; benign with a single loader thread, but could regress `settled_` if a stale generation settled late.

**SUGGESTION**
- S1: Call `settle` twice on a failed Reload to exercise "repeating the failure notification issues none".
- S2: Add a manual checklist for the SampleLoader Restore-failure-clears path.
- S3: The documented `-A x64` configure command fails on build dirs created without it (docs fix).

## Next

Add the W1 and S1 tests, then bounded review, then PR-A commit. Outstanding: manual host checks D.1-D.5 and a sanitizer run.
