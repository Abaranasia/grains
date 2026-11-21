# Design: JUCE FetchContent fallback, plugin CI job, AGPLv3 license

## Technical Approach

The root `CMakeLists.txt` resolves JUCE in three steps: local `GRAINS_JUCE_DIR`, then a download-only FetchContent of the pinned tarball, then `FATAL_ERROR` (R-BH-3). Both sources go through the existing `add_subdirectory(<src> ${CMAKE_BINARY_DIR}/juce EXCLUDE_FROM_ALL)`, so the `enable_language(C)` and `grains_plugin` gotchas stay untouched. CI gets a third, informational Windows job that uses the fetch path (R-BH-5). Licensing adds a verbatim `LICENSE`, SPDX lines and a README section (R-LIC-1..3).

## Architecture Decisions

| Topic | Option | Tradeoff | Decision |
|---|---|---|---|
| Download-only on 3.25 | `FetchContent_Populate(juce)` | Deprecated by CMP0169 (3.30+), so it warns | Rejected |
| | `MakeAvailable` + `SOURCE_SUBDIR grains-download-only` (path does not exist) | Supported since 3.18. With no `CMakeLists.txt` there, it populates without calling `add_subdirectory` | **Chosen** |
| | FetchContent's own `EXCLUDE_FROM_ALL` | Needs 3.28 | Rejected |
| Source | `URL .../archive/<sha>.tar.gz` + `URL_HASH SHA256` | Exact bytes. Archive checksum may drift | **Chosen** |
| | `GIT_REPOSITORY` + `GIT_TAG <sha>` | Full clone | Fallback only if the hash drifts |
| Timestamps | No `DOWNLOAD_EXTRACT_TIMESTAMP` | CMP0135 is already NEW under `cmake_minimum_required(3.25)`, so extraction-time timestamps are the default | **Chosen** |
| Disconnected / overridden fetch | Guard after `MakeAvailable` | `FETCHCONTENT_FULLY_DISCONNECTED` or a bad `FETCHCONTENT_*` override can leave an empty source dir without an error | **Chosen** |
| Informational job | `continue-on-error: true` in `ci.yml` | One file, and R-BH-5 counts three jobs | **Chosen** |
| | Separate workflow | Contradicts the R-BH-5 wording | Rejected |
| CI cache | `actions/cache` on `build-plugin/_deps`, key `${{ runner.os }}-juce-<sha>-${{ hashFiles('CMakeLists.txt') }}` | A cache hit restores _deps (sources and stamps), so FetchContent skips both the download and the URL_HASH check. This is accepted because only successful runs save the cache, fork-PR caches are scoped to the PR ref, and the key includes the JUCE SHA and hashFiles('CMakeLists.txt'). A cache miss always does a verified download. | **Chosen** |
| Fetch failure | FetchContent aborts configure on a network or hash error | Nothing is extracted before verification | Plus the STATUS hint and the guard |

## Final CMake Block (root `CMakeLists.txt`, replaces lines 25-42)

