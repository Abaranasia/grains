# Archive Report: sample-relative-path

**Archived**: 2026-11-14
**Archive path**: `openspec/changes/archive/2026-11-14-sample-relative-path/` (copied by the archive executor; the orchestrator verified the copies, removed the original `openspec/changes/sample-relative-path/`, and committed the archive as 1b4e1e4, see Risks)
**Mode**: hybrid (openspec + engram `sdd/sample-relative-path/*`)
**Branch**: feat/sample-relative-path-plugin (chained PR2 on top of PR1 feat/sample-relative-path)
**Verdict carried from verify**: PASS WITH WARNINGS (0 CRITICAL, 5 WARNING, 3 SUGGESTION; Engram #637)
**Archive status**: intentional archive with warnings (maintainer-directed; the manual checks 4.1–4.3 were run after verify, with partial coverage; see Task Completion Gate and Verification Evidence)

## Commit Stack

| Slice | Commit | Subject | Review lineage |
|---|---|---|---|
| PR1 feat(core) | 960fec4 | feat(core): add SamplePathResolver for relocatable sample references (branch feat/sample-relative-path) | review-73ab176dc2e50d81 (4R, high) |
| PR2 feat(plugin) | 25532c3 | feat(plugin): restore samples through relocatable references (branch feat/sample-relative-path-plugin; includes cross-shape `\`/`:` hardening, task 3.0) | review-13297fab4f346d9b (reliability, medium) |

Archive commit (not part of 25532c3): 1b4e1e4 `docs(openspec): archive sample-relative-path change` on feat/sample-relative-path-plugin. It holds this archive folder `openspec/changes/archive/2026-11-14-sample-relative-path/` (including the final `tasks.md` 4.1–4.4 state and `verify-report.md`), the merged main specs `openspec/specs/sample-source/spec.md` and `openspec/specs/plugin-shell/spec.md`, and the removal of the original `openspec/changes/sample-relative-path/`.

No commit was made by the archive step itself; the orchestrator made commit 1b4e1e4.

## Review Gate

```yaml
reviewGate:
  result: allow
  checkedBy: sdd-archive (native gate output supplied by the orchestrator; receipts read on disk)
  code_review_pr1:
    lineage: review-73ab176dc2e50d81
    receipt: .git/gentle-ai/review-transactions/v2/review-73ab176dc2e50d81/review-receipt.json
    receipt_check: schema gentle-ai.review-receipt/v2, lineage_id matches, generation 1, terminal_state approved,
      risk_level high, selected_lenses review-risk, review-resilience, review-readability, review-reliability,
      base_tree 7746be1a, initial_review_tree and final_candidate_tree eafed748, resolved_finding_ids null
  code_review_pr2:
    lineage: review-13297fab4f346d9b
    receipt: .git/gentle-ai/review-transactions/v2/review-13297fab4f346d9b/review-receipt.json
    receipt_check: terminal_state approved, risk_level medium, selected_lenses review-reliability,
      base_tree eafed748, initial_review_tree and final_candidate_tree 7b636d36, resolved_finding_ids null
  docs_review:
    lineage: review-7016affc11b80da5 (pre-archive review of tasks.md, verify-report.md, bound to the change)
    receipt: .git/gentle-ai/review-transactions/v2/review-7016affc11b80da5/review-receipt.json
    receipt_check: terminal_state approved, risk_level medium, selected_lenses review-readability,
      base_tree 7b636d36, initial and final_candidate_tree 229a1a84, resolved_finding_ids null
    findings: 4 WARNING + 1 SUGGESTION (orchestrator-reported; not present in the receipt JSON).
      Addressed by the archived-copy edits: tasks 4.1–4.4 evidence notes and 4.4 tick; verify-report status note and addendum.
  limits: archive has no shell and did not run `gentle-ai review validate` or `gentle-ai sdd-status`;
    the live Git tree was not recomputed against the receipts. The receipts on disk match the orchestrator's native output.
```

Engram topics `sdd/sample-relative-path/review/*` were not found (searched `sdd/sample-relative-path/review`, no results). The receipts on disk are the authority, consistent with the soft-limiter and loader-hardening archives.

## Task Completion Gate

`openspec/changes/sample-relative-path/tasks.md` at archive start: 21 tasks checked (Phases 1–3); Phase 4 had 4.1–4.3 checked and 4.4 unchecked with the note "PENDING archive time".

Exceptional reconciliation, directed by the orchestrator (maintainer-directed archive):
1. **4.4 ticked** in the archived copy. Reason: 4.4 is the spec-sync step performed by this archive. It is not implementation work. The spec sync is complete (see Specs Synced).
2. **4.1, 4.2, 4.3 evidence notes rewritten** in the archived copy to state exactly what was and was not exercised. Each stays `[x]` for what was checked.
3. **Unchecked sub-items 4.1b, 4.2b, 4.3b** were added on purpose. They are residual manual checks that the maintainer has not exercised. They are NOT stale apply checkboxes, and the stale-checkbox reconciliation rule does not cover them. They are recorded as open follow-ups.

Apply-progress #632 and verify-report (#637 and the file) show the 21 implementation tasks (1.1–3.6, including 3.0) complete with evidence. Engram `sdd/sample-relative-path/tasks` (#631) predates the 4.x updates; the file on disk is the source of truth.

## Specs Synced

| Domain | Action | Details |
|---|---|---|
| sample-source | Updated `openspec/specs/sample-source/spec.md` | MODIFIED R-SS-8 (reference fields, candidate restore, legacy state, 4 scenarios). ADDED R-SS-12 (resolver `makeRef`/`candidates`/`resolve`, 21 Catch2 scenarios), R-SS-13 (legacy compatibility, 3 scenarios), R-SS-14 (candidate loading and relocated status, 7 scenarios). Out of scope (explicit) section replaced by the delta text; "relative-path lookup" removed. |
| plugin-shell | Updated `openspec/specs/plugin-shell/spec.md` | MODIFIED R-PS-7 (reference properties; old-format restore, moved folder, missing label; 5 scenarios). Out of scope (explicit): "relative-path lookup" removed. Assumptions: "absolute sample path stored in state" replaced by the sample reference text (the absolute path is still written). |

Merge method: requirement blocks replaced in full with the delta content (R-SS-8, R-PS-7). New requirements inserted after R-SS-11 (sample-source). Non-requirement `##` sections (Out of scope, Assumptions) merged by hand at the main spec's `##` level. Heading levels kept as in the main specs. No REMOVED or RENAMED sections exist. Line endings were preserved by in-place edits (Edit tool).

Post-merge check: no main spec still says "Absolute paths are used; relative-path lookup ... not provided" and no main spec lists "relative-path lookup" as out of scope. The phrase existed only in the two specs edited here. The other main specs (grain-rendering, grain-scheduling, parameter-mapping, licensing, build-harness) were not content-searched because no grep tool was available (see Risks).

Copies in the archive folder keep the delta form of the two spec files, as the audit trail requires.

## Verification Evidence (recorded at verify, plus post-verify manual checks 4.1–4.3; not re-run during archive)

| Command or check | Result |
|---|---|
| cmake --build build --config Debug | exit 0, all targets built (cf229eb5f349217) |
| ctest --test-dir build -C Debug --output-on-failure | 173/173 pass (c58839a8f747ad1) |
| ctest -R sample_path (focused) | 42/42 pass; grains_tests.exe [sample_path] 184 assertions in 42 test cases |
| cmake --build build/plugin-jf2 --config Debug --target GrainsPlugin_Standalone | Grains.exe linked, 0 warning lines (6a799256208ba4c) |
| Manual 4.1 (Standalone, maintainer) | anchored save and restore OK; 4.1b open |
| Manual 4.2 (Standalone, maintainer) | moved file shows "(relocated)"; deleted file shows "Missing: <name>"; 4.2b open |
| Manual 4.3 (Standalone Debug 25532c3, orchestrator, UI Automation) | stray fields with empty samplePath: "No sample"; relocated plus truncated: "Loaded: long.wav (relocated) (truncated to 5 min)"; saved samplePath unchanged after close; 4.3b open |
| Archive: no shell, could not run builds, ctest or gentle-ai | verify evidence is cited, not re-run |

Verify-report body is unchanged. Its verdict line still reads "PASS WITH WARNINGS (manual checks UNVERIFIED)"; the Addendum at the end of that file supersedes it for tasks 4.1–4.3.

## Engram Observation IDs (traceability)

| Artifact | Topic key | Observation ID |
|---|---|---|
| explore | sdd/sample-relative-path/explore | 622 (orchestrator-supplied; not re-fetched by archive) |
| decisions | not supplied by orchestrator | 623 (orchestrator-supplied) |
| proposal | sdd/sample-relative-path/proposal | 625 (orchestrator-supplied; not re-fetched) |
| spec | sdd/sample-relative-path/spec | 626 (orchestrator-supplied; not re-fetched) |
| design | sdd/sample-relative-path/design | 629 (orchestrator-supplied; not re-fetched) |
| tasks | sdd/sample-relative-path/tasks | 631 (predates the 4.x updates; file is source of truth) |
| apply-progress | sdd/sample-relative-path/apply-progress | 632 (orchestrator-supplied) |
| delivery / chain | sdd/sample-relative-path/delivery | 634 (orchestrator-supplied) |
| verify-report | sdd/sample-relative-path/verify-report | 637 (confirmed by search at archive) |
| standalone harness | not supplied by orchestrator | 639 (orchestrator-supplied) |
| sdd-init | sdd-init | 455 (orchestrator-supplied) |
| archive-report | sdd/sample-relative-path/archive-report | saved at archive (this report) |
| review receipts | sdd/sample-relative-path/review/* | not in Engram; on disk, see Review Gate |

Filesystem artifacts (under `openspec/changes/archive/2026-11-14-sample-relative-path/`): `explore.md`, `proposal.md`, `design.md`, `tasks.md`, `verify-report.md`, `specs/sample-source/spec.md`, `specs/plugin-shell/spec.md`, and this `archive-report.md`.

Merged main specs (not moved): `openspec/specs/sample-source/spec.md`, `openspec/specs/plugin-shell/spec.md`.

## Resolved by This Change

- The state stored only the absolute `samplePath`; a moved folder, another account, or another machine gave "Missing: <name>". Now the reference adds `sampleName`, and `sampleAnchor` + `sampleRel` when under Music, Documents, Desktop, or Home. Restore tries the anchor path, the stored absolute path, then the name in each anchor directory (non-recursive, max 10 candidates) (R-SS-8, R-SS-12, R-SS-14, R-PS-7).
- A relocated restore shows "Loaded: <name> (relocated)" and does not rewrite state (R-SS-14).
- Legacy `samplePath`-only state still restores; the name is derived from the path (R-SS-13).
- Cross-shape path handling (`\` vs `/`, drive and UNC case rules, `..` rejection, backslash in POSIX names) is covered by 42 resolver tests in `tests/test_sample_path_resolver.cpp`, plus the task 3.0 hardening.
- Roadmap item P2 (relocatable sample path) from the loader-hardening archive: closed for the anchor and name-search scope.

## Known Limitations

1. Name search is non-recursive and covers only files directly inside Music, Documents, Desktop, or Home (R-SS-12, R-SS-14). A file in a nested subfolder not under the stored anchor path is not found.
2. A same-name file in an anchor folder can load silently; it is flagged "(relocated)" (accepted in the proposal, assumption 1).
3. ASCII-only case folding on Windows (design open question; non-ASCII names with different case are not matched).
4. `samplePath` is still stored, so privacy is partial (follow-up).
5. macOS sandbox anchors are untested (development on Windows).
6. Plugin glue (tasks 3.1–3.5) has no automated RED/GREEN coverage; it rests on code inspection, the build, and the manual checks.
7. Manual coverage is partial: 4.1b, 4.2b, 4.3b remain open (see Follow-ups).
8. Fixed: the R-SS-14 deviation at SampleLoader.cpp:86 (non-empty samplePath with an empty candidate list showed "No sample"; verify WARNING 4) was fixed in the follow-up commit after 1b4e1e4. "No sample" is now shown only for an empty stored samplePath; otherwise the label is "Missing: <name>".
9. Fixed: spec wording gaps in `openspec/specs/sample-source/spec.md` (empty-list rule, POSIX-only backslash normalisation, stored-value hardening rules and scenarios, search directories as the four non-recursive anchor directories, lowercase anchor keys, explicit "No sample" vs "Missing" label rule) were addressed in the same follow-up commit after 1b4e1e4.

## Follow-ups (not implemented)

| ID | Item | Tracking |
|---|---|---|
| 1 | `SampleLoader.cpp:86`: "No sample" vs "Missing: name" for non-empty samplePath with an empty candidate list. Distinguish "samplePath empty" from "list empty". | fixed in the follow-up commit after 1b4e1e4 (with the matching spec wording fixes) |
| 2 | Resolver readability suggestions: dead UNC guard at `SamplePathResolver.cpp:35`; shared separator helper; test helper `refWith`; task-number banners in tests. | next change |
| 3 | Unexercised manual items 4.1b (outside-anchor two fields; old-format restore label), 4.2b (moved parent folder; stored-vs-derived name in Missing label; nested-subfolder non-recursion), 4.3b (superseded restore interactively). | manual session |
| 4 | Drop `samplePath` from state for privacy. | roadmap |
| 5 | Downloads anchor. | roadmap |
| 6 | Persisted last-used directory. | roadmap |
| 7 | Content hash or size check for same-name files. | roadmap |
| 8 | Embedded audio in state (P2 remainder). | roadmap |
| 9 | `gentle-ai sdd-status` remediation-state block reported upstream. | upstream |
| 10 | Install and authorize the gh CLI on the dev host so CI results can be observed. | dev host |
| 11 | Add a processor-free seam so the Restore-empty-list label can be unit-tested (verify SUGGESTION 1). | next change |

## Risks

1. The archive is an intentional archive with warnings. CRITICAL issues: none. Warnings carried: manual items 4.1b, 4.2b, 4.3b remain open; the SampleLoader.cpp:86 deviation; the TDD evidence and plugin-glue RED warnings from verify.
2. Resolved: the archive executor could not delete the original change folder (no shell or delete tool). The orchestrator then verified the copies (five files identical ignoring CR; `tasks.md` and `verify-report.md` differ only by the planned edits), removed the original `openspec/changes/sample-relative-path/`, and committed the archive as 1b4e1e4 on feat/sample-relative-path-plugin.
3. Archived file copies were written with the Write tool, not a byte copy. Line endings in the copies may differ from the originals (CRLF vs LF). Content is identical except the deliberate edits to `tasks.md` (4.1–4.4) and `verify-report.md` (status note and addendum). Main specs were edited in place with the Edit tool, which preserved their line endings.
4. Resolved: the original `tasks.md` (modified) and `verify-report.md` (untracked) were removed with the original folder; the archive, the merged main specs, and the deletion were committed together as 1b4e1e4, separate from 25532c3.
5. The unchecked sub-items 4.1b, 4.2b, 4.3b may trip a naive checkbox scan of the archived tasks. They are deliberate (see Task Completion Gate).
6. The archive did not run `gentle-ai review validate`, `gentle-ai sdd-status`, or git status (no shell). The `reviewGate: allow` result is the orchestrator's native output. The receipts on disk were read and match it. The live Git tree was not recomputed. Any scope check on the final commit must account for `openspec/specs/` and `openspec/changes/archive/`, which are outside the reviewed code path set.
7. The phrase check across main specs was done only for the two specs edited; the other five main specs were not content-searched (no grep tool).
8. Engram IDs other than #637 were supplied by the orchestrator and not re-fetched by the archive executor.
9. The commit subject for the unit-2 commit in the archived tasks (4.4) is the actual subject, `feat(plugin): restore samples through relocatable references`, not the planned subject in the original tasks.md text.
