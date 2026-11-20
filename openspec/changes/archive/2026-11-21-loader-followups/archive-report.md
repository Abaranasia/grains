# Archive Report: loader-followups

**Archived**: 2026-11-21
**Archive path**: `openspec/changes/archive/2026-11-21-loader-followups/` (copies written by the archive executor; the original folder `openspec/changes/loader-followups/` is still present, see Risks 1)
**Mode**: hybrid (openspec + Engram `sdd/loader-followups/*`, project `grains`)
**Branch**: `feat/loader-followups-tests` (PR-B, 2b9c96e) on top of PR-A (30a9cf0, `feat/load-planner`)
**Archive status**: intentional archive before merge (maintainer-directed), with warnings. Spec merge and archive copy complete; original folder removal pending (no delete tool).
**Verdict carried from verify**: PASS WITH WARNINGS for PR-A (0 CRITICAL, `verify-report.md`); PASS WITH WARNINGS for PR-B (0 CRITICAL, `verify-report-prb.md`).

## Overrides Recorded

1. **Intentional archive before merge.** Maintainer-directed. The change is archived before the PRs merge to `main`.
2. **Manual host checks D.1-D.6 pending.** Not run. Owner: user. D.6 was added by the PR-A review remediation (status line after a reissued Reload).
3. **Linux sanitizer job not yet run** with the replaced global `operator new` (see Outstanding Items).

## Review Gate

Structured status supplied by the orchestrator (not re-run by this executor, no shell available):

```yaml
reviewGate:
  result: allow   # gentle-ai review validate --gate pre-commit --cwd . --lineage review-009062a5ad49a498
  allowed: true
  lineages:
    PR-A: review-41cdaf6620627ac9 (approved)
    PR-B: review-009062a5ad49a498 (approved)
```

Live `gentle-ai review validate` was not run by this executor. The receipts were not re-read from disk.

## Task Completion Gate

`openspec/changes/loader-followups/tasks.md` has **0 unchecked `- [ ]` items** (13 checked; 6.2 is ticked with the note "not triggered: 5.4 s"). No stale-checkbox reconciliation was performed and no checkbox was changed.

Audit note: `apply-progress.md` (copied unchanged) still shows `[ ] 6.2 not triggered` in its PR-B section. The persisted tasks artifact (`tasks.md`) is the source of truth and is checked.

## Specs Synced

| Domain | Action | Details |
|---|---|---|
| sample-source | Updated `openspec/specs/sample-source/spec.md` | MODIFIED R-SS-15: replaced in full with the delta text (R4-001 stale-rate recovery rule; 8 new Catch2 scenarios, including the failure-status-preserved scenario; the six original Manual scenarios kept). ADDED R-SS-16 (host-free load planner: 9 Catch2 scenarios and 1 Manual parity scenario). Terms paragraph not copied (delta-only). Out of scope: loader-thread allocation note appended. |
| sample-resampling | Updated `openspec/specs/sample-resampling/spec.md` | MODIFIED R-RS-9: integer 96k to 48k at 3 s; non-integer 96k to 44.1k at 6 s (Release); 384k to 44.1k informational. Recorded measured times added as a paragraph. ADDED R-RS-10 (out-of-memory result: 4 scenarios, hook-based sweep). Out of scope section added (the main spec had none). |
| build-harness | Updated `openspec/specs/build-harness/spec.md` | ADDED R-BH-6 (fail-injection test executable: 5 scenarios). |

Requirement counts: no REMOVED or RENAMED sections in any delta.

Merge method: MODIFIED blocks replaced by content (the "(Previously ...)" lines are not copied into the main specs). ADDED blocks appended after the last existing requirement. Out-of-scope lists edited in place or added where absent. Requirement IDs and scenario formatting follow each main spec.

Stale-text check: the `sample-resampling` delta R-RS-10 scenario is the final hook-based version (Given: a small input and the `armFailure(n)` hook, n = 1, 2, ...). The string "1e12" appears only in the Note that records the dropped huge-ratio test. Not stale, merged as written.

The archived copies under `specs/` keep the delta form, unchanged, as the audit trail.

## Archive Contents

- `proposal.md`, `explore.md`, `design.md`, `apply-progress.md`, `tasks.md`, `verify-report.md`, `verify-report-prb.md`: copied from `openspec/changes/loader-followups/`.
- `specs/sample-source/spec.md`, `specs/sample-resampling/spec.md`, `specs/build-harness/spec.md`: copied (delta form).
- `archive-report.md`: this file.

Verified by Glob: 10 of 10 artifact files present in the archive folder.

## Commits

| Commit | Branch | Scope |
|---|---|---|
| 30a9cf0 `feat(core,plugin): extract LoadPlanner and reissue stale-rate Reload` | feat/load-planner (PR-A) | LoadPlanner, R4-001, SampleLoader adapter |
| 2b9c96e `test(core): add alloc-failure sweeps, OOM coverage and decimation perf cases` | feat/loader-followups-tests (PR-B) | perf cases, OOM sweeps, armFailure, alloc-failure executable |

The archive executor did not commit or switch branches. The main spec edits and the archive folder are uncommitted; the orchestrator commits.

## Verification Evidence (recorded at verify; not re-run during archive)