```cmake
# Opt-in JUCE plugin (VST3 + Standalone). OFF by default: the default build stays CXX-only and
# JUCE-free. Use a separate build directory (e.g. build-plugin) when turning this on.
option(GRAINS_BUILD_PLUGIN "Build the JUCE VST3 + Standalone plugin (local JUCE or pinned fetch)" OFF)
set(GRAINS_JUCE_DIR "${CMAKE_SOURCE_DIR}/../../Projucer" CACHE PATH
  "Path to a JUCE checkout containing CMakeLists.txt (used when GRAINS_BUILD_PLUGIN=ON)")
option(GRAINS_JUCE_FETCH "Download pinned JUCE 9.0.0 when GRAINS_JUCE_DIR has no JUCE" ON)
if(GRAINS_BUILD_PLUGIN)
  if(EXISTS "${GRAINS_JUCE_DIR}/CMakeLists.txt")
    set(_grains_juce_src "${GRAINS_JUCE_DIR}")
    message(STATUS "Grains: using local JUCE at '${GRAINS_JUCE_DIR}'")
  elseif(GRAINS_JUCE_FETCH)
    message(STATUS "Grains: no JUCE at '${GRAINS_JUCE_DIR}'; fetching JUCE 9.0.0 "
      "(f8f8864172464b9adf9eba6101e1f784838d1597). On failure set -DGRAINS_JUCE_DIR=<JUCE checkout> "
      "or -DGRAINS_JUCE_FETCH=OFF.")
    include(FetchContent)
    FetchContent_Declare(juce
      # JUCE tag 9.0.0
      URL https://github.com/juce-framework/JUCE/archive/f8f8864172464b9adf9eba6101e1f784838d1597.tar.gz
      URL_HASH SHA256=<computed at apply>
      SOURCE_SUBDIR grains-download-only) # no CMakeLists.txt there: download only, no add_subdirectory
    FetchContent_MakeAvailable(juce)
    if(NOT EXISTS "${juce_SOURCE_DIR}/CMakeLists.txt")
      message(FATAL_ERROR "Grains: fetched JUCE not available at '${juce_SOURCE_DIR}' (offline, "
        "FETCHCONTENT_FULLY_DISCONNECTED, or FETCHCONTENT_* override?). "
        "Set -DGRAINS_JUCE_DIR=<JUCE checkout> or -DGRAINS_JUCE_FETCH=OFF.")
    endif()
    set(_grains_juce_src "${juce_SOURCE_DIR}")
  else()
    message(FATAL_ERROR
      "GRAINS_JUCE_DIR='${GRAINS_JUCE_DIR}' does not contain a JUCE CMakeLists.txt and "
      "GRAINS_JUCE_FETCH=OFF. Set -DGRAINS_JUCE_DIR=<path to a JUCE checkout>, "
      "or set GRAINS_JUCE_FETCH=ON, or leave GRAINS_BUILD_PLUGIN=OFF.")
  endif()
  enable_language(C) # JUCE's CMake requires the C language to be enabled
  add_subdirectory("${_grains_juce_src}" "${CMAKE_BINARY_DIR}/juce" EXCLUDE_FROM_ALL)
  add_subdirectory(Source/plugin EXCLUDE_FROM_ALL)
  # Not part of `all`. Also makes the excluded plugin targets visible to Visual Studio solutions,
  # which otherwise drop them and break `cmake --build --target GrainsPlugin_VST3`.
  add_custom_target(grains_plugin DEPENDS GrainsPlugin_VST3 GrainsPlugin_Standalone)
endif()
```

To get the hash, download the tarball into an empty scratch directory, run `Get-FileHash -Algorithm SHA256` and record the result in apply-progress. Then confirm it with G1. Never invent the value.

## CI Job

- Job name: "Windows MSVC plugin (informational)". `windows-latest`, `timeout-minutes: 45`, `continue-on-error: true`.
- `actions/checkout@fbc6f3992d24b796d5a048ff273f7fcc4a7b6c09 # v5.1.0`.
- `actions/cache@<40-char SHA> # vX.Y.Z`. Resolve the SHA at apply with `git ls-remote`. Never invent it.
- `cmake -S . -B build-plugin -A x64 -DGRAINS_BUILD_PLUGIN=ON`. `GRAINS_JUCE_DIR` stays at its default, which is absent on the runner.
- `cmake --build build-plugin --config Debug --target grains_plugin --parallel`. No tests.

## Licensing

