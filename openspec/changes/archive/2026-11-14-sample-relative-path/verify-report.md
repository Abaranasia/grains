# Verify Report: sample-relative-path

Status note (added at archive, 2026-11-14): this report was written before manual tasks 4.1–4.3 were run; see the Addendum at the end.

Mode: Strict TDD, hybrid store. Branch feat/sample-relative-path-plugin (960fec4 core, 25532c3 plugin). Working tree clean.

## Verdict: PASS WITH WARNINGS (manual checks UNVERIFIED)

All automated evidence passes. 18 of 40 spec scenarios are [Manual] and have NOT been executed: tasks 4.1-4.3 are pending the maintainer. They are recorded as UNVERIFIED, not as passed. Archive should wait for those checks or an explicit maintainer waiver.

## Completeness

| Tasks | Done | Pending |
|-------|------|---------|
| 1.1-1.3, 2.1-2.11, 3.0-3.6 | 21 of 21 | none |
| 4.1, 4.2, 4.3 (interactive manual Standalone checks) | 0 | maintainer, user away |
| 4.4 (spec sync at archive) | 0 | archive time |

Note: the sdd-verify skill treats unchecked core tasks as CRITICAL. Per the orchestrator brief these are maintainer-gated manual checks and are reported as UNVERIFIED/PENDING rather than counted as CRITICAL. This is the main reason the verdict is not a plain PASS.

## Runtime evidence (run by verifier)

| Command | Exit | Result | Output hash (sha256 prefix) |
|---------|------|--------|-----------------------------|
| cmake --build build --config Debug | 0 | all targets built | cf229eb5f349217 |
| ctest --test-dir build -C Debug --output-on-failure | 0 | 173/173 passed | c58839a8f747ad1 |
| ctest -R sample_path (focused) | 0 | 42/42 passed | n/a |
| grains_tests.exe [sample_path] | 0 | 184 assertions in 42 test cases | n/a |
| cmake --build build/plugin-jf2 --config Debug --target GrainsPlugin_Standalone | 0 | Grains.exe linked, 0 warning lines | 6a799256208ba4c |

Coverage tool: not available (skipped). Linter/type checker: not configured (skipped).

## Spec compliance matrix

Counts: 5 requirements (R-SS-8, R-SS-12, R-SS-13, R-SS-14, R-PS-7), 40 scenarios (22 Catch2, 18 Manual). Tests are in tests/test_sample_path_resolver.cpp, names prefixed "sample_path:".

### R-SS-12 (21 Catch2 scenarios): all PASS

| Scenario | Evidence (test name) | Status |
|---|---|---|
| makeRef finds an anchor | makeRef finds the anchor and stores the relative path with... | PASS |
| Longest anchor wins | makeRef picks the longest anchor | PASS |
| Whole components only | makeRef matches whole components only | PASS |
| Path outside every anchor | makeRef without a containing anchor gives name only | PASS |
| Windows drive case-insensitive | makeRef ignores case on Windows paths but keeps the original | PASS |
| UNC case-insensitive | same test (UNC case, anchor SRV/Sh/Music) | PASS |
| Forward slashes | makeRef accepts forward slashes on Windows paths | PASS |
| POSIX case-sensitive | makeRef is case-sensitive on POSIX paths | PASS |
| Backslash is a POSIX filename char | a backslash is a filename character on POSIX | PASS |
| Dot-dot gets no anchor | a dot-dot path gets no anchor | PASS |
| Name extraction across shapes | name extraction across path shapes | PASS |
| Candidate order | candidates are ordered anchor, absolute, search directories | PASS |
| Missing anchor skipped | candidates skip an anchor this machine lacks | PASS |
| Duplicates keep first | candidates drop duplicates keeping the first position | PASS |
| Dedup case-insensitive on Windows | candidates dedup is case-insensitive only for Windows paths | PASS |
| Cap at 10 | candidates are capped at ten entries keeping the first ten | PASS |
| Trailing separator | candidates treat search directories with and without trailing... | PASS |
| Resolve first existing, stops early | resolve returns the first existing candidate and stops there | PASS |
| Resolve not found, in order | resolve reports not found after trying every candidate in order | PASS |
| Resolve empty list | resolve on an empty list never calls the predicate | PASS |
| Empty/garbage input | empty and separator-only input is harmless; embedded NUL and control characters...; a very long path... | PASS |

