# Archive Report: sample-loader

**Archived**: 2026-10-24
**Archive path**: `openspec/changes/archive/2026-10-24-sample-loader/` (folder move done by the orchestrator with git, before the commit)
**Mode**: hybrid (openspec + engram `sdd/sample-loader/*`)
**Branch**: feat/sample-loader @ cb493a0 (single PR, size:exception approved by the user)
**Verdict carried from verify**: PASS WITH WARNINGS (0 CRITICAL, 2 WARNING, 5 SUGGESTION)

## Commit Stack

| Slice | Commit | Subject |
|---|---|---|
| core | 3b50ffb | feat(core): add SourceExchange and sample builder |
| plugin | 63756b9 | feat(plugin): add sample loader, state restore and editor |
| docs | cb493a0 | docs: describe sample loader |

## Review Gate

```yaml
reviewGate:
  result: allow
  checkedBy: sdd-archive (native gate output supplied by the orchestrator, plus receipt on disk)
  command: gentle-ai review validate --gate post-apply --cwd H:/Proyectos/Juce/Plugins/Grains --lineage review-ae18007c15d78dfb
  allowed: true
  reason: "authoritative transaction, current repository target, and content-bound artifacts match"
  lineages:
    - commit: 3b50ffb
      lineage: review-49c0827ebea08706
      terminal_state: approved
      lenses: 4R (review-risk, review-resilience, review-readability, review-reliability)
    - commit: 63756b9
      lineage: review-c4af12b62d148a7c
      terminal_state: approved
    - commit: cb493a0
      lineage: review-ae18007c15d78dfb
      terminal_state: approved
      risk_level: medium
      lenses: [review-reliability]
  receipt: .git/gentle-ai/review-transactions/v2/review-ae18007c15d78dfb/review-receipt.json
  receipt_check: read directly by sdd-archive; schema gentle-ai.review-receipt/v2, terminal_state approved, lineage_id matches
  bound_via: bind-sdd
```

Receipts live under `.git/gentle-ai/`, not under `openspec/`. The engram `sdd/sample-loader/review/*` topics do not exist, which is expected because the receipts on disk are the authority. The earlier blocked attempt (engram observation 526, same topic key) is superseded by this report.

## Task Completion Gate

`openspec/changes/archive/2026-10-24-sample-loader/tasks.md`: no unchecked implementation tasks (`- [ ]` count 0). Tasks 1.1-1.13, 2.1-2.7, 2.8 (manual, 12 of 12 items confirmed by the user on 2026-10-24) and 3.1 are `[x]`. No stale-checkbox reconciliation was needed. tasks.md was not edited by archive.

## Specs Synced

| Domain | Action | Details |
|---|---|---|
| sample-source | Created `openspec/specs/sample-source/spec.md` | 10 requirements (R-SS-1..10), 24 scenarios (14 [Catch2], 10 [Manual]). New capability, delta copied with the "(NEW capability)" title marker removed. |
| plugin-shell | Updated `openspec/specs/plugin-shell/spec.md` | 4 MODIFIED sections replaced: Purpose, R-PS-4, R-PS-6, R-PS-7; plus Out of scope and Assumptions. R-PS-4 gained 2 scenarios (Sound after load, Source set only on change). R-PS-1, R-PS-2, R-PS-3, R-PS-5 unchanged. Result: 7 requirements, 15 scenarios (was 13). "(Previously: ...)" and "(Unchanged from previous.)" markers dropped. |

The main plugin-shell spec no longer claims the plugin is silent with a sample loaded. Silence is now scoped to "no sample loaded (or the last restore reported a missing file)", and the Purpose states that output is silent until a sample is loaded.

Delta copies under `specs/` in the archive folder are kept as history.

## Test Totals

Verify and apply-progress record `ctest` 111/111 with the plugin OFF build (baseline 95 plus 16 new sample-source cases). The plugin build `grains_plugin` (VST3 and Standalone) also succeeded. These totals are taken from the recorded verify run and were not re-run during archive.

## Engram Observation IDs (traceability)

