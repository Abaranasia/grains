# Tasks: loader-hardening

Specs: `specs/sample-source` (R-SS-5, R-SS-7, R-SS-9), `specs/build-harness` (R-BH-3 modified, R-BH-5 modified). Design: `design.md`. Strict TDD applies where a deterministic RED exists (S-A); S2 is characterization; comments, CMake and CI items use verify-by-command checks.

Conventions:
- Repo root: `H:/Proyectos/Juce/Plugins/Grains`. Core build dir: `build` (plugin OFF). Plugin build dir: `build-plugin` (plugin ON).
- Core: `cmake -S . -B build -A x64 -DGRAINS_BUILD_PLUGIN=OFF`; `cmake --build build --config Debug`; `ctest --test-dir build -C Debug --output-on-failure`. Baseline 122/122.
- Plugin: `cmake -S . -B build-plugin -A x64 -DGRAINS_BUILD_PLUGIN=ON`; `cmake --build build-plugin --config Debug --target grains_plugin --parallel`.
- Preserve each file's EOL. CRLF: `CMakeLists.txt`, `SampleLoader.cpp`, `SampleBuilder.h`, `SourceExchange.h`, `test_sample_builder.cpp`, `test_engine_realtime.cpp`. LF: `test_generation_gate.cpp`, `ci.yml`, `cmake/*.cmake` (including new `JucePin.cmake`). After each edit confirm with `git diff --numstat` that there are no whole-file rewrites.
- Mutations used for RED are local and one-off; never committed.
- "commit (orchestrator)" steps are performed by the orchestrator, not the apply executor.
- Hash and SHA values come from the design (already resolved); do not retype them from memory, copy from `design.md`.

## Review Workload Forecast

| Field | Value |
|-------|-------|
| Authored changed lines (code, tests, comments, CI) | ~100 (WU1 ~25, WU2 ~40, WU3 ~7, WU4 ~25) |
| Openspec docs (proposal, specs, design, tasks) | ~250-300, committed with WU1 |
| Total authored incl. docs | ~350-400 |
| Review budget | 800 lines; forecast about 50% of budget |
| 400-line budget risk | Low-Medium (code is ~100; the total approaches 400 only because of openspec docs) |
| Chained PRs recommended | No |
| Delivery strategy | single-pr, 4 work-unit commits (each independently revertable) |

Decision needed before apply: No.

| WU | Commit | Authored lines |
|---|---|---|
| 1 | `fix(plugin): wait for loader thread at shutdown` | ~25 (+ openspec folder) |
| 2 | `test: harden stress, frees and cap tests` | ~40 |
| 3 | `docs(core): document cap rationale and collect rule` | ~7 |
| 4 | `build: single JUCE pin file and CI cache key` | ~25 |

## WU1: `fix(plugin): wait for loader thread at shutdown`

Rollback boundary: `Source/plugin/SampleLoader.cpp`, `Source/plugin/SampleLoader.h` (and the openspec folder if desired). Runtime harness: N/A for automated runs (stalled-share behaviour is [Manual]).

