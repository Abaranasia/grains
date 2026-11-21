# Archive Report: loader-hardening

**Archived**: 2026-11-14
**Archive path**: `openspec/changes/archive/2026-11-14-loader-hardening/` (folder move done by the orchestrator with `git mv`; this report was written in the change folder and moves with it)
**Mode**: hybrid (openspec + engram `sdd/loader-hardening/*`)
**Branch**: feat/loader-hardening (4 commits, single PR, targets feat/juce-fetch-license)
**Verdict carried from verify**: PASS WITH WARNINGS (0 CRITICAL, 1 WARNING, 2 SUGGESTION)
**Archive status**: intentional archive with warnings (see Task Completion Gate)

## Commit Stack

| Slice | Commit | Subject | Review lineage |
|---|---|---|---|
| fix(plugin) | 3f8f65b | fix(plugin): wait for the loader thread at shutdown instead of killing it | review-7ed51b98a41716fe (4R) |
| test | 415a457 | test: harden the gate stress, frees and source cap tests | review-7ed51b98a41716fe (4R) |
| docs(core) | 5a732ee | docs(core): document the source cap and where collect is enforced | review-7ed51b98a41716fe (4R) |
| build | aade368 | build: keep the JUCE pin in one file and key the CI cache on it | review-7ed51b98a41716fe (4R) |

## Review Gate

```yaml
reviewGate:
  result: allow
  checkedBy: sdd-archive (native gate output supplied by the orchestrator; receipt and state read on disk)
  gate: pre-commit (orchestrator output: "authoritative transaction, current repository target, and content-bound artifacts match")
  lineage: review-7ed51b98a41716fe
  receipt: .git/gentle-ai/review-transactions/v2/review-7ed51b98a41716fe/review-receipt.json
  state: .git/gentle-ai/review-transactions/v2/review-7ed51b98a41716fe/review-state.json
  receipt_check: schema gentle-ai.review-receipt/v2, lineage_id matches, generation 1, terminal_state approved,
    risk_level high, selected_lenses review-risk, review-resilience, review-readability, review-reliability,
    base_tree 027d1a8b, initial_review_tree and final_candidate_tree ab52c008, paths_digest sha256:42b7f74d...,
    policy_hash sha256:34fb63d7..., evidence_hash sha256:25125691..., resolved_finding_ids null
  state_check: review-state.json state approved; current_snapshot identical to initial_snapshot
    (candidate_tree, paths_digest, intended_untracked_proof); findings [] in all four lenses;
    original_changed_lines 855, correction_budget 200, fix_finding_ids []
  limits: archive has no shell and did not re-run `gentle-ai review validate`; live Git tree was not recomputed
```

Engram topics `sdd/loader-hardening/review/*` are absent (searched, none found). The receipt and state on disk are the authority, consistent with the juce-fetch-license archive.

## Task Completion Gate

`openspec/changes/loader-hardening/tasks.md`: 4 unchecked items, all outside the automated implementation scope:

| Task | Type | Status at archive |
|---|---|---|
| 1.8 | [Manual] W2 stalled-read shutdown | pending user (verify: MANUAL PENDING) |
| 1.9 | [Manual] W-A failed clear allocation | pending user (verify: MANUAL PENDING) |
| 4.11 | [Manual] single-source cache key | pending user (verify: MANUAL PENDING) |
| 6.1 | Post-push: push branch, open PR, observe first CI run | cannot run before archive |

Every automated implementation task is checked (1.1-1.7, 1.10, 2.1-2.9, 3.1-3.4, 4.1-4.10, 4.12, 5.1-5.3). The four commit tasks were checked by the orchestrator with commit SHAs.

Reconciliation: none performed. These are not stale checkboxes, because apply-progress and verify-report show the manual checks are not yet executed. `tasks.md` was not edited by archive, and the four items remain unchecked in the archived copy.

Reason for archiving with open items: the orchestrator explicitly directed an archive with 1.8, 1.9, 4.11 carried as user-owned pending items and 6.1 carried as a post-push follow-up. This is recorded as an intentional archive with warnings. CRITICAL issues: none, so the verify block does not apply.

## Specs Synced

