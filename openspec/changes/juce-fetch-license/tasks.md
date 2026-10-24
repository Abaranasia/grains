# Tasks: juce-fetch-license

Specs: `specs/build-harness` (R-BH-3, R-BH-5), `specs/licensing` (R-LIC-1, R-LIC-2, R-LIC-3). Design: `design.md`. Strict TDD applies as configure-level RED/GREEN checks (CMake/CI); there are no unit tests.

Conventions:
- Repo root: `H:/Proyectos/Juce/Plugins/Grains`. Scratch dir for downloads: an empty, new directory outside the repo; never run tools from inside it.
- Core build dir: `build/apply-jf` (plugin OFF). Plugin build dir: `build-plugin-jf`. Fresh dirs for each G check.
- Core test: `ctest --test-dir build/apply-jf -C Debug --output-on-failure` (baseline 122/122).
- Hash and SHA values are never invented: compute or resolve them at apply and record them in apply-progress.
- "commit (orchestrator)" steps are performed by the orchestrator, not the apply executor.

## Review Workload Forecast

| Field | Value |
|-------|-------|
| Authored changed lines | ~467 (commit 1 ~395, commit 2 ~72) |
| Vendored, not authored | `LICENSE` ~661 lines, verified by SHA256 (not counted) |
| Review budget | 800 lines; authored forecast is about 58% of budget |
| 400-line budget risk | Medium (commit 1 is ~395 incl. ~300 of openspec docs) |
| Counting LICENSE | ~1128 lines, over budget; hence treated as vendored text proven by hash |
| Chained PRs recommended | No |
| Delivery strategy | single-pr, 2 commits |

Decision needed before apply: No.

| Commit | Contents | Authored lines |
|---|---|---|
| 1 `build:` | CMakeLists.txt ~+35/-10 (~45), ci.yml ~25, README sourcing/stale text ~25, Source/plugin/CMakeLists.txt 1, openspec folder ~300 | ~395 |
| 2 `chore:` | .gitattributes 1, SPDX 56 (54 files + CMakeLists.txt + README.md), README licensing section ~15 | ~72 |
| (vendored) | LICENSE | ~661 |

## Commit 1: `build: add pinned JUCE FetchContent fallback and plugin CI job`