- `LICENSE`: download `https://www.gnu.org/licenses/agpl-3.0.txt` straight into place with `curl -o` or `Invoke-WebRequest -OutFile`, never through an editor, and record its SHA256 in apply-progress. `.gitattributes` sets `LICENSE -text` so EOL conversion cannot alter it. R-LIC-1 hashes the committed blob (`git show HEAD:LICENSE`).
- W7 gate at apply: read `LICENSE.md` in the fetched JUCE 9.0.0 tarball and confirm it still offers AGPLv3. Also check the license of the bundled VST3 SDK. If AGPLv3 is no longer offered, STOP and report.
- README wording: "Grains is AGPL-3.0-or-later; JUCE is used under its AGPLv3 option; distributed plugin binaries are therefore under AGPLv3." Also note that `FETCHCONTENT_SOURCE_DIR_JUCE` bypasses the hash check, so users should prefer `GRAINS_JUCE_DIR`.
- SPDX goes on line 1:
  - `// SPDX-License-Identifier: AGPL-3.0-or-later` in C++ files.
  - `# ...` in CMake files.
  - `<!-- SPDX-License-Identifier: AGPL-3.0-or-later -->` in `README.md`.

  That is 56 files: the 54 files under `Source/`, `tests/` (including `support/`) and `cmake/`, plus the root `CMakeLists.txt` and `README.md`. Keep each file's existing line endings. Excluded: `LICENSE`, `.gitattributes`, `ci.yml`, `Builds/`, `JuceLibraryCode/`, `Grains.jucer`, `openspec/`, fetched sources.
- Stale text to fix:
  - README L13-21: the plugin now builds in CI through the fetch path. Document the sourcing order and `GRAINS_JUCE_FETCH=OFF`.
  - `CMakeLists.txt:27`: the option description (shown in the block above).
  - `Source/plugin/CMakeLists.txt:2`: "JUCE comes from the root CMakeLists (local GRAINS_JUCE_DIR or pinned fetch)."

## File Changes

| File | Action | Est. lines |
|---|---|---|
| `CMakeLists.txt` | Modify | ~+35/-10 |
| `.github/workflows/ci.yml` | Modify (job 3) | ~+25 |
| `README.md` | Modify | ~+30/-4 |
| `Source/plugin/CMakeLists.txt` | Modify comment | ~1 |
| `.gitattributes` | Create | 1 |
| `LICENSE` | Create (verbatim) | ~661 (vendored) |
| 54 other first-party files | SPDX line | +54 |
| `openspec/changes/juce-fetch-license/**` | Docs | ~300 |

## Commit Plan

1. `build: add pinned JUCE FetchContent fallback and plugin CI job`. Covers the CMake files, `ci.yml`, README sourcing and stale text, and openspec.
2. `chore: license under AGPL-3.0-or-later`. Covers `LICENSE`, `.gitattributes`, SPDX and the README licensing section.

Authored lines come to about 150 code/config plus about 300 docs. `LICENSE` is vendored text proven by its hash, so it counts as non-authored. Counting it would put the PR at about 1110 lines, over the 800 budget.

## Testing Strategy (configure-level RED/GREEN; unit-level Strict TDD does not apply)

| # | Check | Expected |
|---|---|---|
| RED | Before the change: `-DGRAINS_BUILD_PLUGIN=ON -DGRAINS_JUCE_DIR=<empty dir>` | FATAL_ERROR |
| G1 | Same after the change, fetch ON | "fetching" STATUS, hash verified, `grains_plugin` builds |
| G2 | Same with `-DGRAINS_JUCE_FETCH=OFF` | W5 message, no `_deps/` |
| G3 | Fresh build dir, wrong `URL_HASH` (temporary edit, reverted) | Configure fails |
| G4 | Local JUCE | "local" STATUS, no `_deps/` |
| G5 | Default OFF, `-Werror=dev -Werror=deprecated` | No download, ctest 122/122 |
| G6 | Fetch configure with `-Werror=dev -Werror=deprecated` | No Grains-originated warnings |
| G7 | SPDX: default `/WX` build + G1 + `rg -c` over the 56 files | Green, exactly 1 per file |
| G8 | Fresh dir, empty `GRAINS_JUCE_DIR`, `-DFETCHCONTENT_FULLY_DISCONNECTED=ON` | Hits the "fetched JUCE not available" guard |
| CI | Job 3 | Green |

## Threat Matrix

N/A. There is no routing, VCS/PR automation or executable-file classification. The download is pinned by SHA with `URL_HASH`, the action is pinned by SHA, and permissions stay at `contents: read`.

## Migration / Rollout

No migration is needed. Offline users without a local JUCE set `-DGRAINS_JUCE_FETCH=OFF`.

## Open Questions

- [ ] If JUCE 9.0.0's own CMake emits dev warnings, G6 is scoped to Grains-originated warnings only.