- [x] 1.1 Baseline: configure and build core, run ctest; record 122/122. (R-SS-7)
- [x] 1.2 Inspect `SampleLoader.cpp:31-37` and confirm via `rg "stopThread|stop\(\)" Source` that `stop()` has only the two callers named in the design (`~GrainsAudioProcessor`, `~SampleLoader`). Record the result. If any other caller exists, STOP and report. (R-SS-7)
- [x] 1.3 Edit `SampleLoader::stop()` to the design body: keep `alive_->store(false)`, `signalThreadShouldExit()`, `notify()`, replace the finite-timeout `stopThread` with `stopThread(-1)`, and add the 4-line explanatory comment. Keep CRLF. (R-SS-7)
- [x] 1.4 Update the `SampleLoader.h:38` comment to: `// Message thread only. Idempotent; blocks until the loader thread exits. Call before the exchange is destroyed.` Keep CRLF. (R-SS-7)
- [x] 1.5 Verify the finite timeout is gone: `rg "stopThread\(" Source/plugin` shows only `stopThread(-1)`. (R-SS-7)
- [x] 1.6 W-A is spec only (R-SS-9): re-read `SampleLoader.cpp` L77-79 and L94-96 and confirm the code matches the spec labels ("Error: No sample" / "Error: <name>", previous source kept). No code change. (R-SS-9)
- [x] 1.7 Build core (OFF) and run ctest, expect 122/122; build plugin (ON) target `grains_plugin`, expect success. (R-SS-7)
- [ ] 1.8 [Manual] W2 stalled-read shutdown (user-performed, may remain pending): load a sample from a stalled or slow network share, close the host mid-read; expect shutdown to wait for the loader and no "killing thread by force" message in the JUCE debug output. (R-SS-7)
- [ ] 1.9 [Manual] W-A failed clear (user-performed, may remain pending): trigger a clear whose allocation fails (or simulate); expect the previous source to remain and the label "Error: No sample" (or "Error: <name>" when loading a named file). (R-SS-9)
- [x] 1.10 Commit (orchestrator) 3f8f65b: `fix(plugin): wait for the loader thread at shutdown instead of killing it` covering `SampleLoader.cpp`, `SampleLoader.h`, and `openspec/changes/loader-hardening/`.

## WU2: `test: harden stress, frees and cap tests`

Rollback boundary: `tests/test_generation_gate.cpp`, `tests/test_engine_realtime.cpp`, `tests/test_sample_builder.cpp`. Runtime harness: ctest focused filters below.

- [x] 2.1 S-A RED (mutation): in `tests/test_generation_gate.cpp` locally change the committer call to `commitIf(gen + 1, ...)`; build and run `ctest --test-dir build -C Debug -R <stress test name> --output-on-failure`. Without the production change in place the current test passes vacuously; the goal here is that after task 2.2 the mutated test fails with `commits == 0` within ~10 s and does not hang. Record the result. (R-SS-5 sibling, R-SS-11 stress)
- [x] 2.2 S-A GREEN: implement the design change in `test_generation_gate.cpp` L60-82: new comment text, `std::atomic<int> commits{0}`, `commits.fetch_add(1)` inside the successful commit, 10 s deadline yield-wait before the bump loop, `done = true`, `committer.join()`, then `CHECK(commits.load() > 0)` as the first assertion after join. Never `REQUIRE` before join. Keep LF. Re-apply the task 2.1 mutation, confirm it fails with `commits == 0` (RED proof), then revert the mutation and confirm `git diff` has no `gen + 1`. (R-SS-11)
- [x] 2.3 S-A verify GREEN: rebuild, run the stress test several times (e.g. `ctest ... -R <name> --repeat until-fail:5`), expect pass. (R-SS-11)
- [x] 2.4 S1: in the anonymous namespace of `tests/test_engine_realtime.cpp` add `int* volatile g_sink` and `int* escape(int*) noexcept` with the design comment; change the frees() test to `auto* outside = escape(new int(1));` and `auto* q = escape(new int(2));`. Keep the null-delete line. Keep CRLF. No deterministic RED exists (optimizer-dependent); optionally run the test on a Release build and record. (test hardening)
- [x] 2.5 S2 (characterization test, expected to pass immediately, not RED): add the design's `builder: maxSourceFrames floors, rejects sub-frame caps and clamps to INT_MAX` test case to `tests/test_sample_builder.cpp` with the five `maxSourceFrames` checks (10.0013 -> 3000, 0.003 -> 0, 1e8 -> INT_MAX, DBL_MAX -> INT_MAX) and `buildSampleBuffer(..., 0.003)` returning `nullptr` + `BuildError::BadSampleRate`. Record that it passed on first run. Keep CRLF. (R-SS-5)
- [x] 2.6 S4: remove `#include <cmath>` at `test_sample_builder.cpp` L6 after confirming with `rg "std::(floor|ceil|round|sqrt|abs|sin|cos|pow|isnan|isinf)|fabs|floor\(" tests/test_sample_builder.cpp` that nothing else needs it (if something does, STOP and report). Build to confirm. (cleanup)
- [x] 2.7 Full core build (plugin OFF) and ctest: expect baseline plus 1 new test case (123 tests if counted per case; record the exact count). (R-SS-5)
- [x] 2.8 (N/A: no sanitizer configuration on MSVC; DBL_MAX path takes the clamp branch by design) Optional: UBSan check of the S2 case if a sanitizer configuration is available (DBL_MAX path must be clean because it takes the clamp branch). Record or mark N/A. (R-SS-5)
- [x] 2.9 Commit (orchestrator) 415a457: `test: harden the gate stress, frees and source cap tests`.

