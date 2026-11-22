# Archive Report: live-grain-display

**Change**: live-grain-display (roadmap #3)
**Project**: grains
**Branch**: feat/live-grain-view (base fd0adb5)
**Archive folder**: `openspec/changes/archive/2026-11-21-live-grain-display/` (moved here by the orchestrator after this report was written; all paths below are relative to the repository root unless stated otherwise)
**Archive status**: intentional-with-warnings. Phase 10 manual checks are carried as pending by user decision.

## Summary

The engine publishes a fixed-size snapshot of its active grains once per block through a generic, wait-free triple buffer. The plugin polls that mailbox at 30 Hz, only while the waveform view is showing, and draws one lane dot per active grain in a 48 px strip under the 140 px waveform. The core is JUCE-free and additive. The render path is unchanged: automated tests show bit-identical output with and without snapshots. Automated verification passes; manual checks are pending.

## Slices and commits

Delivery is three stacked slices plus one docs commit, not the two PRs planned in tasks.md. The planned PR 1 ran about 882 lines against the 800-line budget, so by the documented fallback Phase 5 (GrainDisplayModel) shipped as its own slice.

| Slice | Commit | Branch | Content | Gate evidence (orchestrator-supplied) |
|-------|--------|--------|---------|----------------------------------------|
| Core snapshot + TripleBuffer | bc248ff | feat/live-grain-core | TripleBuffer, GrainSnapshot, snapshotGrains, core tests | pre-commit allow, lineage review-42074d5f7f442691 (4-lens) |
| GrainDisplayModel | 3cd047d | feat/live-grain-model | JUCE-free display model and tests | pre-commit allow, lineage review-12a50020782f74cc |
| Plugin glue | 85ab354 | feat/live-grain-view | processor mailbox, processBlock restructure, WaveformView, editor | post-apply allow, lineage review-62de03322ebca0d6 |
| OpenSpec docs | 03b622e | feat/live-grain-view | change artifacts | no gate status supplied |

The documentation fixes and spec merges written by this archive phase are working-tree changes. This phase did not commit them.

## Gate status

Supplied by the orchestrator:
- The final slice (85ab354, lineage review-62de03322ebca0d6) validates `post-apply` with result `allow`, allowed `true`, reason "authoritative transaction, current repository target, and content-bound artifacts match".
- The earlier slices were each approved at their own pre-commit gate (bc248ff and 3cd047d, see the table above).

Limits:
- This phase did not read the native review files directly. The native store lives under `.git`, which this phase cannot access.
- `mem_search` for `sdd/live-grain-display/review` returned no observations. The engram topics `sdd/live-grain-display/review/{transaction,ledger,receipt,gate-context}` required by the archive skill were therefore not read. The gate rests on the orchestrator's report.
- No reviewer or lens was launched by this phase.
- Risk: this phase edited files under `openspec/` after the post-apply receipt for 85ab354 was bound. If the receipt covers the working tree, it may need native re-validation (`gentle-ai review validate --gate pre-commit`) before the docs are committed. Re-validation only; no new review budget.

## Verify verdict

PASS WITH WARNINGS (0 CRITICAL, 4 WARNING, 5 SUGGESTION), per `verify-report.md` (observation #725).

Automated evidence recorded by verify: core Debug ctest 271/271, core Release 276/276, realtime and alloc-failure executables pass in Release, TripleBuffer two-thread stress passed 30 consecutive repeats, plugin Standalone and VST3 built in Debug and Release with no C/C++ warnings. 21 automated scenarios COMPLIANT. 24 manual scenarios PENDING (Phase 10), with static evidence OK.

| ID | Finding | Status at archive |
|----|---------|-------------------|
| W1 | Phase 10 manual checks unrun | Carried as pending (user decision) |
| W2 | Core slice has no per-task TDD table; evidence is prose | Carried as pending |
| W3 | R-PS-4 said publish on "any early return"; design D5 publishes after render only | Fixed: change-folder spec reworded, merged into main |
| W4 | tasks.md still forecast two PRs; 6.2 unticked | Fixed: 6.2 ticked with split note; Phase 10 left open |
| S1 | R-WD-4 did not state the Waveform-state and poll-function conditions | Fixed: change-folder spec reworded, merged into main |
| S2 | proposal.md and explore.md kept superseded numbers | Fixed: "Superseded by design" pointer added to both |
| S3 | R-GR-11 allocation scenario covers saturated pool but not pitch or position extremes | Carried as pending |
| S4 | Stress test check `observed >= 1` is weak; `torn == 0`, `regressions == 0`, `last == kFrames` are the real guards | Carried as pending |
| S5 | Tick-based staleness (~267 ms) can stretch under a loaded message thread | Carried as pending (documented, accepted) |

## User decisions

- R-WD-4 amendment: a 30 Hz poll timer is allowed, but only while the view is showing.
- Live lane dots in a strip under the waveform (R-WD-7).
- Markers clear after about 267 ms (8 non-fresh ticks at 30 Hz).
- No markers in the no-waveform and drop-a-sample states.
- TSAN CI job is a separate micro-PR, out of scope here.
- Native UI is a follow-up, not part of this change.
- Future wish: real-time feedback of audio and interaction (meters, scope). Out of scope.

## Spec sync

Merged into the main specs under `openspec/specs/`. Requirement IDs are unchanged. The `[PR1]`/`[PR2]` delivery tags from the deltas were dropped from headings to match the main-spec format. No other requirements were touched.

| Domain | Action | Lines before to after | Delta |
|--------|--------|-----------------------|-------|
| waveform-display | R-WD-4 modified in place (scenarios reworded; "No timer and fixed size" replaced by "Timer only while showing and fixed size" plus "Idle tick does not repaint"). Purpose and Out of scope updated. R-WD-5, R-WD-6, R-WD-7 added. | 159 to 285 | +126 |
| plugin-shell | R-PS-4 modified (publish step, empty snapshot after prepare, W3 wording). R-PS-5 modified (lock-free publish, new snapshot scenario). R-PS-3 untouched. | 164 to 179 | +15 |
| grain-rendering | R-GR-11 added (4 scenarios) | 231 to 254 | +23 |

Main-spec total: +164 lines.

Change-folder edits (documentation fixes, step 1): `tasks.md` 134 to 135 (+1); `proposal.md` 63 to 65 (+2); `explore.md` 61 to 63 (+2); `specs/plugin-shell/spec.md` and `specs/waveform-display/spec.md` reworded within existing lines (0).

## Archive checklist

- [x] Main specs updated (waveform-display, plugin-shell, grain-rendering)
- [x] Change folder moved to the archive (orchestrator, after this report)
- [x] Archive contains proposal.md, explore.md, design.md, tasks.md, specs/, verify-report.md, archive-report.md
- [x] tasks.md: all implementation tasks checked except Phase 10. Phase 10 is intentionally unchecked and carried as pending (see below). This is an intentional archive with warnings, not stale checkboxes. No archive-time checkbox reconciliation was performed.
- [x] Active changes directory no longer contains this change (after the orchestrator's move)

## Carried as pending

Phase 10 manual checks (tasks.md 10.1 to 10.5), carried by user decision because the user is away. The same pattern was accepted for waveform-position-ui.
- 10.1 Audio A/B: bounce comparison, bit-for-bit identical or null result, in mono and stereo.
- 10.2 Lane dots: dots line up with the marker; geometry (48 px strip, 8 rows at 6 px pitch, 4 px mint dots, dimmer newer dots; band and marker span 188 px); no dots in Empty or NoWaveform; markers clear about 267 ms after transport stops; no idle flicker.
- 10.3 Timer lifecycle: minimise stops the timer and resets markers, and restore resumes it; editor close and reopen; source swap with the editor open.
- 10.4 Multi-instance CPU (4 or more instances); DPI move between displays (cache rebuild, crisp dots, window non-resizable); large-block flicker above about 233 ms (accepted).
- 10.5 Regression (R-PS-6): 9 generic controls, Load button, status label, waveform click and drag.

Verify findings carried:
- W2: core TDD evidence is prose, not a table.
- S3: R-GR-11 scenario wording ("extreme parameters") is broader than the test coverage.
- S4: stress test check `observed >= 1` is weak.
- S5: tick-based staleness stretches under a loaded message thread.

Additional known limitations:
- The DPI cache compares width only.
- The hint is centred in the 140 px waveform area.

Follow-up changes:
- TSAN stress micro-PR.
- Native UI layout replacing GenericAudioProcessorEditor.
- Roadmap #8 playback modes.
- Real-time audio and interaction feedback (future wish).
- Unresolved waveform-position-ui follow-ups: S4 right-click gesture, apply the two bool flags, jitterBand double sanitize.

## Traceability (engram observations)

- #720 `sdd/live-grain-display/proposal`
- #721 `sdd/live-grain-display/spec`
- #722 `sdd/live-grain-display/design`
- #723 `sdd/live-grain-display/tasks` (revision 3, the apply-progress snapshot before the 6.2 tick; `tasks.md` in the change folder is authoritative)
- #725 `sdd/live-grain-display/verify-report`
- #724 apply-progress (cited by the verify report; not fetched by this phase)
- Not found: `sdd/live-grain-display/review/{transaction,ledger,receipt,gate-context}`
- Referenced, not read: `decision/live-grain-display-product` (#719)
