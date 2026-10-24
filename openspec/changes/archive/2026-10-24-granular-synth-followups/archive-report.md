# Archive Report: granular-synth-followups

**Archived**: 2026-10-24
**Archive path**: `openspec/changes/archive/2026-10-24-granular-synth-followups/`
**Mode**: hybrid (openspec + engram `sdd/granular-synth-followups/*`)
**Branch**: feat/granular-synth-followups @ a72f639
**Verdict carried from verify**: PASS (0 CRITICAL, 0 WARNING, 2 SUGGESTION)

## Status

Archive complete. Delta specs merged into `openspec/specs/grain-rendering/spec.md` and `openspec/specs/build-harness/spec.md`. The change folder was moved from `openspec/changes/granular-synth-followups/` to `openspec/changes/archive/2026-10-24-granular-synth-followups/` (folder move and commit performed by the orchestrator with git). `grain-scheduling` is unchanged by this change.

## Review Gate

```yaml
reviewGate:
  result: allow
  checkedAt: 2026-10-24
  checkedBy: sdd-archive (filesystem receipts and review-state records)
  chain:
    - commit: 61e442b
      subject: "test(engine): close granular-synth follow-ups a-d and f"
      lineage: review-e17a776da2c1454f
      terminal_state: approved
      risk_level: high
      lenses: [review-risk, review-resilience, review-readability, review-reliability]
      candidate_tree: 9b8502bef96db8944879369e30bdffd96531c195
    - commit: c2dfc74
      subject: "ci: add GitHub Actions with MSVC and clang sanitizer jobs"
      lineage: review-6a2d9e3df6b62cc4
      terminal_state: approved
      risk_level: high
      lenses: [review-risk, review-resilience, review-readability, review-reliability]
      candidate_tree: 8fe4e72978841a5a53e5a55daa6404ffd998131e
    - commit: 5fd4bbb
      subject: "docs(followups): close T9 after green first CI run"
      lineage: review-d43000d9ff4ec6f4
      terminal_state: approved
      risk_level: medium
      lenses: [review-reliability]
      candidate_tree: 58a733b91ccfefec65d2d8a6f200008aaf18e7de
    - commit: a72f639
      subject: "docs(followups): add machine-readable task checklist"
      lineage: review-e61075376cd66481
      terminal_state: approved
      risk_level: medium
      lenses: [review-reliability]
      candidate_tree: f2f1c7cc28ace172fb8041a547da51f87092c9a57
      receipt_published: true
  head_ref: refs/heads/feat/granular-synth-followups -> a72f639 (.git/refs/heads/feat/granular-synth-followups)
  findings: "all info; no blocking findings; no correction transaction required"
  note: "Each lineage base_tree equals the previous lineage candidate_tree. The last candidate (f2f1c7c) is the tree of HEAD a72f639 per the review chain; the archive-time git status was clean for tracked files."
```

The archive-time `git status` showed only the untracked `verify-report.md` (committed alongside this archive) and the pre-existing untracked `build-plugin/`, which is unrelated and was not touched.

## Task Completion Gate

`openspec/changes/granular-synth-followups/tasks.md` (canonical in hybrid mode): 11/11 checked (T1-T11). The Checklist section and the task headings agree. T9 is closed `[x]` (commit 5fd4bbb, first CI run c2dfc74 green on both jobs, no clang `-Werror` fix and no float-cast-overflow report, flag set kept as designed). No stale checkboxes. No exceptional reconciliation was needed.

