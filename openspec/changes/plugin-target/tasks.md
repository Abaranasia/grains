# Tasks: plugin-target

Change: silent VST3 + Standalone shell with a JUCE-free parameter layer.
Specs: `specs/parameter-mapping` (R-PM-1..4), `specs/plugin-shell` (R-PS-1..7), `specs/build-harness` (R-BH-3 modified).
Design: `design.md`. Strict TDD is enabled: every core task is a RED then GREEN pair.

Default-build commands (plugin OFF, used for all core tasks):

- Configure: `cmake -S . -B build -A x64 -DGRAINS_BUILD_PLUGIN=OFF`
- Build: `cmake --build build --config Debug`
- Test: `ctest --test-dir build -C Debug --output-on-failure`
- Baseline: 84/84 passing. Must stay green after every task; new param tests are added on top.

## Review Workload Forecast

| Item | Estimated changed lines |
| --- | --- |
| `Source/core/include/grains/ParamSpec.h` (new) | ~110 |
| `tests/test_param_spec.cpp` (new) | ~150 |
| `tests/CMakeLists.txt` (modify) | ~1 |
| `CMakeLists.txt` (modify) | ~20 |
| `Source/plugin/CMakeLists.txt` (new) | ~35 |
| `Source/plugin/PluginProcessor.{h,cpp}` (new) | ~170 |
| **Code + tests + CMake total** | **~486** |
| Docs (build note ~20 + OpenSpec artifacts ~350), reported separately | ~370 |

- Review budget: 800 lines (code + tests + CMake). Forecast ~486, so roughly 60 percent of budget, no overrun risk expected. (The 400-line figure used by the delivery guard is exceeded by about 85 lines; the user budget of 800 governs.)
- Chained PRs recommended: no. Single PR, two commits (core, then plugin) for reviewability.
- Decision needed before apply: no. Open items (final plugin identity, licensing) do not block; placeholders are used.

## Commit 1: `feat(core): add JUCE-free parameter spec table and mapping`

Parallelism: tasks 1.1 to 1.6 are sequential (all touch the same header and test file). Task 1.7 is the commit gate.

### [x] 1.1 Test scaffolding and registration (R-PM-1..4)

- Files: `tests/test_param_spec.cpp` (new, empty Catch2 TU including `grains/ParamSpec.h`), `tests/CMakeLists.txt` (add the file to `grains_tests`).
- RED: build fails because `grains/ParamSpec.h` does not exist.
- GREEN: add minimal `Source/core/include/grains/ParamSpec.h` with `namespace grains::params`, `enum class Id` (9 ids + `Count`), `kCount`, and `struct Spec{id,name,unit,min,max,def,skew}`. Build compiles; 84/84 still pass (no test cases yet).
- Verify: configure and build with the default-build commands; `ctest` reports 84/84.

### [x] 1.2 Table: unique IDs, ranges and defaults from limits (R-PM-1, R-PM-2)

- Files: `tests/test_param_spec.cpp`, `Source/core/include/grains/ParamSpec.h`.
- RED: tests assert 9 entries; stable string IDs `grainSize, density, position, positionJitter, pitch, pitchJitter, timingJitter, panSpread, gain` are unique and ordered by `Id`; min/max/def match `grains::limits` per the design table; `min <= def <= max`; skew > 0.
- GREEN: add `constexpr std::array<Spec,kCount> kSpecs` populated from `grains::limits` (skew 0.3 for grainSize and density, 1.0 elsewhere).
- Verify: `ctest --test-dir build -C Debug --output-on-failure -R "param"`.

### [x] 1.3 Defaults equal EngineParams (R-PM-2)

- Files: `tests/test_param_spec.cpp`.
- RED: test builds an array from `kSpecs[i].def` and compares with `EngineParams{}` field by field. (If 1.2 already satisfies it, record it as a characterization test that passes immediately.)
- GREEN: adjust table defaults only if the test fails.
- Verify: same `-R "param"` run.

### [x] 1.4 Normalisation with skew and clamping (R-PM-3)

- Files: `tests/test_param_spec.cpp`, `Source/core/include/grains/ParamSpec.h`.
- RED: tests for `toNormalised` / `fromNormalised`: round trip within 1e-4 including endpoints for all 9 specs; midpoint of a skewed spec equals the `juce::NormalisableRange` formula computed inline in the test (`plain = min + (max-min)*exp(ln(n)/skew)`, `n = ((v-min)/(max-min))^skew`, no JUCE include); out-of-range plain clamps to `[min,max]`; out-of-range normalised clamps to `[0,1]`; NaN maps to `def` for both functions.
- GREEN: implement both functions `constexpr`/inline, header-only.
- Verify: `-R "param"`.

