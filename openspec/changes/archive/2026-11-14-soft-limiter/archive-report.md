# Archive Report: soft-limiter

**Archived**: 2026-11-14
**Archive path**: `openspec/changes/archive/2026-11-14-soft-limiter/` (copied by the archive executor; the orchestrator verified the copies identical to the originals and removed `openspec/changes/soft-limiter/` in the archive commit bfae342)
**Mode**: hybrid (openspec + engram `sdd/soft-limiter/*`)
**Branch**: feat/soft-limiter (single PR, single commit, roadmap P1)
**Verdict carried from verify**: PASS WITH WARNINGS (0 CRITICAL, 3 WARNING, 2 SUGGESTION)
**Archive status**: intentional archive with warnings (orchestrator-directed; see Dispatcher Note in Review Gate)

## Commit Stack

| Slice | Commit | Subject | Review lineage |
|---|---|---|---|
| feat(core) | ad0351c | feat(core): soft-limit engine output to a 1.0 ceiling (pushed on feat/soft-limiter) | review-7234f34a862850b9 (4R, high) |
| docs(openspec) | bfae342 | docs(openspec): archive soft-limiter change (tasks.md 3.2/4.4 ticks, verify report, merged main specs, folder move) | review-2bad917fd7010cd4 (4R, high) |

The archive step itself made no commit; the orchestrator committed it as bfae342.

## Review Gate

```yaml
reviewGate:
  result: allow
  checkedBy: sdd-archive (native gate output supplied by the orchestrator; receipt and state read on disk)
  code_review:
    lineage: review-7234f34a862850b9
    receipt: .git/gentle-ai/review-transactions/v2/review-7234f34a862850b9/review-receipt.json
    state: .git/gentle-ai/review-transactions/v2/review-7234f34a862850b9/review-state.json
    receipt_check: schema gentle-ai.review-receipt/v2, lineage_id matches, generation 1, terminal_state approved,
      risk_level high, selected_lenses review-risk, review-resilience, review-readability, review-reliability,
      base_tree 527f2a2c, initial_review_tree and final_candidate_tree c265ef51, paths_digest sha256:fb2a5099...,
      policy_hash sha256:34fb63d7..., evidence_hash sha256:84c88434..., resolved_finding_ids null
    state_check: state approved; current_snapshot identical to initial_snapshot (candidate_tree, paths_digest,
      intended_untracked_proof); findings [] in all four lenses; original_changed_lines 849, correction_budget 200,
      fix_finding_ids []
  docs_review:
    lineage: review-8198bdad9362f858 (tasks.md, verify-report.md; bound to the change)
    receipt: .git/gentle-ai/review-transactions/v2/review-8198bdad9362f858/review-receipt.json
    receipt_check: terminal_state approved, risk_level medium, selected_lenses review-readability,
      base_tree c265ef51 (the reviewed code candidate), final_candidate_tree 265cf78b, resolved_finding_ids null
    orchestrator_note: 1 WARNING reported by this review (not present in the receipt JSON; recorded from the orchestrator)
  superseded:
    lineage: review-20291cd59c96f8ff
    status: approved, then superseded after the 4 readability suggestions were applied (scope-changed as expected)
  docs_review_second_pass: review-42df073ebed62e3e (tasks.md), superseded by review-8198bdad9362f858
  limits: archive has no shell and did not re-run `gentle-ai review validate` or `gentle-ai sdd-status`;
    live Git tree was not recomputed against the receipt
```

Engram topics `sdd/soft-limiter/review/*` are absent (searched `sdd/soft-limiter/review/receipt`, none found). The receipt and state on disk are the authority, consistent with the loader-hardening and juce-fetch-license archives. Engram #613 is a discovery note about the re-approval, not the exact review topic keys.

### Dispatcher Note

`gentle-ai sdd-status` reports verify/archive blocked with "compact remediation requires a failed evidence revision", while `remediationState.required` is `false` and `reviewGate` is `allow`. The archive proceeds on explicit maintainer direction, the same as loader-hardening. This is recorded as an intentional archive with warnings. Follow-up 7 tracks the upstream investigation.

## Task Completion Gate

`openspec/changes/soft-limiter/tasks.md`: 17/17 checked (Phase 1: 1.1-1.2; Phase 2: 2.1-2.9; Phase 3: 3.1-3.2; Phase 4: 4.1-4.4). No unchecked items remain.