Note: the engram observation `sdd/granular-synth-followups/tasks` (#500) is a pre-T9 snapshot and still shows `- [ ] T9`. It is stale. The filesystem `tasks.md` is authoritative for this archive. See Risks.

## Commit Stack

| Slice | Commit | Subject |
|---|---|---|
| 1 | 61e442b | test(engine): close granular-synth follow-ups a-d and f |
| 2 | c2dfc74 | ci: add GitHub Actions with MSVC and clang sanitizer jobs |
| 3 | 5fd4bbb | docs(followups): close T9 after green first CI run |
| 4 | a72f639 | docs(followups): add machine-readable task checklist |

Source: `.git/logs/HEAD` (reflog), `.git/refs/heads/feat/granular-synth-followups`.

## Test Totals

- Debug, MSVC, fresh configure and build, `ctest -C Debug` (verify): **84/84 passed** (7.94 s).
- Release, MSVC (apply, after T7): **84/84 passed**. Not re-run at verify.
- Baseline before this change: 78/78. Delta: +6 test cases (T5 extends existing cases and adds `RunStats`; it does not add a case). 84 = 78 + 6.
- CI on c2dfc74 (reported by the orchestrator via GitHub API, recorded in verify-report): "Windows MSVC" success; "Linux clang ASan + UBSan" success (ubuntu-24.04, clang++-18, `-DGRAINS_SANITIZE=ON`, `ctest --output-on-failure`).

## Specs Synced

| Domain | Action | Requirements | Scenarios | Details |
|--------|--------|--------------|-----------|---------|
| grain-rendering | Updated (`openspec/specs/grain-rendering/spec.md`) | 9 (R-GR-1..9), unchanged count | 20 -> 27 (+7) | MODIFIED R-GR-1: +1 scenario (Window range over a dense phase sweep). MODIFIED R-GR-7: +4 scenarios (Extremes worst-case subset, Engine-level pitch clamp is bit-identical in both directions, Grain length scales with engine sample rate, Float-to-index casts are overflow-safe), grid changed to at least 0.25 s, text amended with grain length and sanitizer requirements; existing Extremes grid, Clamping equivalence and Sample-rate range kept. MODIFIED R-GR-8: +2 scenarios (Extremes exercise real grain activity, Density zero plays nothing), header text extended. R-GR-2..R-GR-6 and R-GR-9 preserved unchanged. |
| build-harness | Updated (`openspec/specs/build-harness/spec.md`) | 4 -> 5 (+R-BH-5) | 6 -> 11 (+5) | ADDED R-BH-5 CI and sanitizers with 5 scenarios (Both jobs run on push to main and on PRs; Sanitizer option is opt-in; Sanitizer build is clean; Workflow hardening; Realtime exclusion fallback). R-BH-1..R-BH-4 preserved unchanged. |

Scenario counts were taken from the merged files by reading them back: grain-rendering scenario headings are 2+3+3+2+3+2+7+4+1 = 27 across R-GR-1..R-GR-9; build-harness is 2+2+1+1+5 = 11 across R-BH-1..R-BH-5.

Delta markers (`## MODIFIED Requirements`, `## ADDED Requirements`) were dropped. Requirement text and scenarios were otherwise copied from the delta files; the main-spec style (title, Purpose, `## Requirements`) was kept.

## Design and Tasks Amendments

No design amendment was needed. The float-cast-overflow flag was kept (T9 closed with no source change), so `design.md` and the R-BH-5 delta match the final flag set `-fsanitize=address,undefined,float-cast-overflow -fno-sanitize-recover=all`. `tasks.md` was not edited by archive.

## Apply-Progress Notes (carried from `apply-progress.md`)

- T2: the pitch clamp exists in two places (`setParams` and `spawn()`). Removing only the `setParams` clamp does not change output, because `spawn()` clamps again. The test turns RED only when both are removed. Defense in depth, not a gap; T3 covers the spawn side.
- T5: `maxActive > 0` is asserted per combination only for size 1000 ms at position 0. 1 ms grains and the 2-sample move-back grains are released inside a block, so audibility is covered by `peak > kSoundThreshold`.
- T5: a sabotage on the scheduler density guard hung the test (infinite period); it was killed and reverted, and a different density-0 sabotage was used.
- T10: `actions/checkout` is pinned to the full SHA `fbc6f3992d24b796d5a048ff273f7fcc4a7b6c09` (tag `v5.1.0`, lightweight, resolved via `git ls-remote`). `v6` exists upstream; v5 was kept as designed.
- No engine fix was needed. `Source/` is untouched by this change.

## Archive Contents

Engram observation IDs (hybrid traceability):

| Artifact | Topic key | Observation ID | Note |
|---|---|---|---|
| proposal | sdd/granular-synth-followups/proposal | 497 | current |
| spec | sdd/granular-synth-followups/spec | 498 | current (delta summary) |
| design | sdd/granular-synth-followups/design | 499 | current; rev 2 |
| tasks | sdd/granular-synth-followups/tasks | 500 | STALE: shows T9 `[ ]`; canonical is `tasks.md` (T9 `[x]`) |
| apply-progress | sdd/granular-synth-followups/apply-progress | 501 | STALE: shows 10/11; canonical is `apply-progress.md` (11/11) |
| verify-report | sdd/granular-synth-followups/verify-report | 515 | current |
| archive-report | sdd/granular-synth-followups/archive-report | (this report, saved with mem_save) | current |

Filesystem artifacts in `openspec/changes/archive/2026-10-24-granular-synth-followups/`:
- `proposal.md`
- `explore.md`
- `design.md`
- `tasks.md`
- `apply-progress.md`
- `verify-report.md`
- `archive-report.md`
- `specs/grain-rendering/spec.md` (delta, kept as history)
- `specs/build-harness/spec.md` (delta, kept as history)

Main specs updated: `openspec/specs/grain-rendering/spec.md`, `openspec/specs/build-harness/spec.md`.

## Verification of the Archive

- Main specs updated: yes (scenario counts above).
- Change folder moved: yes (performed by orchestrator).
- Archive contains proposal, specs, design, tasks, apply-progress, verify-report: yes.
- Archived `tasks.md` has no unchecked implementation tasks: yes (11/11).
- Active `openspec/changes/` no longer contains `granular-synth-followups`: yes.

## Follow-ups (not implemented)

| ID | Item | Source |
|---|---|---|
| 1 | Re-run the Release `ctest` (84/84 at T7 only; verify ran Debug only) on the archived tree | verify SUGGESTION 1 |
| 2 | Make the CI jobs ("Windows MSVC", "Linux clang ASan + UBSan") a required status check on `main` so CI gates merges. Out of scope for this change per R-BH-5 | verify SUGGESTION 2 |

Resolved in this change (no longer open): the R-GR-7 grain-length sample-rate scaling test (archive follow-up a of granular-synth), the engine-level pitch +100 == +24 test (follow-up b), the active-grain assertion in realtime extremes (follow-up c), the window [0,1] assertion (follow-up d), and the ASan/UBSan CI job (follow-up e). Follow-up f (0.25 s grid vs 1 s spec) was resolved by amending R-GR-7 to at least 0.25 s plus a 1 s worst-case subset.

## Risks

1. The engram observations #500 (tasks) and #501 (apply-progress) are pre-T9 snapshots. The filesystem files are canonical and correct. If another tool reads engram as the source of truth, it will see stale unchecked T9. Recommended: upsert `sdd/granular-synth-followups/tasks` and `apply-progress` from the filesystem.
2. Scenario count correction for the prior archive: `openspec/changes/archive/2026-10-18-granular-synth/archive-report.md` states grain-rendering has 22 scenarios. The merged spec before this change had 20 scenarios (R-GR-1 1, R-GR-2 3, R-GR-3 3, R-GR-4 2, R-GR-5 3, R-GR-6 2, R-GR-7 3, R-GR-8 2, R-GR-9 1). That archive is an audit trail and was not modified; this report records the corrected count.
3. The review gate was evaluated from on-disk receipts and review-state records, because no structured status object was included in the archive launch input. The chain is consistent (each base tree matches the previous candidate tree), but the orchestrator should confirm `reviewGate.result: allow` against its structured status before committing.
4. No Release/Tag or CI-as-required-check evidence exists yet. Neither blocks archive.