## WU3: `docs(core): document cap rationale and collect rule`

Rollback boundary: comment-only edits in `Source/core/include/grains/SourceExchange.h` and `SampleBuilder.h`. Runtime harness: N/A (comments only); build must still succeed.

- [x] 3.1 S3: in `SourceExchange.h` after L15 add the 2-line comment naming `SampleLoader::commitGated` (design text). Keep CRLF. (documentation)
- [x] 3.2 S5: in `SampleBuilder.h` above `kMaxSourceSeconds` (L11) add the 4-line comment (115 MB, about 2x peak, INT_MAX; `maxSourceFrames` clamps). Keep CRLF. (R-SS-5 rationale)
- [x] 3.3 Verify `git diff --numstat` shows only additions in both files and no whole-file rewrite; build core and run ctest, expect unchanged results from task 2.7. (documentation)
- [x] 3.4 Commit (orchestrator) 5a732ee: `docs(core): document the source cap and where collect is enforced`.

## WU4: `build: single JUCE pin file and CI cache key`

Rollback boundary: `cmake/JucePin.cmake`, `CMakeLists.txt` JUCE block, `.github/workflows/ci.yml` cache key. Runtime harness: configure-level checks below.

- [x] 4.1 RED: confirm before any edit that `rg "f8f88641" --glob "!openspec/**"` matches `CMakeLists.txt` and `.github/workflows/ci.yml` (literal SHA duplicated). Record the matches. (R-BH-3, R-BH-5)
- [x] 4.2 Create `cmake/JucePin.cmake` (LF) with the SPDX header, the explanatory comment, and `GRAINS_JUCE_VERSION`, `GRAINS_JUCE_SHA`, `GRAINS_JUCE_SHA256` copied exactly from `design.md` (values come from the existing `CMakeLists.txt` pin; before overwriting confirm the SHA and SHA256 equal the ones currently in `CMakeLists.txt`). (R-BH-3)
- [x] 4.3 Edit `CMakeLists.txt` (CRLF): add `include(JucePin)` before the `GRAINS_JUCE_FETCH` option; use `${GRAINS_JUCE_VERSION}` in the option description; use `${GRAINS_JUCE_VERSION}` and `${GRAINS_JUCE_SHA}` in the fetch STATUS message; `URL https://github.com/juce-framework/JUCE/archive/${GRAINS_JUCE_SHA}.tar.gz`; `URL_HASH SHA256=${GRAINS_JUCE_SHA256}`; remove the `# JUCE tag 9.0.0` comment. No other behaviour change. (R-BH-3)
- [x] 4.4 Edit `.github/workflows/ci.yml` (LF) line ~74: key becomes `${{ runner.os }}-juce-${{ hashFiles('cmake/JucePin.cmake') }}`. Nothing else in the file changes; jobs, pins and permissions untouched. (R-BH-5)
- [x] 4.5 GREEN single source: `rg "f8f88641" --glob "!openspec/**"` matches only `cmake/JucePin.cmake`; `rg "0bef95e4" --glob "!openspec/**"` likewise. (R-BH-3, R-BH-5)
- [x] 4.6 G1: fresh dir `-B build-plugin-lh -DGRAINS_BUILD_PLUGIN=ON -DGRAINS_JUCE_DIR=<new empty dir>`; expect the fetching STATUS showing version 9.0.0 and the SHA, hash verified, and `cmake --build build-plugin-lh --config Debug --target grains_plugin --parallel` succeeds. (R-BH-3)
- [x] 4.7 G3: fresh dir, temporarily alter one hex digit of `GRAINS_JUCE_SHA256` in `cmake/JucePin.cmake`, configure as in G1; expect a hash mismatch failure. Revert and confirm via `git diff` that the correct value is restored. (R-BH-3)
- [x] 4.8 G2/G4/G5: `-DGRAINS_JUCE_FETCH=OFF` with an empty JUCE dir gives the FATAL_ERROR naming the path and `GRAINS_JUCE_DIR` and no `_deps/`; a local `GRAINS_JUCE_DIR` prints "using local JUCE" with no `_deps/`; default core build (OFF) with `-Werror=dev -Werror=deprecated` configures, builds and passes ctest with no download. (R-BH-3)
- [x] 4.9 Verify the CI file by inspection: three jobs, every `uses:` still pinned to a 40-char SHA, `permissions: contents: read`, cache path `build-plugin/_deps` unchanged, only the key line differs in `git diff`. (R-BH-5)
- [x] 4.10 Verify untouched paths: `git diff --stat -- Grains.jucer Builds JuceLibraryCode` is empty. (R-BH-3)
- [ ] 4.11 [Manual] Single-source-of-truth scenario (user-performed, may remain pending): after push, change only a comment in `CMakeLists.txt` and confirm the CI cache key does not change; change the SHA in `cmake/JucePin.cmake` and confirm the key changes (one-time cache miss expected on the first run of this PR). (R-BH-5)
- [x] 4.12 Commit (orchestrator) aade368: `build: keep the JUCE pin in one file and key the CI cache on it`.