- [x] 1.1 Create scratch dirs and a core build dir. Run RED (before any edit): `cmake -S . -B build-red -DGRAINS_BUILD_PLUGIN=ON -DGRAINS_JUCE_DIR=<new empty dir>`. Expect FATAL_ERROR (no fetch fallback yet). Record the message in apply-progress. (R-BH-3)
- [x] 1.2 Download `https://github.com/juce-framework/JUCE/archive/f8f8864172464b9adf9eba6101e1f784838d1597.tar.gz` into a new empty scratch dir and compute `Get-FileHash -Algorithm SHA256`. Record the digest in apply-progress. Never invent it. (R-BH-3)
- [x] 1.3 Resolve the `actions/cache` 40-char commit SHA and its version tag with `git ls-remote https://github.com/actions/cache.git` (tag deref `^{}` for annotated tags). Record SHA and tag. Never invent it. (R-BH-5)
- [x] 1.4 Replace `CMakeLists.txt` lines 25-42 with the design's Final CMake Block: `GRAINS_JUCE_FETCH` option (default ON), local-first resolution, `FetchContent_Declare(juce URL ... URL_HASH SHA256=<task 1.2> SOURCE_SUBDIR grains-download-only)`, `FetchContent_MakeAvailable`, the `juce_SOURCE_DIR/CMakeLists.txt` guard, `FATAL_ERROR` for fetch OFF, then `enable_language(C)`, `add_subdirectory(<src> ${CMAKE_BINARY_DIR}/juce EXCLUDE_FROM_ALL)` and the unchanged plugin lines. Includes the line-27 option description fix. (R-BH-3)
- [x] 1.5 Update `Source/plugin/CMakeLists.txt` line 2 comment to: "JUCE comes from the root CMakeLists (local GRAINS_JUCE_DIR or pinned fetch)." (R-BH-3, stale text)
- [x] 1.6 Add job 3 to `.github/workflows/ci.yml`: name "Windows MSVC plugin (informational)", `windows-latest`, `timeout-minutes: 45`, `continue-on-error: true`; `actions/checkout@fbc6f3992d24b796d5a048ff273f7fcc4a7b6c09 # v5.1.0`; `actions/cache@<task 1.3 SHA> # <tag>` on `build-plugin/_deps` with key `${{ runner.os }}-juce-f8f8864172464b9adf9eba6101e1f784838d1597-${{ hashFiles('CMakeLists.txt') }}`; steps `cmake -S . -B build-plugin -A x64 -DGRAINS_BUILD_PLUGIN=ON` and `cmake --build build-plugin --config Debug --target grains_plugin --parallel`; no tests. Jobs 1 and 2 untouched; `permissions: contents: read` kept. (R-BH-5)
- [x] 1.7 Fix README L13-21 stale text: plugin now builds in CI via the fetch path; document the sourcing order (local `GRAINS_JUCE_DIR`, then pinned fetch), `GRAINS_JUCE_FETCH=OFF`, and that `FETCHCONTENT_SOURCE_DIR_JUCE` bypasses the hash check (prefer `GRAINS_JUCE_DIR`). (R-LIC-3 sourcing scenario, R-BH-3)
- [x] 1.8 GREEN G1: fresh `-B build-plugin-jf -DGRAINS_BUILD_PLUGIN=ON -DGRAINS_JUCE_DIR=<new empty dir>`; expect the "fetching" STATUS and hash verified; then `cmake --build build-plugin-jf --config Debug --target grains_plugin --parallel` succeeds. (R-BH-3, R-BH-5)
- [x] 1.9 G2: fresh dir, same plus `-DGRAINS_JUCE_FETCH=OFF`; expect the FATAL_ERROR naming the path and `GRAINS_JUCE_DIR`, and no `_deps/` directory. (R-BH-3)
- [x] 1.10 G3: fresh dir, temporarily alter one hex digit of `URL_HASH`, configure with the G1 command; expect configure failure on hash mismatch. Revert the edit and confirm with `git diff` that the correct hash is restored. (R-BH-3)
- [x] 1.11 G4: fresh dir with `-DGRAINS_JUCE_DIR=<local JUCE checkout>` (the Projucer default if present, otherwise the extracted tarball from task 1.2); expect the "using local JUCE" STATUS and no `_deps/`. (R-BH-3)
- [x] 1.12 G5: default OFF: `cmake -S . -B build/apply-jf -Werror=dev -Werror=deprecated`, build Debug, `ctest --test-dir build/apply-jf -C Debug --output-on-failure`; expect no download and 122/122. (R-BH-1, R-BH-3)
- [x] 1.13 G6: fetch configure as in G1 in a fresh dir with `-Werror=dev -Werror=deprecated`; expect no Grains-originated warnings (JUCE-originated ones are out of scope per the open question). (R-BH-3)
- [x] 1.14 G8: fresh dir, empty `GRAINS_JUCE_DIR`, `-DFETCHCONTENT_FULLY_DISCONNECTED=ON`; expect the "fetched JUCE not available" guard FATAL_ERROR. (R-BH-3)
- [x] 1.15 Verify untouched paths: `git diff --stat -- Grains.jucer Builds JuceLibraryCode` is empty. (R-BH-3 Projucer scenario)
- [x] 1.16 Verify the CI file by inspection: three jobs, every `uses:` pinned to a 40-char SHA, `permissions: contents: read`, concurrency cancel group intact, jobs 1 and 2 keep `GRAINS_BUILD_PLUGIN` OFF. (R-BH-5)
- [ ] 1.17 Commit (orchestrator): `build: add pinned JUCE FetchContent fallback and plugin CI job` covering CMakeLists.txt, Source/plugin/CMakeLists.txt, ci.yml, README, and `openspec/changes/juce-fetch-license/`.

## Commit 2: `chore: license under AGPL-3.0-or-later`