### [x] 1.5 toEngineParams mapping and density dead zone (R-PM-3)

- Files: `tests/test_param_spec.cpp`, `Source/core/include/grains/ParamSpec.h`.
- RED: tests that `toEngineParams(array<float,kCount>)` maps each slot to the matching `EngineParams` field; result is `.clamped()`; density exactly 0 stays 0 (off); density in `(0, kDensityMin)` snaps up to `kDensityMin`; out-of-range inputs are clamped; defaults array yields `EngineParams{}`.
- GREEN: implement `toEngineParams` using `EngineParams::clamped()`.
- Verify: `-R "param"`.

### [x] 1.6 JUCE-freedom of the header (R-PM-4)

- Files: `Source/core/include/grains/ParamSpec.h`, `tests/test_param_spec.cpp`.
- RED/GREEN: confirm the header includes only standard library and `grains/` headers. Enforced by convention and review (the `grains_assert_juce_free` check covers link dependencies, not includes). Optionally add a tests-side compile check that the TU builds with plugin OFF, which already proves no JUCE include.
- Verify: `rg -n "juce" Source/core/include/grains/ParamSpec.h` returns no matches (use `rg`, not grep); plugin-OFF build is green.

### [x] 1.7 Commit gate

- Verify: full default build + `ctest` shows 84 baseline plus the new param tests, all passing, zero warnings (`/WX` active for core).
- Commit message: `feat(core): add JUCE-free parameter spec table and mapping`.

## Commit 2: `feat(plugin): add opt-in silent VST3 and Standalone target`

Parallelism: 2.1 and 2.2 can be written in parallel (different files) but must be reviewed together; 2.3 depends on both; 2.4 (docs) is independent and can run in parallel with 2.1 and 2.2. 2.5 onward are sequential verification. JUCE glue has no automated unit tests (design decision); RED is replaced by a failing verification step, GREEN by it passing.

### [x] 2.1 Root CMake plugin branch (R-BH-3)

- Files: `CMakeLists.txt`.
- RED: `cmake -S . -B build-plugin -A x64 -DGRAINS_BUILD_PLUGIN=ON` fails or does nothing useful (option does not exist).
- GREEN: add `option(GRAINS_BUILD_PLUGIN ... OFF)` and cache PATH `GRAINS_JUCE_DIR` (default `${CMAKE_SOURCE_DIR}/../../Projucer`). When ON: `FATAL_ERROR` with a clear message if `${GRAINS_JUCE_DIR}/CMakeLists.txt` is missing; `enable_language(C)`; `add_subdirectory(${GRAINS_JUCE_DIR} ${CMAKE_BINARY_DIR}/juce EXCLUDE_FROM_ALL)`; `add_subdirectory(Source/plugin)`. When OFF nothing JUCE-related runs and the project stays CXX-only. CI stays OFF (no workflow change).
- Verify: default configure (OFF) unchanged and 84+new tests green.

### [x] 2.2 Plugin target CMake (R-BH-3, R-PS-2)

- Files: `Source/plugin/CMakeLists.txt` (new).
- GREEN: `juce_add_plugin(GrainsPlugin FORMATS VST3 Standalone IS_SYNTH TRUE NEEDS_MIDI_INPUT TRUE ...)` with placeholder metadata (COMPANY_NAME Grains, PLUGIN_MANUFACTURER_CODE Grns, PLUGIN_CODE Grn1, PRODUCT_NAME Grains, BUNDLE_ID com.grains.grains, COPY_PLUGIN_AFTER_BUILD FALSE); compile definitions `JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0`; link `grains_core`, `juce::juce_audio_utils` (plus processors/devices as needed) and `juce::juce_recommended_{config,lto,warning}_flags`; do not use `grains_set_warnings`; no `juce_generate_juce_header`.
- Verify: with 2.3 in place, see 2.5.

### [x] 2.3 PluginProcessor (R-PS-1..7)

- Files: `Source/plugin/PluginProcessor.h`, `Source/plugin/PluginProcessor.cpp` (new).
- GREEN, per requirement:
  - Buses: one stereo output, no input; accept mono or stereo output (R-PS-1, R-PS-2).
  - MIDI accepted and ignored; `processBlock` clears the MIDI buffer (R-PS-2).
  - Constructor builds the APVTS layout by looping `grains::params::kSpecs` with `ParameterID{id, 1}` and `NormalisableRange` using spec skew; caches 9 `getRawParameterValue` pointers (R-PS-3).
  - `prepareToPlay`: `engine.prepare(sampleRate, fixedSeed)` only; max block size sizes the mono scratch R buffer (R-PS-4).
  - `processBlock`: `ScopedNoDenormals`, relaxed atomic loads into `plain[9]`, `params::toEngineParams`, `engine.setParams`, `engine.process(L, R or scratch, n)` with a chunked loop if the host block exceeds scratch size; no locks, allocation or logging (R-PS-4, R-PS-1 silence since no source).
  - Editor: `juce::GenericAudioProcessorEditor` (R-PS-5).
  - State: `apvts.copyState` to XML to binary; restore checks the XML tag before `replaceState` (R-PS-6).