| Artifact | Topic key | Observation ID |
|---|---|---|
| proposal | sdd/sample-loader/proposal | 518 |
| spec | sdd/sample-loader/spec | 519 |
| design | sdd/sample-loader/design | 520 |
| tasks | sdd/sample-loader/tasks | 521 |
| apply-progress | sdd/sample-loader/apply-progress | 522 |
| verify-report | sdd/sample-loader/verify-report | 525 |
| archive-report (blocked record, superseded) | sdd/sample-loader/archive-report | 526 |
| review receipts | not in engram | on disk, see Review Gate |

Filesystem artifacts: `proposal.md`, `explore.md`, `design.md`, `tasks.md`, `apply-progress.md`, `verify-report.md`, `specs/sample-source/spec.md`, `specs/plugin-shell/spec.md`, and this `archive-report.md`.

## Known Limitations (carried from verify)

1. W1: the loader publish race (see Follow-ups). The bounded impact was judged not CRITICAL.
2. W2: the 4 s stop timeout (see Follow-ups).
3. Multi-rate (44.1/48/96 kHz) and 30 s idle checks are covered by manual items 11 and 12, which the user confirmed.
4. VST3-in-host checks (automation, MIDI ignored, rates) were waived by the user and deferred to the plugin-target follow-up list.
5. The tasks.md forecast of about 730 lines is stale. The measured size is about 1010 code and test lines, which was accepted as size:exception.

## Follow-ups (not implemented)

| ID | Item | Tracking |
|---|---|---|
| W1 | SampleLoader publish race: `isCurrent()` is checked before `exchange_.publish(...)` in `SampleLoader::load`, so a request superseded in that window can still publish (narrow violation of R-SS-7 "earlier unfinished requests MUST NOT publish"). User-visible effect: if superseded load A publishes and the newer user load B then fails, A plays while the label shows "Error: B" and the stored path still names the file loaded before A, so a later restore loads a different file than the one heard. Fix: re-check the generation under the same lock as the publish, or make publish conditional on generation. | next change |
| W2 | `stopThread(4000)` in `SampleLoader::stop()` can force-kill the thread if a slow chunk read or decode blocks beyond 4 s (for example on a network drive), risking leaks or torn state at shutdown. Fix: longer or unbounded wait, or cancellable reads. | next change |
| S1 | `frees()` test assertions could be elided by an optimizing compiler in Release. Make the frees path observable (volatile or escape). | next change |
| S2 | `maxSourceFrames` floor and INT_MAX clamp branches have no tests. | next change |
| S3 | The collect-after-publish rule (`publish; collect;`) is documented only in a class comment. Consider a helper that enforces it. | next change |
| S4 | Unused `<cmath>` include in `tests/test_sample_builder.cpp`. | next change |
| S5 | Add a rationale comment for `kMaxSourceSeconds` (5 minute cap). | next change |
| P1 | Soft limiter decision: output can reach the ±4 ceiling with the drone always on. | roadmap |
| P2 | Embedded audio and relative-path state (both out of scope; only absolute paths are stored). | roadmap |
| P3 | Load-time resampling (out of scope; the builder does not resample). | roadmap |
| P4 | VST3-in-DAW manual checks (host automation, MIDI ignored, rates and block sizes), waived for this change. | next change or manual session |
| P5 | JUCE FetchContent fallback, plus adding a LICENSE file (AGPLv3) and the related licensing change. | before release |
| P6 | Plugin identity codes and bundle ID (placeholders `Grns`/`Grn1`, company "Grains"). | before release |
| P7 | Plugin CI job (out of scope; CI stays OFF per R-BH-3). | next change |

## Risks

1. The folder move was done by the orchestrator with git (`openspec/changes/sample-loader/` to `openspec/changes/archive/2026-10-24-sample-loader/`). The folder contains the untracked `verify-report.md`, so a plain folder move plus `git add` was needed.
2. R-PS-2 (Placeholder metadata) still says "in this change". It was left untouched because the delta marks it unchanged. Its wording is change-relative and may be reworded in a later spec edit.
3. R-SS legend says "verify phase". This is process wording, not change-relative, and was kept as delta text.
4. The review gate was verified from the orchestrator's native output and the receipt read on disk. The candidate tree hash was not copied into this report because it could not be verified independently.
5. The engram `sdd/sample-loader/apply-progress` topic was not searched. The filesystem `apply-progress.md` is the artifact of record.