Reconciliation: none. The tasks artifact was already complete when the archive started. The 3.2 and 4.4 ticks were applied by apply and are uncommitted. Engram `sdd/soft-limiter/tasks` #610 still shows 15/17 (its revision predates those two ticks); the file on disk is the source of truth.

Caveat on 4.4: the manual Standalone check was subjective. The maintainer listened at heavy overlap (1000 ms, about 150 gr/s, gain 1.68) and approved. No dBFS meter readings were recorded (Follow-up 1).

## Specs Synced

| Domain | Action | Details |
|---|---|---|
| grain-rendering | Updated `openspec/specs/grain-rendering/spec.md` | MODIFIED R-GR-7: ceiling is now C = 1.0 with strict |y| < 1.0 enforced by R-GR-10; body and the Extremes grid and worst-case scenarios replaced from the delta. ADDED R-GR-10 Soft output limiting (7 scenarios, including one [Manual]). Other requirements unchanged. |
| plugin-shell | Updated `openspec/specs/plugin-shell/spec.md` | MODIFIED R-PS-4: bounded by the engine soft limiter (|y| < 1.0, 0 dBFS); "Sound after load" peak <= 1 (was 4). Out of scope (explicit): "soft limiter" replaced by "lookahead limiting and any user-adjustable ceiling or drive". Assumptions: "no soft limiter, ±4" replaced by the fixed soft limiter with no user control. Heading level kept as `##` to match the main spec (the delta used `###`). |
| sample-source | Updated `openspec/specs/sample-source/spec.md` | MODIFIED Out of scope (explicit): "soft limiter" replaced by "lookahead limiting and any user-adjustable output ceiling or drive". Heading level kept as `##`. No requirement or scenario changes. |

Merge method: requirement blocks replaced in full with the delta content. Non-requirement `##` sections (Out of scope, Assumptions) were merged by hand, because a requirement-only merge misses them (verify WARNING 3). The delta "(Previously: ...)" wording was not carried over. No REMOVED or RENAMED sections exist. Line endings were preserved by in-place edits.

Post-merge check: no main spec (grain-rendering, plugin-shell, sample-source, grain-scheduling, parameter-mapping, build-harness, licensing) still states a ±4 ceiling or "no limiter". The remaining "±4" text is only in archived history (loader-hardening archive, which is not edited).

Copies in the archive folder keep the delta form of the three spec files, as the audit trail requires.

## Verification Evidence (recorded at verify, not re-run during archive)

