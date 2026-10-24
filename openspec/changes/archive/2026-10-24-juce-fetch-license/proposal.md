# Proposal: JUCE FetchContent fallback, plugin CI job, AGPLv3 license

## Intent

The plugin builds only where a JUCE checkout exists at `GRAINS_JUCE_DIR`, so CI never compiles it and plugin regressions go unnoticed. The repo also has no license. JUCE is dual-licensed AGPLv3/commercial, and we chose AGPLv3, so publishing under AGPLv3 is compatible with JUCE.

## Current state

- `CMakeLists.txt`: with `GRAINS_BUILD_PLUGIN=ON`, a missing `${GRAINS_JUCE_DIR}/CMakeLists.txt` causes `FATAL_ERROR`. Otherwise it runs `enable_language(C)`, `add_subdirectory(JUCE ... EXCLUDE_FROM_ALL)`, and defines the `grains_plugin` aggregate target. The project declares `VERSION 0.1.0`.
- `ci.yml`: has two jobs (Linux ASan+UBSan and Windows MSVC), both with the plugin OFF. Actions are pinned by SHA.
- Spec R-BH-3 says "No FetchContent JUCE fallback" and "no plugin CI job". R-BH-5 requires exactly two jobs.
- There is no LICENSE file, no SPDX tags and no license note in the README.

## Scope

### In Scope
- JUCE sourcing: local `GRAINS_JUCE_DIR` first. If it has no `CMakeLists.txt`, fall back to FetchContent of JUCE 9.0.0, pinned to commit `f8f8864172464b9adf9eba6101e1f784838d1597` with a tag comment.
- A new Windows MSVC CI job that builds `grains_plugin` through the fetch path.
- `LICENSE`: the full AGPLv3 text, downloaded verbatim from gnu.org at apply time.
- SPDX lines in first-party files, plus a README licensing and JUCE-compatibility note.

### Out of Scope
- Plugin identity codes, the soft limiter, and other roadmap items.
- A Linux plugin job, which would need ALSA, X11, freetype and webkit packages.
- Plugin tests in CI. The new job only builds the plugin; it runs no tests.
- Changes to the Projucer files.

## Capabilities

### New Capabilities
- `licensing`: the LICENSE file, SPDX policy, README note and JUCE compatibility statement.

### Modified Capabilities
- `build-harness`: R-BH-3 changes from fail-fast to local-then-fetch. The error now fires only when the fetch is disabled or fails. R-BH-5 grows from two jobs to three.

## Approach

- Use `FetchContent_Declare(juce URL .../archive/<sha>.tar.gz URL_HASH SHA256=<computed at apply>)`. `GIT_SHALLOW` cannot pin a SHA, and a full git clone is large.
- Download only, by setting `SOURCE_SUBDIR` to a non-existent path. Then reuse the existing `add_subdirectory(<src> ... EXCLUDE_FROM_ALL)` path, so the `enable_language(C)` and `grains_plugin` gotchas are untouched. This also avoids FetchContent's own `EXCLUDE_FROM_ALL`, which needs CMake 3.28 (the minimum here is 3.25).
- The CI job leaves `GRAINS_JUCE_DIR` at its default, which is absent on the runner. That exercises the fallback, which is the only automatic verification of the CMake logic. It caches the download keyed on the SHA. Timeout is about 45 minutes, covering configure-time juceaide plus the JUCE modules.
- License: AGPLv3. The SPDX identifier choice is still open (see the question round).

## Affected Areas

| Area | Impact | Description |
|------|--------|-------------|
| `CMakeLists.txt` | Modified | fetch fallback |
| `.github/workflows/ci.yml` | Modified | plugin job |
| `LICENSE` | New | AGPLv3 text, vendored |
| `README.md` | Modified | sourcing and license notes |
| `Source/`, `tests/`, `cmake/` (~54 files) | Modified | one SPDX line each |
| `openspec/specs/build-harness/spec.md` | Delta | R-BH-3, R-BH-5 |

## Risks

| Risk | Likelihood | Mitigation |
|------|------------|------------|
| GitHub archive checksum drifts | Low | On a hash mismatch, switch to `GIT_REPOSITORY` + `GIT_TAG <sha>` |
| CI time grows (tarball ~60-100 MB, cold build ~10-15 min) | Med | Cache, Debug build, single platform |
| Offline machines silently attempt a download | Low | Add a `GRAINS_JUCE_FETCH` option (default ON); OFF restores the hard error |
| License text gets typed from memory | Low | Download from gnu.org and record the SHA256 |

## Rollback Plan

Revert the PR. CMake returns to fail-fast and CI to two jobs. LICENSE is removed only on an explicit licensing decision.

## Dependencies

- Network access to github.com at configure time (fallback path only).
- gnu.org for the license text.

## Success Criteria

- [ ] The default build is unchanged and JUCE-free, and both existing CI jobs stay green.
- [ ] The plugin builds with a local JUCE and also through the fetch fallback.
- [ ] The Windows plugin job builds `grains_plugin` and passes.
- [ ] `LICENSE` byte-matches gnu.org `agpl-3.0.txt`.

## Proposal question round

Auto mode: these are assumptions for the user to review.
1. SPDX `AGPL-3.0-or-later` or `-only`? Assumed: `-or-later`.
2. Should SPDX lines go in all first-party files (~54 lines), or only in the README and LICENSE?
3. Fallback opt-out: is a `GRAINS_JUCE_FETCH` option (default ON) acceptable?
4. Should the plugin CI job be required or informational? Assumed: informational, matching R-BH-5.
