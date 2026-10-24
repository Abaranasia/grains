# Archive Report: juce-fetch-license

**Archived**: 2026-10-24
**Archive path**: `openspec/changes/archive/2026-10-24-juce-fetch-license/` (folder move done by the orchestrator with git, before the commit; this report was written in the change folder and moves with it)
**Mode**: hybrid (openspec + engram `sdd/juce-fetch-license/*`)
**Branch**: feat/juce-fetch-license @ 03fe028 (single PR, 3 commits)
**Verdict carried from verify**: PASS (0 CRITICAL, 0 WARNING, 8 SUGGESTION)

## Commit Stack

| Slice | Commit | Subject | Review lineage |
|---|---|---|---|
| build | 931eae6 | build: add pinned JUCE FetchContent fallback and plugin CI job | review-6d051d7c5d07af76 (4R) |
| licensing | f8b48be | chore: license under AGPL-3.0-or-later | review-c5af3498af48b663 (4R) |
| records | 03fe028 | docs(juce-fetch-license): record commit and first CI run | review-a59ce71344c45d63 (review-reliability, risk medium) |

## Review Gate

```yaml
reviewGate:
  result: allow
  checkedBy: sdd-archive (native gate output supplied by the orchestrator; receipt read on disk)
  command: gentle-ai review validate --gate post-apply --lineage review-a59ce71344c45d63
  receipt: .git/gentle-ai/review-transactions/v2/review-a59ce71344c45d63/review-receipt.json
  receipt_check: schema gentle-ai.review-receipt/v2, lineage_id matches, terminal_state approved,
    generation 1, projection staged, base_tree f68deca, initial_review_tree and final_candidate_tree 2beb2c4,
    risk_level medium, selected_lenses review-reliability
  other_lineages: review-6d051d7c5d07af76 and review-c5af3498af48b663 are approved per the orchestrator's
    native output; their receipts were not re-read by archive
```

Engram topics `sdd/juce-fetch-license/review/*` are absent (searched, none found). The receipts on disk are the authority.

## Task Completion Gate

`openspec/changes/archive/2026-10-24-juce-fetch-license/tasks.md`: 0 unchecked (`- [ ]`). Checked: commit 1 tasks 1.1-1.17 (17), commit 2 tasks 2.1-2.11 (11), post-push task 3.1 (1). Total 29 of 29. No stale-checkbox reconciliation was needed. tasks.md was not edited by archive.

## Specs Synced

| Domain | Action | Details |
|---|---|---|
| licensing | Created `openspec/specs/licensing/spec.md` | ADDED R-LIC-1, R-LIC-2, R-LIC-3. 3 requirements, 6 scenarios (1 [CI], 5 [Manual]). R-LIC-2 comment wording now reads "`#` for CMake files". |
| build-harness | Updated `openspec/specs/build-harness/spec.md` | MODIFIED R-BH-3: 5 to 8 scenarios. Added "fetch failure or hash mismatch" and "fetch option defaults"; "JUCE missing" split into "fetch enabled" and "fetch disabled"; "JUCE present" renamed "local JUCE present"; "Projucer files and CI unchanged" renamed "Projucer files unchanged". MODIFIED R-BH-5: 5 to 8 scenarios. Added "plugin job builds through the fetch path", "plugin job is informational", "plugin job is cached and bounded"; "workflow hardening" gained SHA pinning. R-BH-1, R-BH-2, R-BH-4 and the Purpose section are unchanged (re-read after the merge). Result: 5 requirements, 21 scenarios (was 15). |

The "(Previously: ...)" and "(Unchanged ...)" markers from the deltas were dropped. No trailing non-requirement section existed in the build-harness main spec, so none was removed.

## Verification Evidence (recorded at verify, not re-run during archive)

| Command or check | Result |
|---|---|
| Fresh default build and ctest (MSVC Debug, plugin OFF) | 122/122 passed, no warnings |
| SHA256 of HEAD:LICENSE | 0d96a4ff...abcb0, matches the recorded digest |
| SPDX AGPL-3.0-or-later over the 56 first-party files | 56 files, each exactly once |
| Fetch OFF, empty GRAINS_JUCE_DIR, plugin ON | FATAL_ERROR naming the path and the three remedies |
| CI run 40119183345 on f8b48be | All three jobs succeeded. Plugin job: cache miss, configure 1m53s including the download, build 1m17s. Log unreadable (admin rights), so the fetch path is inferred from the cache miss and configure duration. |

## Engram Observation IDs (traceability)

