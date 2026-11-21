# Archive Report: granular-synth

**Archived**: 2026-10-18
**Archive path**: `openspec/changes/archive/2026-10-18-granular-synth/`
**Mode**: hybrid (openspec + engram `sdd/granular-synth/*`)
**Branch**: feat/granular-synth-04b-realtime @ 1e58e1e
**Verdict carried from verify**: PASS WITH WARNINGS (0 CRITICAL, 5 WARNING, 4 SUGGESTION)

## Status

Archive complete. Design amended, main specs merged into `openspec/specs/{build-harness,grain-rendering,grain-scheduling}/spec.md`, archive report written, and the change folder moved from `openspec/changes/granular-synth/` to `openspec/changes/archive/2026-10-18-granular-synth/`. The folder move and the working-tree check (`git status`) were performed by the orchestrator.

## Review Gate

```yaml
reviewGate:
  result: allow
  checkedAt: 2026-10-18
  checkedBy: sdd-archive (filesystem receipts)
  lineages:
    - pr: PR1
      commit: 776c205
      branch: feat/granular-synth-01-harness
      lineage: review-d2ede75742906d55-r2
      generation: 2
      terminal_state: approved
      risk_level: high
      lenses: [review-risk, review-resilience, review-readability, review-reliability]
      final_candidate_tree: f9d05461e08b7a888a1d43eb1170bbd832d3663a
      findings: "4 info (1 SUGGESTION risk-1, 2 readability, 1 WARNING reliability-1 later fixed)"
      predecessor: review-d2ede75742906d55 (invalidated: finalize rejected duplicate lens finding IDs; maintainer rerun)
    - pr: PR2
      commit: 4a607b2
      branch: feat/granular-synth-02-pool-scheduler
      lineage: review-0d46f7ac56e12c30
      generation: 1
      terminal_state: approved
      risk_level: high
      lenses: [review-risk, review-resilience, review-readability, review-reliability]
      final_candidate_tree: 133dfbf290ff477b92670cd71db67d37d9fdc70c
      findings: "3 info (R4-001 WARNING, R2-001 WARNING, R2-002 SUGGESTION)"
    - pr: PR3a
      commit: f33d347
      branch: feat/granular-synth-03a-render
      lineage: review-39b5aacdd22ac058
      generation: 1
      terminal_state: approved
      risk_level: high
      lenses: [review-risk, review-resilience, review-readability, review-reliability]
      final_candidate_tree: ccb2aa03d61085e3da7bd797204729fc78c3226a
    - pr: PR3b
      commit: 4e47e19
      branch: feat/granular-synth-03b-engine
      lineage: review-06bb0d48f5b8f488
      generation: 1
      terminal_state: approved
      risk_level: high
      lenses: [review-risk, review-resilience, review-readability, review-reliability]
      final_candidate_tree: a02a745801070da896616c56bdd2405452520689
    - pr: PR4a
      commit: 365cde1
      branch: feat/granular-synth-04a-hardening
      lineage: review-4d240ff9790a6eaa
      generation: 1
      terminal_state: approved
      risk_level: high
      lenses: [review-risk, review-resilience, review-readability, review-reliability]
      final_candidate_tree: 390795877512c87f1c2eb49805c9e1ea12545849
    - pr: PR4b
      commit: 1e58e1e
      branch: feat/granular-synth-04b-realtime
      lineage: review-ec5d921ccbcac914
      generation: 1
      terminal_state: approved
      risk_level: medium
      lenses: [review-reliability]
      original_changed_lines: 335
      correction_budget: 168
      final_candidate_tree: 83147b0a2316a25810d982af33258370232b795d
      findings: none
    - pr: archive
      lineage: review-cf31ad5dd251139f
      scope: archive docs review (re-reviewed after doc fixes)
  evidence_read:
    receipts: all 6 (`.git/gentle-ai/review-transactions/v2/<lineage>/review-receipt.json`), all terminal_state approved
    review_state_read: review-d2ede75742906d55-r2, review-0d46f7ac56e12c30, review-ec5d921ccbcac914 (state approved)
    engram_topics: "sdd/granular-synth/review/{transaction,ledger,receipt,gate-context} do NOT exist in engram; receipts on disk are the authority"
```

Review budget note: every lineage is `approved` with no blocking findings. All findings are `info`. No correction transaction was required.

## Task Completion Gate

`openspec/changes/archive/2026-10-18-granular-synth/tasks.md`: 35/35 checked (PR1 11, PR2 9, PR3 9, PR4 6). No stale checkboxes. No exceptional reconciliation was needed.

## Commit Stack

| Slice | Commit | Subject |
|---|---|---|
| PR1 | 776c205 | build: add CMake and Catch2 harness with window, rng and param limits |
| PR2 | 4a607b2 | feat(core): add sample buffer, grain pool and grain scheduler |
| PR3a | f33d347 | feat(core): add linear interpolator, grain render and engine params |
| PR3b | 4e47e19 | feat(core): add granular engine with bounded grain spawning |
| PR4a | 365cde1 | test(engine): add determinism, extremes, loudness and pool-full suites |
| PR4b | 1e58e1e | test(engine): prove process() is allocation-free |

Source: `.git/logs/HEAD` (reflog). The working tree was checked with `git status` by the orchestrator after the folder move.

## Test Totals

