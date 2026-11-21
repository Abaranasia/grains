# Archive Report: plugin-target

**Archived**: 2026-10-24
**Archive path**: `openspec/changes/archive/2026-10-24-plugin-target/` (folder move performed by the orchestrator with git; see Risks)
**Mode**: hybrid (openspec + engram `sdd/plugin-target/*`)
**Branch**: feat/plugin-target @ b572920 (stacked on feat/granular-synth-followups; not pushed)
**Verdict carried from verify**: PASS WITH WARNINGS (0 CRITICAL, 5 WARNING, 5 SUGGESTION)

## Commit Stack

| Slice | Commit | Subject |
|---|---|---|
| core | 47aa048 | feat(core): add JUCE-free plugin parameter spec layer |
| plugin | 98f1fbf | feat(plugin): add opt-in silent VST3 and Standalone plugin shell |
| fix | 18c0045 | fix(plugin): output silence when processBlock runs before prepareToPlay |
| docs | b572920 | docs(plugin-target): record manual verification results |

## Review Gate

```yaml
reviewGate:
  result: allow
  checkedAt: 2026-10-24
  checkedBy: sdd-archive (filesystem receipts + engram #509)
  lineages:
    - commit: 47aa048
      lineage: review-452541d4f2b92ce2
      terminal_state: approved
      risk_level: high
      lenses: [review-risk, review-resilience, review-readability, review-reliability]
      final_candidate_tree: 47ad062e2540d64c32a51e84ec0b08f96adac797
    - commit: 98f1fbf
      lineage: review-254ba89f95d94f47
      terminal_state: approved
      risk_level: medium
      lenses: [review-reliability]
      final_candidate_tree: 66c831964b90a40c83463ab99381dc3c58905569
    - commit: 18c0045
      lineage: review-02a742d01cac7e74
      terminal_state: approved
      risk_level: medium
      lenses: [review-reliability]
      final_candidate_tree: 97cd239f40a831736c3edab07e3b557c6bd706ba
    - commit: b572920
      lineage: review-8cd92aa35756b20e
      terminal_state: approved
      risk_level: medium
      lenses: [review-reliability]
      base_tree: 97cd239f40a831736c3edab07e3b557c6bd706ba
      final_candidate_tree: 08976c67c0a652c81a0cc737966b2753436c1a58
  evidence_read: "receipts at .git/gentle-ai/review-transactions/v2/<lineage>/review-receipt.json, all terminal_state approved; engram sdd/plugin-target/review/* topics absent (receipts on disk are the authority, as in granular-synth)"
```

Note: no structured status object was supplied by the orchestrator. The gate was checked against the receipts on disk. The receipt-to-tree mapping is taken from the receipt fields and was not re-derived with git in this run.

## Task Completion Gate

`openspec/changes/plugin-target/tasks.md`: no unchecked implementation tasks (`- [ ]` count 0). Tasks 1.1-1.7, 2.1-2.5 and 2.7 are `[x]`. Task 2.6 is `[x]` with the user's 2026-10-24 manual results and the waiver of the VST3-in-DAW checks. No stale-checkbox reconciliation was needed.

## Specs Synced

| Domain | Action | Details |
|---|---|---|
| build-harness | Updated `openspec/specs/build-harness/spec.md` | R-BH-3 replaced by the MODIFIED text from the delta (plugin targets behind `GRAINS_BUILD_PLUGIN`, default OFF; 5 scenarios). R-BH-1, R-BH-2, R-BH-4 unchanged. |
| parameter-mapping | Created `openspec/specs/parameter-mapping/spec.md` | 4 requirements (R-PM-1..4), 11 scenarios. New capability, delta copied with title normalised. |
| plugin-shell | Created `openspec/specs/plugin-shell/spec.md` | 7 requirements (R-PS-1..7), 14 scenarios. New capability, delta copied with title normalised. |

Delta copies under `specs/` in the archive folder are kept as history.

## Engram Observation IDs (traceability)