Extra hardening tests (task 3.0, beyond spec): unsafe name/relative, POSIX backslash traversal (2 tests), colon on Windows targets. All pass.

### R-SS-13

| Scenario | Evidence | Status |
|---|---|---|
| Legacy reference derives the name [Catch2] | a legacy reference lists the absolute path then the name in...; restoreRef derives the name for legacy state | PASS |
| Legacy state with a moved file [Manual] | pending 4.2 (resolver order is unit-tested; loader glue is not) | UNVERIFIED |
| Stray fields with empty path [Manual] | pending 4.3; resolver half covered by "an empty samplePath clears every other field" (PASS); the loader No sample path is code-inspected only (SampleLoader.cpp lines 86-93) | UNVERIFIED |

### R-SS-8 (4 Manual): all UNVERIFIED

State round trip; anchored file writes all four fields; file outside anchors writes two; old-format state restores. Code inspection: onLoadResult writes samplePath, sampleName, sampleAnchor and sampleRel after a User load and removes the anchor pair when unanchored (PluginProcessor.cpp lines 155-166). Build passes. Not exercised at runtime. UNVERIFIED (pending 4.1).

### R-SS-14 (7 Manual): all UNVERIFIED

Relocated by anchor, moved parent folder, unmoved file shows no hint, missing file label unchanged, relocated restore does not rewrite state, superseded candidate load does not publish, relocated plus truncated. Code inspection is consistent with each: relocated is computed with samePath against the original (SampleLoader.cpp line 123); status text at lines 166-168; Restore never writes properties; commitGated and the generation gate are unchanged. Not exercised. UNVERIFIED (pending 4.2, 4.3).

### R-PS-7 (5 Manual): all UNVERIFIED

State round trip, bad state data (early return on null/empty/non-matching XML, setStateInformation lines 126-128, code-inspected only), old-format restore, moved folder restore, missing label. UNVERIFIED (pending 4.1-4.3).

## Strict TDD compliance

| Check | Result | Details |
|---|---|---|
| TDD evidence reported | WARNING | apply-progress #632 has per-unit RED/GREEN narrative (39 cases unit 1; task 3.0: 3 RED then GREEN, 173/173) but no formal TDD Cycle Evidence table. The module says CRITICAL when no table exists; treated as WARNING because concrete counts are reported and re-verified here |
| Test files exist | PASS | tests/test_sample_path_resolver.cpp, 42 TEST_CASEs, registered in tests/CMakeLists.txt |
| GREEN confirmed | PASS | 42/42 focused, 173/173 full, run now |
| Triangulation | PASS | multiple cases per behavior (shapes, styles, caps, order) |
| Safety net | PASS | unit 1 files new; 173/173 baseline preserved |
| Plugin glue RED | WARNING | tasks 3.1-3.5 have no RED test: the JUCE plugin target is not in ctest. Verified only by build, resolver tests and a 4 second launch. Disclosed in apply-progress |

Assertion quality: 164 REQUIRE/CHECK lines; no tautologies (search for REQUIRE(true)/CHECK(true) is empty); tests call production code and assert concrete values; no ghost loops observed. Layer: all 42 are unit tests (Catch2); 0 integration, 0 E2E.

## Design coherence

All design decisions are followed: struct of statics; shape-based detection, case rule and name split; relative stored with slash; empty samplePath yields no candidates; search dirs are the anchor dirs, non-recursive (PluginProcessor.cpp lines 135-136); probing sequential on the loader thread with cap 10; relocated flag via samePath in the loader; request() copies before lock_ and gate_.next() under lock_; decode errors do not fall through to the next candidate. No design deviations.

