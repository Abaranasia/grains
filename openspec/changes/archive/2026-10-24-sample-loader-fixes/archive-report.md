# Archive Report: sample-loader-fixes

**Archived**: 2026-10-24
**Archive path**: `openspec/changes/archive/2026-10-24-sample-loader-fixes/` (folder move done by the orchestrator with git, before the commit; this report was written in the change folder and moves with it)
**Mode**: hybrid (openspec + engram `sdd/sample-loader-fixes/*`)
**Branch**: feat/sample-loader-fixes @ 2bc44ee (single PR, 3 commits)
**Verdict carried from verify**: PASS WITH WARNINGS (0 CRITICAL, 1 WARNING, 3 SUGGESTION)

## Commit Stack

| Slice | Commit | Subject | Review lineage |
|---|---|---|---|
| core | b4559e0 | fix(core): add GenerationGate and staged SourceExchange commit | review-2ad30e7dc1494499 (4R, risk high) |
| plugin | 4143e53 | fix(plugin): gate SampleLoader exchange mutations | review-f3c4d60a4fa9ba32 (review-reliability, risk medium) |
| records | 2bc44ee | docs(sample-loader-fixes): record commits and manual verification | review-b50579fc8d21a0be (review-reliability, risk medium) |

## Review Gate

```yaml
reviewGate:
  result: allow
  checkedBy: sdd-archive (native gate output supplied by the orchestrator, plus receipts on disk)
  command: gentle-ai review validate --gate post-apply --lineage review-b50579fc8d21a0be
  allowed: true
  reason: "authoritative transaction, current repository target, and content-bound artifacts match"
  lineages:
    - commit: b4559e0
      lineage: review-2ad30e7dc1494499
      terminal_state: approved
      final_candidate_tree: 8b999d3862f07deea0c6d6b678c21267c60d6935
    - commit: 4143e53
      lineage: review-f3c4d60a4fa9ba32
      terminal_state: approved
      final_candidate_tree: e499f741c04927d9a0fa6c8352f11df80d4e733b
    - commit: 2bc44ee
      lineage: review-b50579fc8d21a0be
      terminal_state: approved
      final_candidate_tree: be27c8a9cb114c2271651f9f81a487fb988b74e0
  receipt: .git/gentle-ai/review-transactions/v2/review-b50579fc8d21a0be/review-receipt.json
  receipt_check: read directly by sdd-archive; schema gentle-ai.review-receipt/v2, terminal_state approved, lineage_id matches; base_tree of each receipt equals the final_candidate_tree of the previous commit's receipt
  bound_via: bind-sdd
```

Receipts live under `.git/gentle-ai/`, not under `openspec/`. The engram topics `sdd/sample-loader-fixes/review/{transaction,ledger,receipt,gate-context}` do not exist (searched, none found), which is expected because the receipts on disk are the authority.

## Task Completion Gate

`openspec/changes/archive/2026-10-24-sample-loader-fixes/tasks.md`: no unchecked implementation tasks (`- [ ]` count 0). Checked items: 1.0-1.14 (15), 2.1-2.5 (5), 3.1 (1), total 21 of 21. The 2.5 manual Standalone checks were confirmed by the user on 2026-10-24. No stale-checkbox reconciliation was needed. tasks.md was not edited by archive.

## Specs Synced

| Domain | Action | Details |
|---|---|---|
| sample-source | Updated `openspec/specs/sample-source/spec.md` | MODIFIED: test-layer legend, R-SS-7, R-SS-9, R-SS-10. ADDED: R-SS-11. Result: 11 requirements (R-SS-1..11), 37 scenarios (22 [Catch2], 15 [Manual]). Was 10 requirements and 24 scenarios. New scenarios: R-SS-7 "Superseded successful load does not publish", R-SS-9 "Empty path on reload" and "Superseded restore does not clear", R-SS-10 "Truncated file shows status" and "Short file shows no truncation text", and 8 R-SS-11 Catch2 scenarios. |
| plugin-shell | Updated `openspec/specs/plugin-shell/spec.md` | MODIFIED: test-layer legend and R-PS-2 wording only (process and change-relative phrases removed). No scenario changes. Result: 7 requirements, 15 scenarios (14 [Manual], 1 [Catch2]), unchanged in count. |

The "(Previously: ...)" and "(Unchanged from previous.)" markers from the deltas were dropped. Requirements not in the deltas were preserved verbatim.

## Verification Evidence (recorded at verify, not re-run during archive)

| Command | Result |
|---|---|
| fresh `cmake --build build/verify-slf --config Debug` (MSVC, plugin OFF) | exit 0, no warnings |
| `ctest --test-dir build/verify-slf -C Debug` | 122/122 passed (baseline 111 + 11 new) |
| `ctest -R gate` x5 | 5/5 runs green |
| `cmake --build build-plugin --target grains_plugin --config Debug` | exit 0 |
| JUCE-free core | `rg juce Source/core` matches only the CMake assertion helper |