| Artifact | Topic key | Observation ID |
|---|---|---|
| proposal | sdd/plugin-target/proposal | 504 |
| spec | sdd/plugin-target/spec | 505 (revised twice) |
| design | sdd/plugin-target/design | 506 |
| tasks | sdd/plugin-target/tasks | 507 |
| apply-progress | sdd/plugin-target/apply-progress | 508 (summary; full file on disk) |
| state (commit and review record) | sdd/plugin-target/state | 509 |
| verify-report | sdd/plugin-target/verify-report | 512 |

The engram apply-progress observation (508) predates the 2.6 correction below. The filesystem `apply-progress.md` is authoritative.

## Corrections Applied at Archive

1. **R3-1 (apply-progress 2.6)**: the stale "partial / still pending" entry was replaced. It now records Standalone items 1-4 as confirmed by the user on 2026-10-24, and the VST3-in-DAW checks (host automation, MIDI ignored) as WAIVED and deferred as follow-up (a).
2. **R3-2 (follow-ups)**: the waived checks are listed explicitly below.

No code was touched. `tasks.md` was not edited (Windows-1252 encoding).

## Test Totals

Verify (MSVC 19.51, Debug): plugin OFF `ctest` 95/95 (84 baseline + 11 parameter-mapping cases). Plugin ON: `GrainsPlugin_VST3` and `GrainsPlugin_Standalone` built. Standalone manual checks confirmed 2026-10-24.

## Known Limitations (carried from verify)

1. VST3-in-host behaviour (automation, MIDI ignored, rates, block sizes) is not manually verified. Waived by user.
2. Multi-rate (44.1/48/96 kHz) and 30 s CPU-idle scenarios are not recorded in the manual evidence.
3. Skew test duplicates the NormalisableRange formula inline (intentional, no JUCE).
4. Plugin CI job is out of scope (CI stays OFF per R-BH-3).

## Follow-ups (not implemented)

| ID | Item | Tracking |
|---|---|---|
| a | VST3-in-DAW manual checks: host parameter automation (R-PS-3, R-PS-4 host part), MIDI ignored (R-PS-2), and multiple sample rates and block sizes (R-PS-4, 44.1/48/96 kHz) | next change or manual session |
| b | Confirm defaults before release: (1) silent shell (no sample loader) is the intended first shell; (2) JUCE is local `../../Projucer` only, no FetchContent; (3) placeholder metadata `Grns` / `Grn1` / company `Grains` / bundle `com.grains.grains`; (4) JUCE licensing, AGPLv3 vs commercial, is the user's distribution decision | before release |
| c | Alignment items: (1) traceability numbering differs between tasks.md and spec (tasks R-PS-5 = editor vs spec R-PS-6; tasks R-PM-2 = ranges vs spec R-PM-2 = conversion); (2) apply-progress RED-count wording is imprecise (earlier review note); (3) plugin CI job remains out of scope; (4) tasks.md is Windows-1252, normalise encoding in a separate change | next change |
| d | Pre-existing roadmap items: sample loader (with RT-safe publish/retire), Projucer file deletion (Grains.jucer, Builds/, JuceLibraryCode/), soft limiter decision | roadmap |
| e | MIDI note handling (later change, per R-PS-2) | roadmap |

## Risks

1. The folder move was not performed by sdd-archive (no move tool). The orchestrator must move `openspec/changes/plugin-target/` to `openspec/changes/archive/2026-10-24-plugin-target/`. The folder contains the untracked `verify-report.md`, so a plain folder move plus `git add` is needed rather than `git mv` alone.
2. `apply-progress.md` line 3 (status header) still reads "2.6 manual checks pending user". Only the 2.6 entry was corrected, as scoped. The status line should be fixed in a follow-up edit.
3. The review gate was verified from receipts on disk, not from a structured status object supplied by the orchestrator.
4. Revert path: `git revert` the archive commit restores the change folder and removes the main spec additions together.
