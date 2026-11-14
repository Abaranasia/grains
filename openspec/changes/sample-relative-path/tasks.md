# Tasks: Relocatable sample path

## Review Workload Forecast

| Field | Value |
|-------|-------|
| Estimated changed lines | 730-770 |
| 800-line budget risk | Medium |
| Chained PRs recommended | No |
| Suggested split | Single PR, two work-unit commits |
| Delivery strategy | ask-on-risk |
| Chain strategy | pending (not needed) |

Decision needed before apply: No
Chained PRs recommended: No
Chain strategy: pending
800-line budget risk: Medium

Forecast is under 800. If unit 1 tests exceed ~320 lines, re-forecast; above 800 total say "Chained PRs recommended: Yes" (PR 1 = unit 1, PR 2 = unit 2 based on PR 1).

### Suggested Work Units

| Unit | Goal | Likely PR | Focused test command | Runtime harness | Rollback boundary |
|------|------|-----------|----------------------|-----------------|-------------------|
| 1 | `feat(core): add SamplePathResolver` | PR 1 | `ctest --test-dir build -C Debug --output-on-failure -R "sample_path"` | N/A: pure core unit, no runtime boundary | Revert commit: new resolver .h/.cpp, test file, two CMake lines |
| 2 | `feat(plugin): restore samples from relocated paths` | PR 1 | `ctest --test-dir build -C Debug --output-on-failure` (full suite) | Standalone: `cmake --build build/plugin-jf2 --config Debug --target GrainsPlugin_Standalone`, manual checks 3.x | Revert commit: `Source/plugin/SampleLoader.*`, `PluginProcessor.*`, spec deltas |

## Phase 1: Resolver scaffolding (unit 1)

- [x] 1.1 Create `Source/core/include/grains/SamplePathResolver.h` (CRLF, SPDX): `PathAnchor`, `SampleRef`, `struct SamplePathResolver` declarations per design; stubs return empty.
- [x] 1.2 Create `Source/core/src/SamplePathResolver.cpp` with stub bodies; add to `Source/core/CMakeLists.txt`.
- [x] 1.3 Create `tests/test_sample_path_resolver.cpp`; add to `tests/CMakeLists.txt`. Test names prefixed `sample_path:` and tag `[sample_path]`.

## Phase 2: RED then GREEN, per group (unit 1)

Each group: write failing tests, run `ctest -R "sample_path"` (RED), implement in `SamplePathResolver.cpp` (GREEN).

- [x] 2.1 RED/GREEN shape and names: `isWindowsShaped`/`isAbsolute` (drive `\` or `/`, UNC, POSIX, `C:foo`, empty, relative); `fileName` (mixed separators, trailing separator, POSIX name containing `\`).
- [x] 2.2 RED/GREEN `samePath`/normalisation: Windows lowercase and `/`->`\`, POSIX case-sensitive, trailing-separator and repeated-separator collapse.
- [x] 2.3 RED/GREEN `makeRef`: longest anchor, whole-component match (`C:\Music2`), drive/UNC case-insensitive, forward slashes, POSIX case-sensitive, POSIX `we\ird.wav`, anchor equal to path, POSIX anchor vs Windows path, outside every anchor.
- [x] 2.4 RED/GREEN `..` rule: path with a `..` component gets no anchor and no relative (scenario "Dot-dot path gets no anchor").
- [x] 2.5 RED/GREEN relative `/` storage: `makeRef` yields `loops/kick.wav` for Windows and UNC paths (spec scenarios at lines 42, 67).
- [x] 2.6 RED/GREEN `restoreRef`: legacy (absolute only, name derived); `..`, absolute or empty relative cleared; key without relative cleared; `\` in stored relative normalised to `/` (Windows shape; kept on POSIX).
- [x] 2.7 RED/GREEN empty-samplePath rule: `restoreRef("", name, key, rel)` and `candidates` give an empty list regardless of other fields (R-SS-13).
- [x] 2.8 RED/GREEN `candidates`: order anchor, absolute, search dirs; anchor join in each style; unknown key skipped; non-absolute `absolute`; dedup on case and separators; trailing-separator dedup (`E:\a\` vs `E:\a`, spec lines 114-117); cap exactly 10 with 20 dirs; legacy order.
- [x] 2.9 RED/GREEN `resolve`: first true wins and stops early; all false gives `nullopt` with calls in order; empty list never calls predicate.
- [x] 2.10 RED/GREEN garbage input: empty string, only separators, embedded NUL and control characters, very long path (e.g. 100k chars) through `makeRef`, `candidates`, `resolve`; no throw, empty path gives empty name, no anchor, empty list.
- [ ] 2.11 REFACTOR: tidy helpers, run full `ctest --test-dir build -C Debug --output-on-failure` green; commit unit 1.

## Phase 3: Plugin wiring (unit 2, manual-verified)

- [ ] 3.1 `Source/plugin/SampleLoader.h/.cpp`: `request(StringArray candidates, String original, String displayName, Origin)`; build list before `lock_`, move into `pending_`; empty list with Restore clears ("No sample").
- [ ] 3.2 `SampleLoader.cpp` load: `resolve` with `File::existsAsFile`; failure label `"Missing: " + displayName`; `Result{path, relocated}` using `samePath`; status `Loaded: <name>[ (relocated)][truncation]`.
- [ ] 3.3 `Source/plugin/PluginProcessor.h/.cpp`: `localAnchors()` (music, documents, desktop, home; skip empty); `loadFile` calls `request({path}, path, name, User)`.
- [ ] 3.4 `PluginProcessor.cpp` `onLoadResult`: after a User load write `samplePath`, `sampleName`, and `sampleAnchor`/`sampleRel` from `makeRef`; remove the anchor pair when no match; never rewrite on Restore.
- [ ] 3.5 `PluginProcessor.cpp` `setStateInformation`: `restoreRef` from the four properties, `candidates` with anchors and search dirs, `request(list, absolute, name, Restore)`.
- [ ] 3.6 Build `GrainsPlugin_Standalone` and run the full suite; existing tests unchanged.

## Phase 4: Manual verification and spec sync (unit 2)

- [ ] 4.1 Manual (Standalone, Debug): anchored file writes four fields; file outside anchors writes two; old-format state (only `samplePath`) restores without "(relocated)".
- [ ] 4.2 Manual: move the sample's parent folder, restore; label `Loaded: <name> (relocated)`; missing file label `Missing: <name>` (stored name, else derived), state re-saved unchanged. Name search covers only files directly inside Music, Documents, Desktop or Home (non-recursive); a file in a nested subfolder not under the stored anchor path is NOT found.
- [ ] 4.3 Manual: stray `sampleName` with empty `samplePath` gives silence and "No sample"; relocated restore does not rewrite state; superseded restore does not publish (R-SS-7 gate checks); relocated plus truncated label shows both.
- [ ] 4.4 Sync deltas into `openspec/specs/sample-source/spec.md` and `openspec/specs/plugin-shell/spec.md` text at archive time; commit unit 2 as `feat(plugin): restore samples from relocated paths`.
