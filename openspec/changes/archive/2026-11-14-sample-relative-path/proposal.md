# Proposal: Relocatable sample path

## Intent

State stores only the absolute `samplePath`. When a project moves to another machine or user account, or its sample folder moves, restore fails with "Missing: <name>". This change resolves roadmap item P2.

## Scope

### In Scope
- A new JUCE-free core unit `grains::SamplePathResolver` (`std::string` only, no `std::filesystem`): `makeRef`, `candidates`, `resolve` with an injected `exists` predicate.
- Additive state: keep `samplePath`; add `sampleName`, and `sampleAnchor` + `sampleRel` when the file lies under an anchor (Music, Documents, Desktop, Home; the longest prefix wins).
- Candidate order: anchor + relative path on this machine, then the stored absolute path, then the name in each search directory (non-recursive, deduplicated, about 10 at most).
- `SampleLoader` accepts a candidate list. `Result.path` reports the loaded path. A relocated Restore shows `"Loaded: <name> (relocated)"`.
- Legacy states: the name is derived from the old `samplePath`.

### Out of Scope
- Embedded audio, dropping `samplePath` (privacy follow-up), size or hash check, last-used directory, rewriting state after a relocated Restore, a Downloads anchor, recursive search.

## Capabilities

### New Capabilities
None.

### Modified Capabilities
- `sample-source`: R-SS-8 MODIFIED (stores the reference fields; restore tries candidates). ADDED: resolver candidate ordering and dedup; legacy-state compatibility; relocated status hint. Remove "relative-path lookup" from Out of scope.
- `plugin-shell`: R-PS-7 text and Assumptions now say "sample reference" instead of "absolute path". Remove "relative-path lookup" from Out of scope.

## Approach and work units

Path shape (Windows drive or UNC vs POSIX) is read from the path itself, not the host OS. Matching is case-insensitive for Windows-shaped paths. The generation gate and locking in `SampleLoader` stay unchanged; only the payload shape changes. Parameters (9, version 1) are unchanged.

| WU | Commit | Lines |
|---|---|---|
| 1 | `feat(core): add SamplePathResolver` (unit, Catch2 tests, CMake) | ~500 |
| 2 | `feat(plugin): restore samples from relocated paths` (state, loader, status, spec deltas) | ~200 |

## Review Workload Forecast

- Estimated changed lines: ~700
- 800-line budget risk: Medium
- Chained PRs recommended: No (one PR, two commits)
- Decision needed before apply: No
- Cut lines if the count passes 800: (1) split into chained PRs at the WU boundary; (2) otherwise drop the search-directory step (3) and keep (a) only.

## Affected Areas

| Area | Impact |
|---|---|
| `Source/core/include/grains/SamplePathResolver.h`, `Source/core/src/SamplePathResolver.cpp` | New |
| `tests/test_sample_path_resolver.cpp`, `tests/CMakeLists.txt`, core CMake source list | New / Modified |
| `Source/PluginProcessor.cpp/.h`, `Source/SampleLoader.*` | Modified |
| `openspec/specs/sample-source`, `openspec/specs/plugin-shell` | Delta |

## Risks

| Risk | Likelihood | Mitigation |
|---|---|---|
| A same-named file loads silently | Med | Strict order, no recursion, "(relocated)" hint |
| Cross-OS separator bugs (`\` is legal on POSIX) | Med | Shape-based detection, Catch2 cases |
| Slow probes on network shares | Low | Small list, sequential, loader thread only |
| Loader invariants regress | Low | Payload-only change; manual R-SS-7 checks |
| Privacy stays partial | High (accepted) | Follow-up recorded |

## Rollback Plan

Revert the two commits. The new state fields are ignored by the old code, and `samplePath` is still written, so saved sessions load in both directions. No migration.

## Success Criteria

- [ ] Resolver tests cover anchors, separators, drive/UNC, ordering, dedup, legacy and garbage input.
- [ ] Moving the anchored folder's parent or switching accounts restores the sample with "(relocated)".
- [ ] Old-format states still restore; a missing file still shows "Missing: <name>".
- [ ] Core stays JUCE-free; existing tests pass.

## Proposal question round

Auto mode. These assumptions are for the user to review:
1. Loading a same-named file from a search directory is acceptable when flagged "(relocated)".
2. Keeping `samplePath` (privacy) is acceptable for now.
3. Four anchors are enough; Downloads waits.