| Domain | Action | Details |
|---|---|---|
| sample-source | Updated `openspec/specs/sample-source/spec.md` | MODIFIED R-SS-5 (+4 scenarios: cap floored, floors to zero rejected, clamped to INT_MAX, extreme rate); MODIFIED R-SS-7 (+1 scenario: stalled read at shutdown); MODIFIED R-SS-9 (+1 scenario: failed clear allocation). Requirements 11 (unchanged); scenarios 37 to 43. Out of scope (explicit) section preserved. |
| build-harness | Updated `openspec/specs/build-harness/spec.md` | MODIFIED R-BH-3: pin now referenced from `cmake/JucePin.cmake`; the literal SHA is removed from the requirement text; fetch-enabled scenario reworded. Scenarios stay at 8. MODIFIED R-BH-5: single pin file rule added; cache key derived from `cmake/JucePin.cmake`; "single source of truth" scenario added; cached-and-bounded scenario reworded. R-BH-5 scenarios 8 to 9. R-BH-1, R-BH-2, R-BH-4 and the Purpose section unchanged. Result: 5 requirements, 22 scenarios (was 21). |

Merge method: each MODIFIED requirement block was replaced in full with the delta content. The "(Previously: ...)" markers from the deltas were dropped, because they are change-relative wording and the main specs must not reference the change. No content was otherwise altered. The main specs carry no requirement or scenario count header or legend, so no count line was updated. No REMOVED or RENAMED sections exist in either delta.

Note on the build-harness baseline: the main spec had 21 scenarios before this change, matching the juce-fetch-license archive report. The archive merge first duplicated the "Plugin job builds through the fetch path" scenario; the orchestrator removed the duplicate before committing.

## Verification Evidence (recorded at verify, not re-run during archive)

| Command or check | Result |
|---|---|
| cmake --build build --config Debug | exit 0 |
| ctest --test-dir build -C Debug --output-on-failure | 123/123 pass |
| ctest -R "own generation" --repeat until-fail:5 | pass x5 |
| cmake --build build-plugin --config Debug --target grains_plugin | exit 0 (VST3 and Standalone built) |
| JUCE SHA and SHA256 literals | only in cmake/JucePin.cmake (outside openspec) |
| ci.yml cache key | `hashFiles('cmake/JucePin.cmake')` at line 74 |
| stopThread calls in Source/plugin | only `stopThread(-1)` |
| `gen + 1` leftover in tests | none |
| Commit attribution | none |

## Engram Observation IDs (traceability)