Manual scenarios are covered by the user's confirmation of all 5 task 2.5 checks in the Debug Standalone built from 4143e53.

## Engram Observation IDs (traceability)

| Artifact | Topic key | Observation ID |
|---|---|---|
| proposal | sdd/sample-loader-fixes/proposal | 527 |
| spec | sdd/sample-loader-fixes/spec | 528 |
| design | sdd/sample-loader-fixes/design | 529 |
| tasks | sdd/sample-loader-fixes/tasks | 530 |
| apply-progress | sdd/sample-loader-fixes/apply-progress | 531 |
| implementation note (related, not an SDD artifact) | (bugfix, no topic key) | 532 |
| verify-report | sdd/sample-loader-fixes/verify-report | 533 |
| archive-report | sdd/sample-loader-fixes/archive-report | this report (upserted at archive) |
| review receipts | not in engram | on disk, see Review Gate |

Filesystem artifacts (post-move paths under `openspec/changes/archive/2026-10-24-sample-loader-fixes/`): `proposal.md`, `design.md`, `tasks.md`, `apply-progress.md`, `verify-report.md`, `specs/sample-source/spec.md`, `specs/plugin-shell/spec.md`, and this `archive-report.md`.

## Resolved by This Change

- W1 (SampleLoader publish race between `isCurrent()` and `publish`, carried from sample-loader): CLOSED. Every exchange mutation now goes through `commitGated` (`gate_.commitIf`) at the three sites: empty-path restore clear, restore-failure clear in `fail()`, and publish after decode.
- Truncation status spec gap (carried from sample-loader R-SS-5/R-SS-10 follow-up): RESOLVED. Truncation status text is specified in R-SS-10 and verified manually.

## Known Limitations

1. OOM on the empty-restore clear path posts "Error: No sample" (stageClear allocation failure). The label is unspecified by R-SS-9. Behaviour keeps the previous source. Documented in design.md as a deviation from R-SS-9 silence. See follow-up W-A.

## Follow-ups (not implemented)

| ID | Item | Tracking |
|---|---|---|
| W-A (new, WARNING) | OOM restore label: the empty-restore clear path posts "Error: No sample" when `stageClear()` fails to allocate. R-SS-9 does not specify a label for this failure. Either specify it in R-SS-9 or accept the current label. | next change |
| S-A (new, SUGGESTION) | G5 stress test could also assert commits > 0 to prove the stress is exercised. It currently asserts no commit count and could pass vacuously. | next change |
| W2 | `stopThread(4000)` in `SampleLoader::stop()` can force-kill the thread if a slow chunk read or decode blocks beyond 4 s (for example on a network drive), risking leaks or torn state at shutdown. Fix: longer or unbounded wait, or cancellable reads. | next change |
| S1 | `frees()` test assertions could be elided by an optimizing compiler in Release. Make the frees path observable (volatile or escape). | next change |
| S2 | `maxSourceFrames` floor and INT_MAX clamp branches have no tests. | next change |
| S3 | The collect-after-publish rule (`publish; collect;`) is documented only in a class comment. Consider a helper that enforces it. | next change |
| S4 | Unused `<cmath>` include in `tests/test_sample_builder.cpp`. | next change |
| S5 | Add a rationale comment for `kMaxSourceSeconds` (5 minute cap). | next change |
| Privacy | Absolute file paths are stored in plugin state (R-SS-8). Review the privacy exposure of absolute paths. | next change |
| P1 | Soft limiter decision: output can reach the ±4 ceiling with the drone always on. | roadmap |
| P2 | Embedded audio and relative-path state (both out of scope; only absolute paths are stored). | roadmap |
| P3 | Load-time resampling (out of scope; the builder does not resample). | roadmap |
| P4 | VST3-in-DAW manual checks (host automation, MIDI ignored, rates and block sizes), waived for sample-loader. | next change or manual session |
| P5 | JUCE FetchContent fallback, plus adding a LICENSE file (AGPLv3) and the related licensing change. | before release |
| P6 | Plugin identity codes and bundle ID (placeholders, company "Grains"). | before release |
| P7 | Plugin CI job (out of scope; CI stays OFF per R-BH-3). | next change |

## Risks

1. The folder move is performed by the orchestrator with git (`openspec/changes/sample-loader-fixes/` to `openspec/changes/archive/2026-10-24-sample-loader-fixes/`). The folder contains the untracked `verify-report.md` and this report, so a plain folder move plus `git add` is needed.
2. The engram review topics (`sdd/sample-loader-fixes/review/*`) are absent. The gate was verified from the orchestrator's native output and the receipts read on disk.
3. The engram `sdd/sample-loader-fixes/archive-report` topic did not exist before this save, so no earlier blocked record was overwritten.
4. The candidate tree hashes are copied from the receipts on disk, not recomputed by archive.
