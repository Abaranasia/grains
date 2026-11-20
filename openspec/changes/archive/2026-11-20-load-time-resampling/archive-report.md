# Archive Report: load-time-resampling

**Archived**: 2026-11-20
**Archive path**: `openspec/changes/archive/2026-11-20-load-time-resampling/` (files copied by the archive executor; see Risks 1 and 2 for the original folder)
**Mode**: hybrid (openspec + Engram `sdd/load-time-resampling/*`, project `grains`)
**Delivery**: single-pr with user-approved `size:exception` (about 960 changed lines, no chain split)
**Verdict carried from verify**: PASS WITH WARNINGS (0 CRITICAL, 5 WARNING, 3 SUGGESTION; Engram #691)
**Archive status**: intentional archive with warnings (user-directed override; see Overrides and Outstanding Items)

## Overrides Recorded (user-authorized)

1. **Native dispatcher block overridden.** `gentle-ai sdd-continue` reported archive as blocked with "missing valid gentle-ai.verify-result/v1 envelope; bounded review transaction is missing". The user explicitly authorized archiving anyway, matching earlier archives under `openspec/changes/archive/` (markdown verify-report only). No verify-result envelope was fabricated.
2. **Task 4.6 left outstanding.** Manual host checks D.1 to D.5 were not run. User decision: archive with these checks outstanding (see Outstanding Items).
3. **Verify warnings W2 to W5 carried, not resolved.** W1 is resolved in the main specs (see Specs Synced).

## Review Gate

The lineage `review-0514b93b64617e4c` was checked on disk. The native `gentle-ai review validate --gate post-apply` command was not run (no shell in this executor). Receipt and state read directly:

```yaml
reviewGate:
  result: allow   # per user statement and on-disk receipt; post-apply gate reported allow by user
  lineage: review-0514b93b64617e4c
  receipt: .git/gentle-ai/review-transactions/v2/review-0514b93b64617e4c/review-receipt.json
  receipt_check: schema gentle-ai.review-receipt/v2, lineage_id matches, generation 1,
    terminal_state approved, risk_level high, selected_lenses review-risk, review-resilience,
    review-readability, review-reliability, base_tree 78db0741, initial_review_tree and
    final_candidate_tree bcb902c0, paths_digest sha256:29dd2055..., policy_hash sha256:34fb63d7...,
    evidence_hash sha256:f4206da5..., resolved_finding_ids null
  review_state: .git/gentle-ai/review-transactions/v2/review-0514b93b64617e4c/review-state.json
    state approved, original_changed_lines 1842, correction_budget 200, identity matches receipt
  findings (both SUGGESTION, both outcome info, none blocking):
    R4-001 (resilience, Source/plugin/SampleLoader.cpp:134)
    R3-001 (reliability, Source/plugin/SampleLoader.cpp:264)
  limits: live Git tree not recomputed; no gentle-ai CLI run
```

Discrepancy: the dispatcher reported the bounded review transaction as missing, but the transaction directory exists on disk with a valid approved receipt. The dispatcher result should be re-checked by the orchestrator.

## Task Completion Gate

`openspec/changes/load-time-resampling/tasks.md` at archive time has **no unchecked `- [ ]` implementation tasks**. 20 of 20 implementation tasks are checked (1.1 to 5.2). Task 4.6 is not a checkbox: apply moved it to "Deferred follow-ups" (D.1 to D.5) by user decision. The gate therefore passes on the literal check. No stale-checkbox reconciliation was performed, and no checkbox was changed in the archived copy. Task 5.2 (R-RS-3 0.275 x src) is verified done: the scenario in the spec reads "0.275 x src".

## Specs Synced

| Domain | Action | Details |
|---|---|---|
| sample-resampling | Created `openspec/specs/sample-resampling/spec.md` | New domain, copied from the change spec. R-RS-1 to R-RS-9. Archive-time addition to R-RS-8: 1000:1 rejection sentence and scenario "Ratio beyond 1000:1 rejected" (W1). |
| sample-source | Updated `openspec/specs/sample-source/spec.md` | MODIFIED R-SS-5: replaces "The builder MUST NOT resample" with the target-rate and output-cap rules; adds the 1000:1 rule (W1); 4 scenarios added (resampled to target, equal rate bit-exact, output cap at target, invalid target). ADDED R-SS-15 (6 Manual scenarios). Out of scope: "load-time resampling" removed; "runtime (pitch-shift) aliasing fixes, keeping the original decode in RAM, user-adjustable resampler quality" added. |
| grain-rendering | Updated `openspec/specs/grain-rendering/spec.md` | MODIFIED R-GR-7: adds the `srcSr / sampleRate_` retention and equal-rate neutrality sentence; 2 scenarios added (mismatched source keeps pitch, matched source rate neutral). |
| plugin-shell | Updated `openspec/specs/plugin-shell/spec.md` | ADDED R-PS-8 (5 Manual scenarios). Out of scope: "load-time resampling" removed; runtime aliasing fixes, `processBlock` interpolator/oversampling changes, and removal of source-rate compensation added. |

Merge method: MODIFIED blocks replaced by content (the "(Previously ...)" lines are not copied into the main specs). ADDED blocks inserted before `## Out of scope (explicit)`. Out-of-scope lists edited in place. No REMOVED or RENAMED sections exist.

The archived copies under `specs/` keep the change's delta form, unchanged, as the audit trail. The W1 addition lives only in the main spec.

## Archive Contents

- `proposal.md`, `explore.md`, `design.md`, `apply-progress.md`, `tasks.md`, `verify-report.md`: copied unchanged from `openspec/changes/load-time-resampling/`.
- `specs/sample-resampling/spec.md`, `specs/sample-source/spec.md`, `specs/grain-rendering/spec.md`, `specs/plugin-shell/spec.md`: copied unchanged (delta form).
- `archive-report.md`: this file.

## Verification Evidence (recorded at verify; not re-run during archive)

| Command or check | Result (from verify-report) |
|---|---|
| `ctest --test-dir build -C Debug --output-on-failure` | 193/193 pass |
| `ctest --test-dir build -C Release --output-on-failure` | 193/193 pass |
| `grains_tests.exe "resample: 300 s stereo 96k to 48k within 3 s"` (Release) | 2.47 s, pass (limit 3 s) |
| MSVC /W4 /WX, Release | 0 warnings |
| Archive: no shell, tests not re-run | evidence cited, not re-executed |

## Verify Warnings and Review Suggestions

Outstanding (carried forward):

| ID | Item | Status |
|---|---|---|
| W1 | 1000:1 ratio limit was unspecified | RESOLVED in main specs (sample-resampling R-RS-8 and sample-source R-SS-5). Implementation: `buildTable` rejects `min(1, dst/src) < 1/1000`, i.e. downsampling beyond 1000:1. Upsampling is not limited by this check. |
| W2 | Loader and processor logic (retarget race handling, `settled_`/`published_`, `restoreGeneration_`) has no automated tests | OUTSTANDING |
| W3 | R-GR-7 bit-identical check uses a constructed buffer, not a pre-change golden | OUTSTANDING (accepted proof weakness) |
| W4 | `restoreGeneration_` behaviour is not in spec or tests; minor residual race | OUTSTANDING |
| W5 | Perf margin thin (2.4 to 2.5 s of 3 s) | OUTSTANDING |
| S1 | `OutOfMemory` path untested (verify SUGGESTION, not in user list) | OUTSTANDING (recorded for completeness) |
| S2 | R-RS-3 "0.55 x src" typo | RESOLVED (spec reads 0.275 x src; design.md open question kept as history) |
| S3 | Host-free test seam for `SampleLoader::retarget` | OUTSTANDING (verify SUGGESTION, not in user list) |
| R4-001 | Stale-rate Reload not reissued after a failed superseding User load (`SampleLoader.cpp:134`). Quality-only; engine compensates. | OUTSTANDING (review SUGGESTION, outcome info) |
| R3-001 | No automated tests for retarget, `settled_`, `published_`, Reload (`SampleLoader.cpp:264`) | OUTSTANDING (review SUGGESTION, outcome info; overlaps W2) |
| (user list) | Non-integer decimation performance not timed | OUTSTANDING. Note: this item is not in the review receipt or verify-report. The nearest source is design.md Open Questions ("384k to 8k ... may take several seconds"). Recorded as the user stated it. |

## Outstanding Items: Manual Host Checks (task 4.6, user decision)

Requirements R-SS-15 and R-PS-8 have no automated coverage. These checks need a real plugin host.

- D.1 R-SS-15: 96 kHz file with host going 44.1 kHz to 48 kHz: exactly one reload, published rate becomes 48000 Hz.
- D.2 R-PS-8: restore before the first `prepareToPlay`: file plays at its own rate, then one reload; equal rates cause no reload.
- D.3 File deleted when a rate-change reload runs: old buffer keeps playing and the label does not read "Missing".
- D.4 Rapid rate changes: only the last result is published.
- D.5 A long resample superseded by a newer load aborts and publishes nothing.

Also listed in verify-report "Manual checks still pending": allocation-free `processBlock` during reload, and peak memory release.

## Engram Observation IDs (traceability)

| Artifact | Topic key | Observation ID |
|---|---|---|
| explore | sdd/load-time-resampling/explore | 684 (cited in explore.md; not re-fetched by archive) |
| proposal | sdd/load-time-resampling/proposal | 685 (retrieved and read in full) |
| spec | sdd/load-time-resampling/spec | 686 (retrieved and read in full) |
| design | sdd/load-time-resampling/design | 687 (retrieved and read in full) |
| tasks | sdd/load-time-resampling/tasks | 689 (retrieved; Engram copy is revision 2; file on disk is source of truth and matches the 20/21 state) |
| apply-progress | sdd/load-time-resampling/apply-progress | not searched; file `apply-progress.md` copied |
| verify-report | sdd/load-time-resampling/verify-report | 691 (retrieved and read in full; file copied) |
| review receipt | sdd/load-time-resampling/review/* | not in Engram; on disk (see Review Gate) |
| archive-report | sdd/load-time-resampling/archive-report | saved at archive (this report) |

## Resolved by This Change

- Sample files are resampled once on the loader thread to the host rate using a band-limited Kaiser-windowed sinc (stopband at least 100 dB, passband ripple at most 0.1 dB up to 0.9 x cutoff). Downsampling aliasing from the per-grain linear interpolator is removed at load time.
- Host rate changes trigger one reload through the generation gate; restore before prepare publishes at file rate and reloads on the first prepare.
- R-GR-7 and R-SS-5 wording is consistent with the implementation (srcSr compensation kept; output cap at target rate).

## Known Limitations

1. Pitch-shift (runtime) aliasing during playback is not addressed (documented non-goal).
2. Rate-change reload re-decodes from disk; a moved or deleted file keeps the current buffer (no reload).
3. Loader and processor logic is JUCE-bound and has no automated tests (W2, R3-001).
4. Perf budget margin is thin on the reference machine (W5).
5. Downsampling ratios beyond 1000:1 are rejected (`BadSampleRate`); the loader shows the error path for such pairs.

## Follow-ups (not implemented)

| ID | Item |
|---|---|
| 1 | Complete manual host checks D.1 to D.5 and record results. |
| 2 | Add a host-free seam for `SampleLoader::retarget` state logic (verify S3, review R3-001, W2). |
| 3 | Re-issue a stale-rate Reload after a failed superseding User load (R4-001). |
| 4 | Test or time non-integer decimation performance (user-listed item). |
| 5 | Add an allocation-failure injection test for the OutOfMemory path (verify S1). |
| 6 | Restore-generation behaviour: spec or test coverage (W4). |

## Risks

1. **Original change folder not removed.** This executor has no shell or delete tool. `openspec/changes/load-time-resampling/` still exists with the same files. Until it is removed, the active changes directory still contains this change, which fails the Step 4 check. The orchestrator or user must delete it after confirming the copies (same procedure as `sample-relative-path`).
2. **Copies written with the Write tool.** Line endings in the copies may differ from the originals (CRLF vs LF). Content is intended to be identical.
3. **Review gate not run live.** `gentle-ai review validate` was not executed. The receipt and state were read from disk and match the lineage. The live Git tree was not recomputed.
4. **Dispatcher discrepancy.** The dispatcher said the bounded review transaction was missing; it exists on disk. Re-check the dispatcher output.
5. **Commit not made.** Per instructions, nothing was committed. The archive changes (new `openspec/specs/sample-resampling/`, edits to three main specs, archive folder) are uncommitted.
6. **Outstanding manual checks.** Behaviour of R-SS-15 and R-PS-8 is unverified in a host (D.1 to D.5).
7. **Source untouched.** No source code, tests, or CMake files were changed by this archive.