## Review follow-up: SampleLoader.cpp line 86, "No sample" vs "Missing: name"

Question: Restore with non-empty samplePath but an empty candidate list shows "No sample". Is that a spec deviation?

Finding: a narrow deviation from the letter of R-SS-14 (no candidate exists or loads, so the label reads "Missing: name"), not of R-SS-13 or R-SS-9. Those two only give "No sample" for an empty or absent samplePath, and that case is handled correctly (the resolver clears every field and the processor passes an empty list). The mismatch needs samplePath non-empty while the resolver still returns an empty list: a non-absolute stored path (absolute entry skipped) AND no search directories (Music, Documents, Desktop, Home all resolving empty), or no derivable name. On a real machine localAnchors() returns at least Home, so name entries exist and the list is non-empty. Practically unreachable; severity WARNING (low), not CRITICAL. Suggested later fix: distinguish "samplePath empty" from "list empty" and use the stored or derived name for "Missing:" in the latter case. No unit-test seam exists for it today.

## Issues

CRITICAL: none (see the completeness note on pending manual tasks).

WARNING
1. 18 [Manual] scenarios UNVERIFIED; tasks 4.1-4.3 pending the maintainer. Archive should wait or be explicitly waived.
2. apply-progress lacks a formal TDD Cycle Evidence table (narrative evidence only).
3. Plugin glue (tasks 3.1-3.5) has no automated RED/GREEN coverage; its correctness rests on code inspection, build and the pending manual checks.
4. Low-severity R-SS-14 deviation: empty candidate list with non-empty samplePath shows "No sample" instead of "Missing: name" (SampleLoader.cpp line 86), practically unreachable.
5. Task 4.4 spec sync still to do at archive.

SUGGESTION
1. Add a processor-free seam so the Restore-empty-list label can be unit-tested.
2. The proposal places plugin files under Source/; they are under Source/plugin/ (design already notes it).
3. Non-ASCII case folding on Windows is not handled (accepted in design open questions).

## Addendum: manual checks after verification

Written at archive (2026-11-14). The original body above is unchanged. It records the outcome of tasks 4.1–4.3, which the maintainer and orchestrator ran after this verification.

- **4.1 (passed, partial).** Maintainer saved a sample from inside an anchor folder and restored it in the Standalone; restore OK. Passed: the anchored save and restore. Not exercised: the four saved fields were not inspected. Still open as 4.1b: file outside anchors writes two fields; old-format state restores without "(relocated)". That is covered only by the R-SS-13 legacy Catch2 test and code review.
- **4.2 (passed, partial).** Maintainer moved the file into another anchor folder and saw "(relocated)"; deleted it and saw "Missing: <name>". Not exercised, open as 4.2b: the moved parent folder case, the stored-vs-derived name in the Missing label, and nested-subfolder non-recursion.
- **4.3 (passed, partial).** Orchestrator ran the Standalone (Debug, 25532c3) with crafted settings states and read the label through UI Automation. Stray fields with empty `samplePath` (a real file existed at the anchored path): "No sample", nothing loaded. Relocated plus truncated: "Loaded: long.wav (relocated) (truncated to 5 min)". Saved `samplePath` unchanged after close. Not exercised, open as 4.3b: the superseded restore interactively. It relies on the superseded cases in test_generation_gate.cpp and test_source_exchange.cpp and on the unit-2 review's code inspection (lineage review-13297fab4f346d9b) that commitGated and GenerationGate were not modified.

The review SUGGESTION at SampleLoader.cpp:86 (non-empty samplePath with an empty candidate list shows "No sample") is a different case from the 4.3 stray-fields check. The 4.3 check uses an empty samplePath, which the resolver clears before any candidate list is built. The SampleLoader.cpp:86 case needs a non-empty samplePath with no usable candidate. It remains a follow-up and was not exercised by 4.1–4.3.
