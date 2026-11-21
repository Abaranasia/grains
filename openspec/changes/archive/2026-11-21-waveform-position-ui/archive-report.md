# Archive Report: waveform-position-ui (roadmap #2)

**Date**: 2026-11-21
**Mode**: hybrid (openspec + engram)
**Archive status**: intentional-with-warnings (manual checks carried pending by user decision; review gate allow for PR 2, PR 1 scope-changed as expected for a stacked branch)
**Archived to**: `openspec/changes/archive/2026-11-21-waveform-position-ui/` (folder move performed by the orchestrator; this report travels with the folder)

## Summary

Roadmap #2 adds a waveform display with a position marker and a read-only jitter band to the Grains plugin. Click and drag set `position` through a `ParameterAttachment` with gestures, so host automation records the move. Pure logic (peaks, geometry, display state) lives in the JUCE-free `grains_core` and is Catch2-tested. The JUCE glue (`WaveformView`, loader and processor wiring, editor layout) is thin and verified manually.

## PR Split

- **PR 1** (base: `feat/waveform-core`): docs commit `67be0ea` (openspec change artifacts) + core commit `4117411` (waveform peaks, geometry, display state, tests, specs). Measured size about 1180 lines (docs 462, core 717), above the 800 line budget.
- **PR 2** (stacked on PR 1, branch `feat/waveform-plugin-view`): plugin commit `2a16087` (loader, processor, `WaveformView`, editor). Stacked, so its diff is against PR 1.

## Verify Verdict