| Command or check | Result (from verify reports) |
|---|---|
| ctest Debug | 220/220 (PR-B) |
| ctest Release | 224/224 (PR-B) |
| `[loadplan]` | 25 cases after PR-A remediation |
| Perf, Release, 96k to 44.1k | 5.42 s (verify pass 1), 5.76 s (verify pass 2); budget 6 s |
| Perf, Release, 96k to 48k | 2.70 s, 2.82 s; budget 3 s |
| Perf, Release, 384k to 44.1k | 20.09 s, 20.9 s (informational) |
| MSVC /W4 /WX | 0 warnings (Debug and Release) |

Archive: no shell, tests not re-run; evidence cited, not re-executed.

## Engram Observation IDs (traceability)

| Artifact | Topic key | Observation ID |
|---|---|---|
| explore | `sdd/loader-followups/explore` | 693 |
| proposal | `sdd/loader-followups/proposal` | 694 |
| spec | `sdd/loader-followups/spec` | 695 (summary; the files are the source of truth in hybrid mode) |
| design | `sdd/loader-followups/design` | 696 (summary; file is the source of truth) |
| tasks | `sdd/loader-followups/tasks` | 697 (summary; `tasks.md` is the source of truth and is checked) |
| apply-progress | `sdd/loader-followups/apply-progress` | not searched; file copied |
| verify-report (PR-A) | `sdd/loader-followups/verify-report` | 699 |
| verify-report-prb (PR-B) | `sdd/loader-followups/verify-report-prb` | 700 |
| review receipts | `sdd/loader-followups/review/*` | not retrieved; orchestrator-supplied gate evidence |
| earlier blocked archive attempt | `sdd/loader-followups/archive-report` | 702 (superseded by this report, same topic key, upsert) |
| archive-report | `sdd/loader-followups/archive-report` | this report |

## Outstanding Items

1. Manual host checks D.1-D.6 pending (user).
2. Linux ASan/UBSan job not yet run with the replaced global `operator new` (`tests/support/AllocationCounter.cpp`). Confirm on the first CI run.
3. clang/gcc `-O2` self-test elision fix (`volatile` sink in `tests/test_alloc_failure.cpp`) reviewed but never run; no clang/gcc toolchain on this machine.
4. Windows CI is Debug-only, where the four alloc sweeps (resampler, builder, planner retarget, planner settle) are compiled out (`GRAINS_ALLOC_SWEEPS`). They run only on the Linux ASan job and local Release runs. Verify W1 (PR-B) recommendation to add a Windows Release `allocfail` run remains open.
5. R-RS-9 non-integer perf margin is about 4-10% against 6 s (5.42-5.76 s measured). The case is hidden `[.][perf]` and outside the CI gate.
6. PR-A verify W2: concurrency invariants (settled <= latest, pending not older than latest) are not asserted; no sanitizer run. PR-A verify W3: `settled_` is assigned unconditionally (benign with a single loader thread). PR-A verify W1 (no Published-outcome no-reissue test) was addressed in PR-A remediation.
7. Stale comment, not edited (source untouched): `Source/core/include/grains/LoadPlanner.h` line 24 reads `// Reload only: the status line to repost if the reload fails`. Since the review remediation, a reissued Reload carries its label and the loader posts `r.label` on Reload success too (per `apply-progress.md`, and the `settle` comment at lines 60-66 of the same header, which says the Reload "keeps showing the failure"). The struct comment is therefore incomplete. Checked from the header text and `apply-progress.md`; `SampleLoader.cpp` was not re-read by this executor.

## Risks

1. **Original change folder not removed (blocks the Step 3 and Step 4 checks).** This executor has no shell or delete tool. `openspec/changes/loader-followups/` still exists with all 10 files. The user or orchestrator must delete it after confirming the copies. Before deleting, compare each file pair (for example `git diff --no-index` or `fc /b`); the copies were written with the Write tool, so line endings may differ (CRLF vs LF) even though the content was transcribed from reads.
2. **Copies written with the Write tool.** Content intended to be identical; byte-equality not verified (no shell).
3. **Review gate not re-run live.** The `pre-commit` validate result and lineages are orchestrator-supplied.
4. **Stale checkbox in the archived `apply-progress.md`** (`[ ] 6.2`, PR-B section). `tasks.md` is checked; archived copy kept unchanged as the audit trail.
5. **Engram summaries.** Observations 694-697 are summaries; the archived files are the authoritative copies. Tasks observation 697 was not fetched in full.
6. **Source untouched.** No source, test, CMake, or `.github` file was changed. Only files under `openspec/` and the Engram archive report were written.

## Follow-ups (not implemented)

| ID | Item |
|---|---|
| 1 | Run manual host checks D.1-D.6 and record results. |
| 2 | Run the Linux ASan/UBSan job with the replaced `operator new`. |
| 3 | Run the clang/gcc `-O2` elision check for the `armFailure` self-tests. |
| 4 | Add a Windows Release `allocfail` run, or document Linux as the sole sweep gate (PR-B verify W1). |
| 5 | Add a sanitizer-run concurrency invariant check for `LoadPlanner` (PR-A verify W2). |
| 6 | Fix the `LoadRequest::label` comment in `LoadPlanner.h` (source change; separate slice). |
