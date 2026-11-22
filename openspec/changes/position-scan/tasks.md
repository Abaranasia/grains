# Tasks: Position Scan

## Review Workload Forecast

| Field | Value |
|-------|-------|
| Estimated changed lines | ~430 core+specs, ~150 plugin, ~60 change docs/docs: ~640 total |
| Review budget | 800 lines (additions + deletions) |
| Budget risk | Medium (about 80% of budget; under it) |
| Chained PRs recommended | Yes (optional, for review load) |
| Suggested split | Slice 1 core+specs (~430) -> Slice 2 plugin (~150); a single PR with two commits is also within budget |
| Delivery strategy | single-pr |
| Chain strategy | pending (size-exception only if the forecast exceeds 800) |

Decision needed before apply: No
Chained PRs recommended: Yes
Chain strategy: pending
400-line budget risk: Medium

Note: budget is 800, not the default 400. Slice 1 alone has no user-visible effect; `ParamSpec` stays in slice 2 so the parameter never ships without its symmetric range.

### Suggested Work Units

| Unit | Goal | Likely PR | Focused test command | Runtime harness | Rollback boundary |
|------|------|-----------|----------------------|-----------------|-------------------|
| 1 | Core scan offset: limits, `EngineParams`, engine, tests 0-7 | Commit/PR 1 | `ctest --test-dir build -C Debug -R "scan\|engine\|realtime" --output-on-failure` | N/A: JUCE-free, host-free | Core headers/sources, `tests/test_engine_scan.cpp`, CMake list |
| 2 | `ParamSpec` 10th entry, symmetric maths, processor wiring | Commit/PR 2 (base = unit 1) | `ctest --test-dir build -C Debug -R "param_spec" --output-on-failure` + Release plugin build | Standalone + DAW checklist (Phase 6) | `ParamSpec.h`, `test_param_spec.cpp`, `PluginProcessor.cpp`; core stays unused |

## Phase 1: Baseline (slice 1, base commit unchanged)

- [x] 1.1 RED/characterise: `tests/test_engine_scan.cpp` golden snapshot of start positions at scan-less params (pitch 0; `position` 1.0; jitter > 0); register in `tests/CMakeLists.txt`. Run on the unchanged base first: must pass (R-GR-12 "Zero scan is identical").

## Phase 2: Params and clamp (R-GR-12 "Scan is clamped and NaN-safe")

- [x] 2.1 RED: `clamped()` for NaN, +/-inf, +/-1e9, -5, +5.
- [x] 2.2 GREEN: `Source/core/include/grains/ParamLimits.h` (`kScanMax = 1.0f`, `sanitizeScan`); `EngineParams.h` (`scan` field, default 0, clamp).

## Phase 3: Engine offset (R-GR-12, R-GR-6)

- [x] 3.1 RED: `scanOffset()` == 0.25 after 1 s at 44.1/48/96 kHz; ramp-source start index at 48 kHz ("Offset advances", "Negative scan").
- [x] 3.2 GREEN: `GranularEngine.h/.cpp` counter `elapsed_`, `scanFrom_`, `scanBase_`, `offsetAt`, `wrap01`, `scanOffset()`, `spawn(elapsed_+offset)`.
- [x] 3.3 RED: scan 0.37 with 512 / 37 / single block bit-identical; 44.1 vs 48 kHz positions agree ("Block-split and sample-rate independence").
- [x] 3.4 RED: wrap 0.9+0.5 and 0.05-0.5; scan +/-1 at 0.5 s; jitter at the seam stays in [0,1] ("Wrap at both ends", "Wrap precedes jitter clamp").
- [x] 3.5 GREEN: spawn fold (`offset == 0.0` branch keeps anchor; fold only outside [0,1]) before jitter clamp.
- [x] 3.6 RED: offset equal across a scan change; offset holds when scan returns to 0 ("Scan change is continuous", "Offset holds").
- [x] 3.7 GREEN: rebase in `setParams()` on clamped-scan change.

## Phase 4: Resets (transport-gate R-TG-4)

- [x] 4.1 RED: Play, `stopNow`, `setSource`, `prepare` reset; second Play bit-identical; redundant Play keeps offset ("Play resets", "Redundant Play keeps", "New load resets").
- [x] 4.2 GREEN: reset counters wherever the scheduler resets (prepare, stopNow, Play restart, fade-out completion, setSource).
- [x] 4.3 RED then GREEN: `tests/test_engine_realtime.cpp` zero allocations at non-zero scan (expect no code change).
- [x] 4.4 Gate: slice 1 green, golden 1.1 still bit-identical, R-BH-1 JUCE-free check.

## Phase 5: Parameter layer and plugin (slice 2; R-PM-1, R-PM-3, R-PS-3, R-PS-7)

- [x] 5.1 RED: `tests/test_param_spec.cpp` 10 ids with `scan` last; range/default; default <-> 0.5 exact; plain at 0.25 below 0.5 in magnitude; symmetric round trip vs JUCE formula; field routing; extremes clamp-stable.
- [x] 5.2 GREEN: `ParamSpec.h` `Id::Scan` after `Gain`, trailing `bool symmetric = false; int versionHint = 1;`, scan `{"scan","Scan","len/s",-1,1,0,0.4f,true,2}`, symmetric `toNormalised`/`fromNormalised`, map to `EngineParams::scan`.
- [x] 5.3 `Source/plugin/PluginProcessor.cpp`: `createLayout` passes `NormalisableRange(min,max,0,skew,symmetric)` and `ParameterID{id, versionHint}`.
- [x] 5.4 (GREEN only; the processor is not Catch2-testable, the restore check stays manual as 6.4) RED then GREEN (Catch2 where the processor is testable): pre-scan state with scan set non-zero restores to 0; `setStateInformation` appends `PARAM{id, def}` for every missing `kSpecs` id before `replaceState` ("Pre-scan state restores").
- [x] 5.5 Gate: full `grains_tests` and Release plugin build clean (Windows).

## Phase 6: Manual verification (Standalone and DAW)

- [ ] 6.1 Host shows `scan` bipolar -1/0/+1, centre at 0, fine near 0, with skewed range; unit label `len/s` acceptable (R-PS-3).
- [ ] 6.2 Editor with 10 rows is 400 px, all controls visible, window fixed (R-PS-6).
- [ ] 6.3 Scan audible: `scan` 0.1 travels through the sample, marker stays at the anchor, each Play restarts at the anchor (R-PS-4).
- [ ] 6.4 Legacy state (9 params, old-format path) restores with `scan` 0 and sample reload (R-PS-7).
- [ ] 6.5 Sweep all 10 parameters while playing: no crash or artifact; tune skew 0.4 by ear (open question).

## Phase 7: Docs

- [x] 7.1 Update README/parameter docs for `scan` (units, range, wrap, reset on Play/load), if present. N/A: the README has no parameter documentation.
- [ ] 7.2 Fold spec deltas into `openspec/specs/**` at archive; confirm "Needs design confirmation" note in plugin-shell R-PS-7 is resolved by 5.4.