PASS WITH WARNINGS: 0 CRITICAL, 4 WARNING, 5 SUGGESTION. Full report: `verify-report.md` in this folder (engram `sdd/waveform-position-ui/verify-report`, observation #712).

- Automated: R-WD-1 (6 scenarios plus the mono-identity requirement text) and R-WD-2 (5 scenarios) pass. Core build clean with `/W4 /WX`; `grains_tests` 246/246 Debug, 251/251 Release; alloc-failure suite 7 cases pass, including the peaks sweep; plugin Standalone and VST3 build in Debug and Release.
- Manual (carried pending): R-WD-3 (8), R-WD-4 (8), R-PS-6 (3). Static reading supports all of them.
- Warnings: W1 apply-progress has no TDD Cycle Evidence table; W2 PR 2 has no automated tests; W3 PR 1 size above budget; W4 the shared multi-config build-plugin directory gives a non-authoritative Debug count of 251 (the authoritative Debug count is 246).

## Review Gate Status

Evidence supplied by the orchestrator from the native review store (repository `.git` common directory). Not in engram topics.

- **PR 2**, lineage `review-fc234975ccc3c935`, gate `post-apply`: result `allow`, allowed `true`. Reason: "authoritative transaction, current repository target, and content-bound artifacts match". Approved receipt at HEAD (commit `2a16087`).
- **PR 1**, lineage `review-b4f61d44c180bb74` (core commit `4117411`, 4-lens review, approved, pre-commit gate allowed at commit time): post-apply validation reports `scope-changed`. This is expected, not new unreviewed code. PR 2 is stacked on PR 1, so the current HEAD tree (PR 1 + PR 2) no longer equals PR 1's reviewed scope. PR 1 was validated at its own commit time.

Limits of this evidence: the archive agent did not read the native transaction, ledger, receipt, or gate-context files directly. The `sdd/waveform-position-ui/review/{transaction,ledger,receipt,gate-context}` engram topics do not exist. The archive proceeds on the orchestrator's structured status. The PR 1 `scope-changed` state is recorded here as a known, accepted stacking condition, not as a clean allow.

## User Decisions (confirmed during proposal)

- Live active-grain display is deferred (follow-up with a `GrainSnapshot` and an engine hook; core stays mode-agnostic).
- Drag sets only `position`. The jitter band is read-only.
- Mixed-mono display.
- Full sample, no zoom or scroll.
- Generic sliders stay below the view; fixed window size.
- Native JUCE interim UI (WebView is roadmap #5).
- Click jumps `position` to the click, then drag continues.
- Peaks failure still loads and plays, with a "no waveform" state.
- Empty view shows a "drop a sample" hint.
- Repaint on change only, no timer.

## Spec Sync

Done in `openspec/specs/` (hybrid mode):

- **NEW** `openspec/specs/waveform-display/spec.md`: 159 lines. Requirements R-WD-1 (peaks contract), R-WD-2 (geometry mapping), R-WD-3 (delivery and lifecycle), R-WD-4 (waveform view), with scenarios and the out-of-scope list. Converted from the delta to main-spec form (`## Requirements`, no `ADDED` header), IDs and scenarios unchanged.
- **MODIFIED** `openspec/specs/plugin-shell/spec.md`, R-PS-6 Editor: replaced with the delta text (waveform view above the generic editor, fixed window size, exactly 9 parameters, three scenarios). Net +10 lines. Out-of-scope list: "waveform display" removed, and "custom parameter UI" changed to "custom parameter UI (beyond the waveform/position view)". No other requirement touched (R-PS-1 to R-PS-5, R-PS-7, R-PS-8 unchanged).

## Task Completion

`tasks.md`: phases 1 to 5 are complete (tasks 1.1 to 5.4 checked). Phase 6 manual checks 6.1, 6.2, and 6.3 remain unchecked. These are genuinely pending manual checks, not stale checkboxes. The user explicitly chose to carry them as pending, so the archive is intentional-with-warnings. No checkboxes were changed by archive. No stale-checkbox reconciliation was performed.

## Carried as Pending

### Manual checks (tasks 6.1 to 6.3, not yet run)

The user must run these in the Standalone and in a DAW with the VST3:

- **6.1 R-WD-3**: load, stale load, failed later load, missing-file restore, rate change, late-opened editor, bit-identical audio. Peaks failure is not reproducible without fault injection; it is covered only by the core alloc sweep (`test_alloc_failure`).
- **6.2 R-WD-4**: click jump, drag clamp, automation gesture, jitter band, marker follows automation, empty hint, fixed size with no `Timer`, editor open during load.
- **6.3 R-PS-6**: the 9 generic controls, Load button, and status label still work.

Most important: in the DAW, check that one gesture pair per drag appears in the `position` automation lane. Also check that a failed later load keeps the current waveform.

### Follow-ups from verify

- **S4**: `mouseDown` starts a gesture for any mouse button, so right click also jumps position. A gesture is never ended if the editor is destroyed mid-drag.
- **S2**: `WaveformState::apply` takes two bool flags. Change to an enum.
- **S3**: `jitterBand` uses a double `finiteOr0`.
- **S5**: add an editor null check on `parameter("position")`.
- **S1**: `proposal.md` still says "generation-gated". The commit order is authoritative per R-WD-3. Update the proposal wording and close the design open question at archive.
- **W1**: add a TDD Cycle Evidence table to apply-progress.
- **W3**: combined PRs are about 1180 lines including docs (core commit alone is 718).

### Upcoming changes (roadmap)

- Live active-grain display (`GrainSnapshot` and engine hook).
- Roadmap #5: WebView UI exploration.
- Roadmap #8: playback modes.

## Observation IDs (traceability, engram project `grains`)

| Artifact | Engram topic | Observation ID |
|---|---|---|
| Proposal | `sdd/waveform-position-ui/proposal` | #706 |
| Product decisions | `decision/waveform-position-ui-product` | #707 |
| Spec (delta) | `sdd/waveform-position-ui/spec` | #708 |
| Design | `sdd/waveform-position-ui/design` | #709 |
| Tasks | `sdd/waveform-position-ui/tasks` | #710 |
| Verify report | `sdd/waveform-position-ui/verify-report` | #712 |
| Prior blocked archive attempt | `sdd/waveform-position-ui/archive-report` | #713 (updated by this report) |

Files in this folder: `proposal.md`, `design.md`, `tasks.md`, `verify-report.md`, `explore.md`, `specs/waveform-display/spec.md`, `specs/plugin-shell/spec.md`.

## Archive Verification

- [x] Main specs updated: `openspec/specs/waveform-display/spec.md` (new), `openspec/specs/plugin-shell/spec.md` (R-PS-6 modified).
- [x] Change folder moved to `openspec/changes/archive/2026-11-21-waveform-position-ui/`.
- [x] Archive folder contents: proposal, design, tasks, specs/, verify-report, explore, and this report.
- [x] No unchecked implementation tasks other than carried manual checks 6.1 to 6.3 (user decision, recorded above).
- [x] No CRITICAL issues in the verify report.