| Command or check | Result |
|---|---|
| cmake --build build --config Debug | exit 0 (grains_core, grains_tests, grains_realtime_tests) |
| ctest --test-dir build -C Debug --output-on-failure | 131/131 pass (123 before the change, plus 8 new) |
| Strict TDD evidence | RED 7/131 failing before the curve; GREEN 131/131; no tautologies |
| Loudness regression guard (engine_loudness, #76/#77) | unchanged, passes |
| CI linux-sanitizers (clang ASan/UBSan) on ad0351c | green; maintainer-confirmed; NOT observed from this host |
| Manual Standalone check (task 4.4, R-GR-10 manual scenario) | maintainer listened at heavy overlap and approved ("sounds good"); no dBFS readings |
| Archive: no shell, could not run builds, ctest or gentle-ai | verify evidence is cited, not re-run |

## Engram Observation IDs (traceability)

| Artifact | Topic key | Observation ID |
|---|---|---|
| explore | sdd/soft-limiter/explore | 605 (cited in explore.md; not re-fetched by archive) |
| proposal | sdd/soft-limiter/proposal | 607 |
| spec | sdd/soft-limiter/spec | 608 (rev 2) |
| design | sdd/soft-limiter/design | 609 |
| tasks | sdd/soft-limiter/tasks | 610 (revision predates the 3.2/4.4 ticks; file is source of truth) |
| apply-progress | sdd/soft-limiter/apply-progress | 611 |
| negative-zero fix | not verified by archive (orchestrator-supplied ID) | 612 |
| review (re-approval) | discovery (orchestrator-supplied ID) | 613 |
| delivery | not verified by archive (orchestrator-supplied ID) | 614 |
| verify-report | sdd/soft-limiter/verify-report | 616 |
| sdd-init | sdd-init | 455 |
| archive-report | sdd/soft-limiter/archive-report | saved at archive (this report) |
| review receipts | sdd/soft-limiter/review/* | not in engram; on disk, see Review Gate |

Filesystem artifacts (under `openspec/changes/archive/2026-11-14-soft-limiter/`): `proposal.md`, `design.md`, `tasks.md`, `verify-report.md`, `explore.md`, `specs/grain-rendering/spec.md`, `specs/plugin-shell/spec.md`, `specs/sample-source/spec.md`, and this `archive-report.md`. No `apply-progress.md` exists on disk for this change; apply-progress lives in Engram #611.

Merged main specs (not moved): `openspec/specs/grain-rendering/spec.md`, `openspec/specs/plugin-shell/spec.md`, `openspec/specs/sample-source/spec.md`.

## Resolved by This Change

- Hard ±4 clamp (+12 dBFS output above 0 dBFS to the host): replaced by a fixed soft limiter with C = 1.0, knee T = 0.8, stateless and per-sample (R-GR-7, R-GR-10).
- Plugin spec claimed ±4 with "no limiter" (R-PS-4, Assumptions, Out of scope): updated to the engine soft limiter with |y| < 1.0 (R-PS-4).
- sample-source Out of scope listed "soft limiter": updated to list lookahead limiting and user-adjustable ceiling as out of scope.
- Open questions in design.md (transparency range versus the subnormal flush; sample-source out-of-scope line): resolved in the R-GR-10 delta and the sample-source delta.
- Roadmap P1 (soft limiter decision) from loader-hardening archive: closed.

## Known Limitations

1. Clang UBSan (float-cast-overflow) was not run on this host. The CI linux-sanitizers job is green on ad0351c per the maintainer. The R-GR-7 float-cast scenario is UNVERIFIED from this host.
2. The R-GR-10 manual scenario ("never exceeds 0 dBFS on a meter", "quiet passages unchanged") is subjective only. The Catch2 scenarios (#75, #80) cover the numeric claims.
3. Limiter is a static soft clipper, not a true look-ahead limiter. Peaks above the knee are shaped, not gain-reduced; aliasing is accepted (no oversampling).
4. The focused ctest command in tasks.md (`ctest -R "soft_limiter|engine_"`) does not match the `realtime:` tests.
5. The ceiling is not user-adjustable; there is no parameter or state change (9 parameters, version 1).

## Follow-ups (not implemented)

| ID | Item | Tracking |
|---|---|---|
| 1 | tasks.md 4.4 note lacks the caveat that the manual check was subjective (no dBFS meter readings). Optionally record a metered check. | optional, next change or manual session |
| 2 | apply-progress says test_soft_limiter.cpp has 8 cases; it has 7 plus 1 realtime test (the 8 new tests are 7 soft_limiter plus 1 realtime). | cosmetic, next change |
| 3 | tasks.md focused ctest regex `soft_limiter|engine_` misses `realtime:` tests; use `soft_limiter|engine_|realtime`. | next change |
| 4 | Existing `build/` cache rejects `cmake -S . -B build -A x64` (generator-platform mismatch); document or recreate the dir. | dev host |
| 5 | Install and authorize the gh CLI on the dev host so CI results can be observed instead of maintainer-reported. | dev host |
| 6 | True look-ahead limiter (explore approach b with setLatencySamples) remains out of scope. | roadmap |
| 7 | `gentle-ai sdd-status` remediation-state block ("requires a failed evidence revision") reported with no remediation required. Investigate and report upstream. | upstream |

## Risks

1. The archive is an intentional archive with warnings. CRITICAL issues: none. Warnings carried: UBSan CI is maintainer-reported, the manual dBFS check is subjective, and the dispatcher reports a remediation-state block that the receipt does not support.
2. Resolved: the archive executor could not delete the original change folder (no shell or delete tool). The orchestrator compared all 8 copies with the originals (identical ignoring CR) and removed `openspec/changes/soft-limiter/` in bfae342.
3. Resolved: the copies were written with the Write tool; their content was verified identical to the originals, and git normalises line endings on commit. Main specs were edited in place and kept their line endings.
4. Resolved: the tasks.md ticks, verify-report.md, the merged main specs and the folder move were committed together in bfae342, separately from the code commit ad0351c.
5. Archive did not re-run `gentle-ai review validate`, `gentle-ai sdd-status`, or git status (no shell). The `reviewGate: allow` result is the orchestrator's native output; the receipts on disk were read and match it. The live Git tree was not recomputed. Any review-scope check on the final commit must account for `openspec/specs/` and `openspec/changes/archive/`, which are outside the reviewed code path set.
6. Engram observation 605 (explore) and 612 (negative-zero fix) were cited from the orchestrator and the apply artifacts, not re-fetched by the archive executor. Engram `sdd/soft-limiter/tasks` #610 is stale relative to the file on disk (3.2 and 4.4 ticks).
