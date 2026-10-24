# Apply progress: plugin-target

Status: all automated tasks done (14 of 14 task groups; 2.6 manual checks pending user).

## Group 1 (core, strict TDD)

| Task | RED | GREEN |
| --- | --- | --- |
| 1.1 | build failed: `grains/ParamSpec.h` not found | minimal header (Id, kCount, Spec); 84/84 |
| 1.2 | build failed: `kSpecs` undeclared | table from `grains::limits` and `EngineParams{}`; ids/ranges/skew tests pass |
| 1.3 | characterization (passed immediately); proven RED by temporarily setting the position default to 0.4f in the table: "table defaults equal EngineParams{}" failed (15/16); sabotage reverted | passes |
| 1.4 | build failed: `toNormalised` undeclared | `toNormalised`/`fromNormalised` (endpoints, round trip, monotonic, skew formula, clamp, NaN) |
| 1.5 | build failed: `toEngineParams` undeclared | `toEngineParams` via `clamped()` (routing, dead zone, extremes) |
| 1.6 | n/a | `rg -n -i juce ParamSpec.h` has no matches; plugin-OFF build green |
| 1.7 | | 95/95 ctest (84 baseline + 11 new), zero warnings under `/WX` |

## Group 2 (plugin)

- 2.1 RED: configure with ON failed (`Source/plugin` missing); GREEN after 2.2/2.3. Bad `GRAINS_JUCE_DIR`
  fails with a message naming the variable and path.
- 2.2/2.3: `Source/plugin/CMakeLists.txt`, `PluginProcessor.{h,cpp}`.
- 2.4: `README.md` (no existing docs).
- 2.5: (1) OFF regression 95/95, no C language or JUCE in the OFF cache. (2) bad-dir error path OK, temp dir removed.
  (3) ON configure OK. (4) `GrainsPlugin_VST3` and `GrainsPlugin_Standalone` build Debug, no warnings from first-party code;
  default `--build` of the ON tree does not build them. (5) `build-plugin*/` added to `.gitignore`.
- 2.6: MANUAL — pending user.

## Deviations from design

- `project(Grains VERSION 0.1.0 ...)`: JUCE's `juce_add_plugin` requires a project version.
- `add_subdirectory(Source/plugin EXCLUDE_FROM_ALL)` plus `add_custom_target(grains_plugin DEPENDS ...)`: Visual Studio
  solutions omit excluded targets, which breaks `cmake --build --target GrainsPlugin_VST3`. The aggregate target
  makes them visible while keeping them out of `all` (matches R-PS-1 / R-BH-3).

## Final results

ctest (plugin OFF): 95/95 passed. Plugin ON: VST3 and Standalone built.