## Final verification

- [x] 5.1 From a fresh state: core configure, build Debug, `ctest --test-dir build -C Debug --output-on-failure`; expect all tests pass (baseline 122 plus the new S2 case). (all)
- [x] 5.2 Plugin configure with `-DGRAINS_BUILD_PLUGIN=ON` into `build-plugin` and build `grains_plugin` Debug; expect success. (R-SS-7, R-BH-3)
- [x] 5.3 (verified in sdd-verify: tree clean, four conventional commits, no attribution, no `gen + 1` leftover) `git status` clean apart from intended files; `git log --oneline -5` shows the four conventional commits with no AI attribution; no mutation leftovers (`rg "gen \+ 1" tests/test_generation_gate.cpp` empty). (all)

## Post-push

- [ ] 6.1 Push the branch and open the PR targeting `feat/juce-fetch-license` (until it merges, then retarget to `main`). Observe the first CI run: plugin job green (expected one-time cache miss from the new key), jobs 1 and 2 unchanged; a second run should show a cache hit. (R-BH-5)

## Requirement Traceability

| Requirement | Tasks |
|---|---|
| R-SS-5 (INT_MAX clamp, floor-to-zero) | 2.5, 2.7, 2.8, 3.2 |
| R-SS-7 (stop waits without finite timeout) | 1.2, 1.3, 1.4, 1.5, 1.7, 1.8 |
| R-SS-9 (failed clear keeps source, labels) | 1.6, 1.9 |
| R-SS-11 (stress test, supporting) | 2.1, 2.2, 2.3 |
| R-BH-3 (pin referenced from JucePin.cmake) | 4.1-4.3, 4.5-4.8, 4.10, 5.2 |
| R-BH-5 (single pin file, cache key, CI) | 4.1, 4.4, 4.5, 4.9, 4.11, 6.1 |

## Parallelism

- Sequential within a WU. WU1 to WU4 touch disjoint files and are independent; execute in order WU1 -> WU2 -> WU3 -> WU4 (one writer, single thread) so each commit builds on a green tree.
- Parallel inside WU2 after 2.3: 2.4 and 2.5/2.6 touch different files; 2.7 follows both.
- Parallel inside WU3: 3.1 and 3.2 (disjoint files).
- WU4: 4.2 -> 4.3 -> 4.4 sequential; 4.6, 4.7, 4.8 use fresh dirs and can run in any order after 4.3/4.4. Manual tasks (1.8, 1.9, 4.11) never block commits.
