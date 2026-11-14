# Exploration: sample-relative-path

Source: roadmap item P2 and the "Privacy" follow-up in `openspec/changes/archive/2026-11-14-loader-hardening/archive-report.md`. Engram mirror: `sdd/sample-relative-path/explore` (obs #622).

## Current state

- `PluginProcessor.cpp` keeps one APVTS property, `samplePath` (line 10). It is written only in `onLoadResult` after a successful `User` load (lines 110-111).
- `setStateInformation` (line 102) calls `loader_.request(path, Origin::Restore)`.
- `SampleLoader::load` treats a non-absolute path as missing, so any relative path currently fails.
- A failed Restore clears the source and posts `"Missing: <name>"`. A failed User load keeps the old source and posts `"Error: <name>"`.
- The editor shows only the `status()` label. The `FileChooser` starts at `juce::File{}` (no last-used directory).
- `Result.path` echoes the requested path, not the resolved one.
- Specs to change: `sample-source` R-SS-8 (line 153: "Absolute paths are used; relative-path lookup and embedded audio are not provided") and `plugin-shell` lines 95 and 111.
- `grains_core` is a static C++17 library whose JUCE-freedom is enforced in CMake. Each unit has a header and a source file. Tests are Catch2 files listed by hand in `tests/CMakeLists.txt`.

## What JUCE offers

No generic host project-directory API is known in JUCE (to be confirmed against the pinned JUCE 9 tree during design). Usable pieces: `File::getSpecialLocation` (Music, Documents, Desktop, Home), `getRelativePathFrom`, `isAbsolutePath`. The only portable signals are the file name, a path relative to a well-known user folder, and a bounded list of search directories.

## Approaches

1. **(a) Absolute path plus a path relative to a user folder.** Small state; survives a different user name or machine when the folder layout matches. Fails for samples outside those folders. Effort: Low to Medium.
2. **(b) Absolute path plus file name, searching candidate directories on restore.** Legacy states benefit (the name can be derived from the old absolute path). Risk: a same-named file can load silently; the search must never recurse. Effort: Medium.
3. **(c) Pure core resolver with an injected `exists` predicate.** The structure that delivers (a) and (b); all path logic becomes unit-testable under strict TDD. Cost: a new core unit with a small `std::string` path library. Effort: Medium.
4. **Embedded audio in state.** Out of scope: solves relocation and privacy fully but costs state size. Stays on the roadmap.

## Recommendation

(c) delivering (a) plus a bounded form of (b), in one PR.

- **State (additive):** keep `samplePath`; add `sampleName`, and `sampleAnchor` + `sampleRel` when the file lies under an anchor.
- **Core `grains::SamplePathResolver`** (`std::string` only, no JUCE, no `std::filesystem`):
  - `makeRef(absolute, anchors)`: longest matching anchor; case-insensitive and drive/UNC-aware for Windows-shaped paths.
  - `candidates(ref, anchors, searchDirs)`: ordered, deduplicated: (1) anchor + relative path on this machine, (2) stored absolute path, (3) the name in each search directory (non-recursive).
  - `resolve(candidates, exists)`: first candidate for which the injected predicate is true.
  - Windows vs POSIX is detected from the path's shape, not the host OS; the name is split on both `\` and `/`.
- **Plugin:** `SampleLoader` takes a candidate list and loads the first existing file; missing-file behaviour unchanged. `Result.path` reports the path actually loaded; a relocated Restore shows `"Loaded: <name> (relocated)"`.

Estimated size: core ~220, tests ~280, plugin ~120, specs ~80, CMake ~5 lines (~700 total).

## Core versus plugin

- **Core (Catch2):** reference derivation, anchor matching, separator/drive/UNC handling, name extraction, candidate ordering and dedup, `resolve` with a fake predicate, legacy fallback, empty or garbage input.
- **Plugin (manual, JUCE):** APVTS read/write, anchor lookup, `existsAsFile`, loader candidate iteration, status text. Manual checks: save, move the folder or switch account, reload; load an old-format state; confirm the missing-file label.

## Risks

- A name-based search can load a different file with the same name (mitigated by strict ordering, no recursion, the "(relocated)" hint; no content hash in this change).
- Privacy is partial if `samplePath` keeps being written.
- Cross-OS path handling is easy to get wrong (a backslash is a legal filename character on POSIX).
- `SampleLoader` generation-gate and locking invariants must be preserved; only the payload shape changes.
- Probing candidates on the loader thread can stall on network shares; keep the list small (about 10) and probe sequentially.
- Anchor locations may differ under macOS sandboxing (untested; development is on Windows).

## Open questions and orchestrator decisions (user away, autonomous mode)

1. Omit `samplePath` when an anchor matches? **Decision: keep writing it** (backward and forward compatible, smaller change). Dropping it for privacy is recorded as a follow-up.
2. Anchors: **Music, Documents, Desktop, Home**; longest prefix wins. Downloads deferred (no JUCE constant).
3. File size or hash check: **deferred**.
4. Persisting a last-used directory: **deferred**.
5. Rewriting state after a relocated Restore: **deferred**; the stored fields are refreshed on the next successful User load as today.
6. JUCE project-directory API: confirm during design.
7. Spec shape: **modify R-SS-8 and add requirements** for resolver ordering and legacy compatibility.