| Artifact | Topic key | Observation ID |
|---|---|---|
| proposal | sdd/juce-fetch-license/proposal | 535 |
| spec | sdd/juce-fetch-license/spec | 536 |
| design | sdd/juce-fetch-license/design | 537 |
| tasks | sdd/juce-fetch-license/tasks | 538 |
| apply-progress | sdd/juce-fetch-license/apply-progress | 539 |
| verify-report | sdd/juce-fetch-license/verify-report | 544 |
| archive-report | sdd/juce-fetch-license/archive-report | this report (saved at archive) |
| review receipts | sdd/juce-fetch-license/review/* | not in engram; on disk, see Review Gate |

Filesystem artifacts (post-move paths under `openspec/changes/archive/2026-10-24-juce-fetch-license/`): `proposal.md`, `design.md`, `tasks.md`, `apply-progress.md`, `verify-report.md`, `specs/licensing/spec.md`, `specs/build-harness/spec.md`, and this `archive-report.md`.

## Resolved by This Change

- P5 (JUCE FetchContent fallback, LICENSE AGPLv3 and the related licensing change): CLOSED. Fallback pinned to JUCE 9.0.0 (f8f8864) with URL_HASH; LICENSE is the verbatim AGPLv3 text; SPDX lines and README notes added.
- P7 (plugin CI job): CLOSED. Informational Windows MSVC job builds `grains_plugin` through the fetch path, with cache and timeout.
- Verify suggestion 7 (R-LIC-2 "YAML-free scripts" wording): CLOSED in the merged spec.

## Known Limitations

1. Verify did not re-run the full fetch build (G1 ran at apply, and CI exercised it).
2. The CI log content was inferred from job results, not read.
3. A full local-JUCE build is not recorded (G4 configured only). See follow-up V6.

## Follow-ups (not implemented)

| ID | Item | Tracking |
|---|---|---|
| V1 | Single CMake variable for the JUCE SHA (duplicated in the URL, status message and CI cache key). | next change |
| V2 | README line 40 "so ... therefore" wording is redundant. | next change |
| V3 | design.md shows a placeholder hash where the real SHA256 is known (0bef95e4...93a9, recorded in apply-progress). The archived design is not edited. | next change |
| V4 | G2/G4 "_deps" wording is imprecise (SOURCE_SUBDIR download-only layout). | next change |
| V5 | Include the URL_HASH digest in the CI cache key. | next change |
| V6 | A full local-JUCE build is not recorded (G4 configured only). | next change |
| V8 | apply-progress.md has no task 2.8 entry. | next change |
| W-A (WARNING) | OOM restore label: the empty-restore clear path posts "Error: No sample" when `stageClear()` fails to allocate. R-SS-9 does not specify a label for this failure. | next change |
| S-A | G5 stress test could also assert commits > 0 to prove the stress is exercised. | next change |
| W2 | `stopThread(4000)` in `SampleLoader::stop()` can force-kill the thread on a slow read or decode. Fix: longer or unbounded wait, or cancellable reads. | next change |
| S1 | `frees()` test assertions could be elided by an optimizing compiler in Release. | next change |
| S2 | `maxSourceFrames` floor and INT_MAX clamp branches have no tests. | next change |
| S3 | The collect-after-publish rule is documented only in a class comment. | next change |
| S4 | Unused `<cmath>` include in `tests/test_sample_builder.cpp`. | next change |
| S5 | Add a rationale comment for `kMaxSourceSeconds` (5 minute cap). | next change |
| Privacy | Absolute file paths are stored in plugin state (R-SS-8). Review the privacy exposure. | next change |
| P1 | Soft limiter decision: output can reach the ±4 ceiling with the drone always on. | roadmap |
| P2 | Embedded audio and relative-path state (out of scope; only absolute paths are stored). | roadmap |
| P3 | Load-time resampling (out of scope; the builder does not resample). | roadmap |
| P4 | VST3-in-DAW manual checks (host automation, MIDI ignored, rates and block sizes), waived for sample-loader. | next change or manual session |
| P6 | Plugin identity codes and bundle ID (placeholders, company "Grains"). | before release |

Verify suggestions V1-V8 are the 8 SUGGESTIONS from verify-report (V7 is closed above). P5 and P7 are resolved by this change and are not carried forward.

## Risks

1. Archive did not re-run `gentle-ai review validate`, because this executor has no shell. The `reviewGate: allow` result is the orchestrator's native output. The 03fe028 receipt was read on disk and matches it.
2. build-harness scenario "Both jobs run on push to main and on PRs" still names two jobs. The delta marked it unchanged, so it was kept verbatim. The orchestrator should decide whether to rename it to three jobs in a follow-up.
3. One wording change in R-BH-5 is not verbatim from the delta. The sentence "Making CI a required check for merges to main is out of scope for this change" was replaced with "Branch protection that makes CI a required check for merges to `main` is not specified here." This removes change-relative wording. The plugin job's non-required status is still stated in the same requirement.
4. The CI job 3 result is inferred from job status and timing, because the log is unreadable (see Known Limitations).
5. The apply-progress.md header still reads "1.17 commit pending". tasks.md records 931eae6 for task 1.17, so tasks.md is treated as authoritative. No reconciliation was needed.