| Artifact | Topic key | Observation ID |
|---|---|---|
| proposal | sdd/loader-hardening/proposal | 597 |
| spec | sdd/loader-hardening/spec | 599 |
| design | sdd/loader-hardening/design | 600 |
| tasks | sdd/loader-hardening/tasks | 601 (revision 2) |
| apply-progress | sdd/loader-hardening/apply-progress | 602 |
| verify-report | sdd/loader-hardening/verify-report | 604 |
| archive-report | sdd/loader-hardening/archive-report | saved at archive (this report) |
| review receipts | sdd/loader-hardening/review/* | not in engram; on disk, see Review Gate |

Filesystem artifacts (post-move paths under `openspec/changes/archive/2026-11-14-loader-hardening/`): `proposal.md`, `design.md`, `tasks.md`, `apply-progress.md`, `verify-report.md`, `explore.md`, `specs/sample-source/spec.md`, `specs/build-harness/spec.md`, and this `archive-report.md`.

Merged main specs (not moved): `openspec/specs/sample-source/spec.md`, `openspec/specs/build-harness/spec.md`.

## Resolved by This Change

- W2 (WARNING, `stopThread(4000)` could force-kill the loader thread): CLOSED. `stopThread(-1)` with rationale comment; R-SS-7 specifies the no-timeout wait.
- W-A (WARNING, failed clear label unspecified): CLOSED. R-SS-9 specifies the failed-clear behaviour and labels; the code was already correct, so no code change.
- S-A (G5 stress test could pass without committing): CLOSED. Deadline-bounded first-commit wait, then `CHECK(commits > 0)` after join.
- S1 (`frees()` assertions could be elided): CLOSED. Volatile sink `escape()` in the test.
- S2 (maxSourceFrames floor and INT_MAX clamp untested): CLOSED. New Catch2 test case with five checks and a BadSampleRate check.
- S3 (collect-after-publish rule only in a class comment): CLOSED. Comment in SourceExchange.h names `commitGated`.
- S4 (unused `<cmath>` include): CLOSED. Removed.
- S5 (no rationale for `kMaxSourceSeconds`): CLOSED. Comment added in SampleBuilder.h.
- V1 (JUCE SHA duplicated in CMake and CI): CLOSED. `cmake/JucePin.cmake` is the single source.
- V5 (URL_HASH digest not in CI cache key): CLOSED. The cache key hashes `cmake/JucePin.cmake`, which contains the digest.

## Known Limitations

1. The four [Manual] scenarios (stalled share, failed clear, cache key single source, and the W2 shutdown wait) are not verified at runtime. They are user-owned.
2. The first CI run after push is not observed. A one-time cache miss is expected.
3. Verify SUGGESTION 1: the S-A RED mutation failed on mismatches after 10 s rather than on `commits == 0` as the task text describes. The mutation was reverted and the outcome is harmless; the test still asserts `commits > 0`.
4. A stalled share can now delay host shutdown indefinitely. This is the accepted tradeoff in R-SS-7. A bounded close requires approach C.

## Follow-ups (not implemented)

| ID | Item | Tracking |
|---|---|---|
| Manual 1.8 | [Manual] W2 stalled-read shutdown: close the host mid-read on a stalled or slow network share; expect shutdown to wait with no "killing thread by force" message. | pending user |
| Manual 1.9 | [Manual] W-A failed clear: induce a clear allocation failure; expect the previous source kept and the label "Error: No sample" or "Error: <name>". | pending user |
| Manual 4.11 | [Manual] Single-source cache key: after push, a comment-only change in `CMakeLists.txt` leaves the key unchanged; an SHA change in `cmake/JucePin.cmake` changes it. | pending user |
| Post-push 6.1 | Push the branch, open the PR against `feat/juce-fetch-license` (retarget to `main` after merge), and observe the first CI run: plugin job green with a one-time cache miss; a second run shows a cache hit. | after push |
| Approach B | Testable cancellation: move the chunk loop into core with a cancel predicate so the loader can be unit-tested. Deferred, not scheduled. | roadmap |
| V2 | README line 40 "so ... therefore" wording is redundant. | next change |
| V3 | design.md (juce-fetch-license) shows a placeholder hash where the real SHA256 is known (0bef95e4...93a9, recorded in apply-progress). The archived design is not edited. | next change |
| V4 | G2/G4 "_deps" wording is imprecise (SOURCE_SUBDIR download-only layout). | next change |
| V6 | A full local-JUCE build is not recorded (G4 configured only). | next change |
| V8 | apply-progress.md has no task 2.8 entry. | next change |
| Privacy | Absolute file paths are stored in plugin state (R-SS-8). Review the privacy exposure. | next change |
| P1 | Soft limiter decision: output can reach the ±4 ceiling with the drone always on. | roadmap |
| P2 | Embedded audio and relative-path state (out of scope; only absolute paths are stored). | roadmap |
| P3 | Load-time resampling (out of scope; the builder does not resample). | roadmap |
| P4 | VST3-in-DAW manual checks (host automation, MIDI ignored, rates and block sizes), waived for sample-loader. | next change or manual session |
| P6 | Plugin identity codes and bundle ID (placeholders, company "Grains"). | before release |

## Risks

1. The Task Completion Gate was not met in its strict form. Four items remain unchecked: three [Manual] items and one post-push item. The archive proceeds on the orchestrator's explicit instruction. The archived `tasks.md` shows these as open, which is accurate.
2. Archive did not re-run `gentle-ai review validate`, because this executor has no shell. The `reviewGate: allow` result is the orchestrator's native output. The receipt and state on disk were read and match it.
3. The folder move to `openspec/changes/archive/2026-11-14-loader-hardening/` is performed by the orchestrator with `git mv`. Until it runs, the change folder still exists at `openspec/changes/loader-hardening/`.
4. The main-spec merge touches `openspec/specs/` only, which is outside the reviewed path set in the receipt. Any review-scope check on the final commit must account for this.
5. The archive merge briefly duplicated the "Plugin job builds through the fetch path" scenario in the main build-harness spec; the duplicate was removed before commit (baseline 21, result 22 scenarios).