- Verify: see 2.5.

### [x] 2.4 Build documentation note (R-BH-3) (docs, parallel)

- Files: existing build documentation (README or docs build section; ~20 lines).
- Content: how to configure with `-DGRAINS_BUILD_PLUGIN=ON -DGRAINS_JUCE_DIR=<path>` in a separate `build-plugin` directory, targets `GrainsPlugin_VST3` and `GrainsPlugin_Standalone`, default OFF, CI stays OFF, `GRAINS_SANITIZE` with the plugin is unsupported. Use repo-relative paths and no usernames.
- Verify: `rg -n "GRAINS_BUILD_PLUGIN" <doc file>` finds the note.

### [x] 2.5 Automated verification (an agent can do these)

1. Regression, plugin OFF: configure `build`, build Debug, `ctest`: baseline 84 plus new param tests all green. Confirm the configure output has no JUCE or C language activity.
2. Error path: `cmake -S . -B build-plugin-bad -A x64 -DGRAINS_BUILD_PLUGIN=ON -DGRAINS_JUCE_DIR=does-not-exist` fails with the clear `GRAINS_JUCE_DIR` message (R-BH-3). Delete the temp dir afterwards.
3. Configure ON: `cmake -S . -B build-plugin -A x64 -DGRAINS_BUILD_PLUGIN=ON` succeeds using the default JUCE dir (requires JUCE sources present at the default path).
4. Build: `cmake --build build-plugin --config Debug --target GrainsPlugin_VST3` and `--target GrainsPlugin_Standalone` succeed. Confirm default `--build build` does not build them (EXCLUDE_FROM_ALL).
5. Ensure `build-plugin*` directories are not committed (check `.gitignore` covers them; add a pattern if missing, counted as docs/CMake trivia).

### 2.6 Manual verification (human required)

- [ ] MANUAL — pending user (all items below; not run by the apply agent)

1. Launch the Standalone exe from `build-plugin` output: window opens, the generic editor shows exactly 9 parameters with expected names (R-PS-5, R-PM-1).
2. Audio output is silent with all defaults and while moving every control (R-PS-1).
3. Change several parameters, close and reopen (or host save/load): values restored (R-PS-6). Automation of a parameter in a VST3 host moves it without glitches or crashes (R-PS-3, R-PS-4).
4. Mono layout: in Standalone audio settings choose a mono output, or load the VST3 on a mono track in a host; no crash and silence (R-PS-2 mono acceptance).
5. Optional: load the VST3 in a host, send MIDI notes, confirm they are ignored without errors (R-PS-2).

### [x] 2.7 Commit gate (2.5 steps 1-4 passed; 2.6 pending user)

- Verify: 2.5 steps 1 to 4 pass; 2.6 results recorded in the apply or verify report (or flagged as pending human check).
- Commit messages: `build: add opt-in GRAINS_BUILD_PLUGIN with JUCE subdirectory import` for CMake plus docs if split, otherwise `feat(plugin): add opt-in silent VST3 and Standalone target`. No AI attribution.

## Requirement Traceability

| Requirement | Tasks |
| --- | --- |
| R-PM-1 (9 stable IDs) | 1.1, 1.2 |
| R-PM-2 (ranges, defaults) | 1.2, 1.3 |
| R-PM-3 (mapping, skew, clamp, NaN, density) | 1.4, 1.5 |
| R-PM-4 (JUCE-free) | 1.6 |
| R-PS-1 (silent) | 2.3, 2.6 |
| R-PS-2 (metadata, buses, MIDI ignored) | 2.2, 2.3, 2.6 |
| R-PS-3 (APVTS from table) | 2.3, 2.6 |
| R-PS-4 (RT-safe, host sample rate) | 2.3, 2.6 |
| R-PS-5 (generic editor) | 2.3, 2.6 |
| R-PS-6 (state persistence) | 2.3, 2.6 |
| R-PS-7 (placeholder metadata) | 2.2 |
| R-BH-3 (OFF default, JUCE dir, EXCLUDE_FROM_ALL, error) | 2.1, 2.2, 2.4, 2.5 |