- [ ] 2.1 W7 STOP gate (before any licensing work): from the extracted JUCE 9.0.0 tarball (task 1.2), read `LICENSE.md` and confirm it still offers AGPLv3; also inspect the license of the bundled VST3 SDK. If AGPLv3 is no longer offered or the VST3 SDK license is incompatible, STOP and report to the orchestrator without touching any licensing file. Record the findings in apply-progress. (R-LIC-3)
- [ ] 2.2 Download `https://www.gnu.org/licenses/agpl-3.0.txt` straight to `LICENSE` at the repo root with `curl -o` or `Invoke-WebRequest -OutFile` (never via an editor). Compute SHA256 and record it in apply-progress. (R-LIC-1)
- [ ] 2.3 Create `.gitattributes` with the single line `LICENSE -text`. (R-LIC-1)
- [ ] 2.4 Verify R-LIC-1: after staging, `git show :LICENSE` hashed with SHA256 equals the recorded digest, and equals a second fresh download hashed in a new scratch dir. (R-LIC-1)
- [ ] 2.5 Add the SPDX line as line 1 of the 56 first-party files, preserving each file's existing line endings (detect CRLF vs LF per file; do not normalize): `// SPDX-License-Identifier: AGPL-3.0-or-later` for C++ files under `Source/` and `tests/` (including `tests/support/`), `# SPDX-License-Identifier: AGPL-3.0-or-later` for CMake files (`cmake/`, `tests/CMakeLists.txt`, `Source/**/CMakeLists.txt`, root `CMakeLists.txt`), and `<!-- SPDX-License-Identifier: AGPL-3.0-or-later -->` for `README.md`. Exclude `LICENSE`, `.gitattributes`, `ci.yml`, `Builds/`, `JuceLibraryCode/`, `Grains.jucer`, `openspec/`. (R-LIC-2)
- [ ] 2.6 Add the README licensing section: AGPL-3.0-or-later, link to `LICENSE`, "JUCE is dual-licensed (AGPLv3 / commercial); Grains uses JUCE under its AGPLv3 option; distributed plugin binaries are therefore under AGPLv3." Confirm the sourcing text from task 1.7 is present. (R-LIC-3)
- [ ] 2.7 G7 count: `rg -c "SPDX-License-Identifier: AGPL-3.0-or-later"` over the 56 files shows exactly 1 per file and exactly 56 files; `rg -l` over the exclusion list returns nothing. (R-LIC-2)
- [ ] 2.8 G7 line endings: `git diff --numstat` shows `1 0` per SPDX file (README: more lines from tasks 1.7/2.6 are expected only in README); `git diff --stat` shows no whole-file rewrites. (R-LIC-2)
- [ ] 2.9 G7 build: fresh default (OFF) build with `/WX` as configured by the project, `ctest` 122/122 green; then re-run G1 (fetch configure and `grains_plugin` build) green with SPDX lines present. (R-LIC-2 build scenario)
- [ ] 2.10 Verify `git diff --stat -- Grains.jucer Builds JuceLibraryCode` is empty. (R-LIC-2)
- [ ] 2.11 Commit (orchestrator): `chore: license under AGPL-3.0-or-later` covering LICENSE, .gitattributes, the 56 SPDX edits and README licensing section.

## Post-push

- [ ] 3.1 Requires the user's push: when the first CI run is observed, confirm job 3 "Windows MSVC plugin (informational)" ran through the fetch path and `grains_plugin` built (informational; a failure does not block merge), and that jobs 1 and 2 are unaffected. (R-BH-5)

## Requirement Traceability

| Requirement | Tasks |
|---|---|
| R-BH-3 (fetch fallback, guard, isolation) | 1.1, 1.2, 1.4, 1.5, 1.7, 1.8, 1.9, 1.10, 1.11, 1.12, 1.13, 1.14, 1.15 |
| R-BH-5 (plugin CI job, pinning, timeout, cache) | 1.3, 1.6, 1.8, 1.16, 3.1 |
| R-LIC-1 (verbatim LICENSE) | 2.2, 2.3, 2.4 |
| R-LIC-2 (SPDX in 56 files) | 2.5, 2.7, 2.8, 2.9, 2.10 |
| R-LIC-3 (README licensing and sourcing) | 1.7, 2.1, 2.6 |

## Parallelism

- Sequential: 1.1 -> 1.2 -> 1.4 (hash needed) ; 1.3 -> 1.6 ; 1.4 -> 1.8..1.14 ; 2.1 (gate) -> everything in commit 2 ; commit 1 -> commit 2.
- Parallel after 1.2/1.3: 1.5 and 1.7 (independent files); 1.9, 1.11, 1.12, 1.14 (independent fresh dirs, but each is slow; run in any order).
- Parallel within commit 2 after 2.1: 2.2/2.3 and 2.5/2.6 touch disjoint files; 2.7 onward is sequential.