Fresh build, Debug, MSVC 19.51, `ctest -C Debug`: **78/78 passed** (73 `grains_tests` + 5 `grains_realtime_tests`), exit 0. `GRAINS_BUILD_PLUGIN=ON` fails as required. Source: verify report, engram #491.

## Specs Synced

| Domain | Action | Details |
|---|---|---|
| build-harness | Created `openspec/specs/build-harness/spec.md` | 4 requirements (R-BH-1..4), 6 scenarios |
| grain-scheduling | Created `openspec/specs/grain-scheduling/spec.md` | 7 requirements (R-GS-1..7), 14 scenarios |
| grain-rendering | Created `openspec/specs/grain-rendering/spec.md` | 9 requirements (R-GR-1..9), 22 scenarios |

No main spec existed before, so each delta became the full spec. Delta files carry no ADDED/MODIFIED/REMOVED headers, so no requirement merge conflicts arose. The main specs were then normalized to current-state form (title, Purpose, `## Requirements`); requirement IDs, text and scenarios are unchanged. R-GS-7, R-GR-3, R-GR-5, R-GR-7 carry their amended text as recorded in the delta. The delta copies under `specs/` in the archive folder are kept as history.

## Design Amendments

`openspec/changes/archive/2026-10-18-granular-synth/design.md` gained an "Implementation amendments" section (A1-A15), inline "Superseded by" notes at the original-plan passages they replace, and a "Known limitations" subsection (8 items), covering the as-built decisions listed in the archive task.

## Archive Contents (for traceability)

Engram observation IDs:

| Artifact | Topic key | Observation ID |
|---|---|---|
| proposal | sdd/granular-synth/proposal | 461 |
| spec | sdd/granular-synth/spec | 464 |
| design | sdd/granular-synth/design | 465 |
| tasks | sdd/granular-synth/tasks | 468 |
| apply-progress | sdd/granular-synth/apply-progress | 475 |
| verify-report | sdd/granular-synth/verify-report | 491 |

Filesystem artifacts in `openspec/changes/archive/2026-10-18-granular-synth/`: `proposal.md`, `design.md` (amended), `tasks.md`, `specs/build-harness/spec.md`, `specs/grain-scheduling/spec.md`, `specs/grain-rendering/spec.md`, `apply-progress.md`, `verify-report.md`, `explore.md`, `archive-report.md`.

Note: the engram design observation (#465) predates the amendments. The filesystem `design.md` is authoritative.

## Known Limitations

Same set as `design.md` "Known limitations" and `verify-report.md` "Known limitations (recorded)".

1. Coherent DC clipping: pre-clamp peak ~9.3 at gain 2 (~4.6 at gain 1), hard-clipped at ±4. R-GR-7 compliant; soft limiter deferred to the plugin stage.
2. `AllocationCounter` sees only the global `operator new`/`new[]` (and aligned forms), not direct `malloc` or third-party allocators.
3. The realtime extremes test does not assert active grains.
4. The JUCE-free guard inspects only `LINK_LIBRARIES`/`INTERFACE_LINK_LIBRARIES` of `grains_core` (not include paths or sources) and was verified by temporary mutation, not by a permanent test.
5. The extremes grid runs 0.25 s per combination vs spec 1 s (the pool saturates at ~0.13 s).
6. R-GR-7 sample-rate grain-length scaling and the pitch +100 == +24 engine-level identity are weakly covered.
7. Determinism holds per binary, flags, and compiler.
8. No sanitizer or coverage runs.

## Follow-ups (not implemented)

| ID | Item | Tracking |
|---|---|---|
| a | R-GR-7 sample-rate grain-length scaling test (current check asserts finite/bounded/peak only) | next change |
| b | Engine-level pitch +100 == +24 bit-identical test (currently proven at params level) | next change |
| c | `REQUIRE(activeGrainCount() > 0)` in realtime extremes for non-zero density | next change |
| d | Assert window values in [0,1] in the window test | next change |
| e | ASan/UBSan CI job | next change |
| f | Amend R-GR-7 to state 0.25 s per combination is sufficient once the pool saturates, or add a Release-only 1 s variant; decide at next change | next change |
| g | Roadmap #1 plugin target (VST3 + Standalone); roadmap #8 playback modes | next change |

Resolved from PR2 review: test `R2-001` in `tests/test_scheduler.cpp` (skip-on-full due-time comparison, flagged as a tautology) was rewritten in PR3a (f33d347) to run a real 128-slot GrainPool with a negative control; the PR3a review found it discriminating. No follow-up needed.

## Risks

1. Specs were merged into `openspec/specs/` as part of this archive. To revert the archive, `git revert` the archive commit; this restores the change folder at `openspec/changes/granular-synth/` and removes the main specs together. Do not re-run the spec merge after the move; the main specs are already current.
2. `tests/CMakeLists.txt` and `Source/core/CMakeLists.txt` changed after PR1's review (SHA pin, deferred JUCE guard). The changes are covered by the PR2 and PR4b lineages, whose paths include those files. PR2 finding R4-001 (FIND_PACKAGE_ARGS silent system Catch2) is not present in the final file, which uses an explicit `GRAINS_USE_SYSTEM_CATCH2` opt-in.
3. The engram `sdd/granular-synth/review/*` topics are absent. Receipts on disk are the authority.
4. Open question carried in design.md: `CMakePresets.json` not decided.
