# Archive Report: play-stop-buttons

**Status**: intentional-with-warnings
**Archived**: 2026-11-22
**Archived to**: `openspec/changes/archive/2026-11-22-play-stop-buttons/`
**Worktree**: `Grains-worktrees/play-stop-buttons` (branch `feat/play-stop-buttons`), code and artifacts committed as 680887a; archive edits left uncommitted.

## Review gate

- Native post-apply gate: `gentle-ai review validate --gate post-apply --lineage review-cec3604d47a51e0d` returned `allow` (reported by the orchestrator). 4R bounded review approved with no findings.
- No `sdd/play-stop-buttons/review/*` topics (transaction, ledger, receipt, gate-context) exist in engram; the gate evidence is the validate result above.

## Engram artifact traceability

| Artifact | Observation ID | Topic key |
|---|---|---|
| proposal | 737 | sdd/play-stop-buttons/proposal |
| spec | 738 | sdd/play-stop-buttons/spec |
| design | 739 | sdd/play-stop-buttons/design |
| tasks | 741 | sdd/play-stop-buttons/tasks |
| verify-report | 744 | sdd/play-stop-buttons/verify-report |

Filesystem copies: `proposal.md`, `design.md`, `tasks.md`, `verify-report.md`, `explore.md`, `specs/{transport-gate,plugin-shell,waveform-display}/spec.md` in this folder.

## Task completion

- Implementation tasks T0, T1.1-T1.3, T2.1-T2.4, T3.1-T3.2, T4.1, T5.1: all `[x]`.
- Manual checks M1-M7: all `[x]`, passed by user in the real plugin on 2026-11-22 (M1 buttons, M2 audio, M3 DAW `prepareToPlay` mid-session, M4 display dots, M5 state restore, M6 play-click race, M7 parameter sweep and 44.1/48/96 kHz).
- Docs D1-D4: `[x]`. README fix applied (old always-on statement removed).
- No unchecked tasks remain.

## Verify warnings and resolution

| ID | Warning | Resolution |
|---|---|---|
| W1 | Change size about 898 added lines, above the 800-line review budget (tests about 650) | Accepted by user as a size exception. Not split. |
| W2 | Processor and editor have no automated tests | Covered by manual checks M1-M7, all passed. |
| W3 | README.md described the old always-on behaviour | Fixed in the docs step. |
| W4 | Manual checks and docs pending | Done at archive: M1-M7 passed, D1-D4 complete. |
| S1 | design D8 wording differs from `onLoadResult` behaviour | Verify judged the code compliant (clears only on loaded or cleared). Design text not edited in the archive. |
| S2 | Mono chunked path (host block larger than declared max) can restart a latched Play in a later chunk of the same block | Carried. Edge case reachable only if a host exceeds its declared max block size. |
| S3 | Mutation-check notes for tests green on first run not recorded in apply-progress | Carried. Documentation gap only. |

## Spec sync

| Domain | Action | Details |
|---|---|---|
| transport-gate | Created | New main spec from the delta. R-TG-1 to R-TG-10 (10 requirements). `## ADDED Requirements` converted to `## Requirements`. |
| plugin-shell | Updated | Purpose (renders only while Playing). R-PS-4 replaced with the transport-gate text (Stopped after load, restore and prepare; hard stop on source change; block-quantised `playing` flag; publish while stopped). R-PS-6 replaced (Play/Stop radio toggles, 40 px top bar, 10 Hz sync, enablement). Scenarios added for R-PS-4 (9) and R-PS-6 (6). Out of scope extended. Assumptions reworded. R-PS-1, R-PS-2, R-PS-3, R-PS-5, R-PS-7, R-PS-8 unchanged. |
| waveform-display | Updated | R-WD-4 timer repaint rule (empty fresh frames while already empty do not repaint). R-WD-7 Stopped/empty-snapshot clause (one repaint on marks to empty). Scenarios added: Stopped publishing empty frames, empty fresh snapshot clears with one repaint, Dots clear on Stop, No dots while stopped after load, Dots resume on Play. R-WD-1, R-WD-2, R-WD-3, R-WD-5, R-WD-6 unchanged. |

### Deliberate deviation: omitted scenario

The delta's plugin-shell Assumptions section carried a change-time check scenario, "Spec wording consistent [Manual]", which searches the main spec for "drone", "always-on" and "renders continuously". It was omitted from the main spec: it is a verification step for this change, not a behaviour, and keeping those words in the main spec would leave them in the current text. Approved by the orchestrator.

### Stale wording removed

The phrases "drone", "always-on" and "renders continuously" were at `openspec/specs/plugin-shell/spec.md` lines 5, 46 and 179 before the sync. All three were replaced by the merged text. They no longer describe current behaviour in any main spec.

## Carried items

- S2 (mono chunked path latched Play edge case): carried, document or ignore in a follow-up.
- S3 (mutation-check notes): carried, documentation only.
- Open point "host `prepareToPlay` mid-session resets to Stopped": confirmed by M3 and recorded in the spec as accepted behaviour.
- Open point "Play-click race": confirmed by M6 and recorded as accepted behaviour.

## Outcome

The change is archived with the size exception (W1), the S2 and S3 suggestions carried, and the spec sync applied. No CRITICAL issues. No code, `Source/` or `tests/` files were modified during archive.
