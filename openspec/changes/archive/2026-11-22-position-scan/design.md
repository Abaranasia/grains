# Design: Position Scan

## Technical Approach
Engine-side offset (proposal option a). `GranularEngine` owns an integer sample counter `elapsed_` that mirrors the scheduler timeline. Each spawn reads the offset at its exact sample `t = elapsed_ + offset`, so the result does not depend on block size or sample rate. `position` stays the anchor.

## Offset Math (core)
- State: `std::int64_t elapsed_`, `std::int64_t scanFrom_`, `double scanBase_` (kept in [0,1)).
- `offsetAt(t) = scan == 0 ? scanBase_ : scanBase_ + scan * (t - scanFrom_) / sampleRate_`.
- `process()`: the scheduler callback calls `spawn(elapsed_ + offset)`. After the final `renderSegment`, `elapsed_ += n`. This is the same path that advances the scheduler, so it skips the same calls (Stopped, no source).
- **Rebase** in `setParams()` when the clamped `scan` changes: `scanBase_ = wrap01(offsetAt(elapsed_)); scanFrom_ = elapsed_`. The offset is continuous, so the read point does not jump.
- **Reset** (`elapsed_ = scanFrom_ = 0`, `scanBase_ = 0`) wherever the scheduler resets: `prepare`, `stopNow`, Play restart, fade-out completion, plus `setSource` (new load).
- `wrap01(x)`: a non-finite x gives 0. Otherwise `r = x - floor(x)`, and `r >= 1` gives 0.
- **spawn fold**: `a = offset == 0.0 ? position : float(wrapIfOutside01(double(position) + offset))`, then `posNorm = sanitizeUnit(a + positionJitter * posDraw, 0.5f)` (jitter still clamps). Only values outside [0,1] are folded, so an anchor of exactly 1.0 stays at the last frame. The `offset == 0.0` branch keeps scan 0 bit-identical to the current code.
- Precision: after 24 h at 384 kHz, `(t - scanFrom_)` is about 3.3e10. The double ulp of the product is about 1e-11 source lengths, which is negligible. There is no periodic rebase, because that would depend on block boundaries.
- `clamped()`: `scan` NaN gives 0, and the value is clamped to ±1 (±inf gives ±1). New constant `limits::kScanMax = 1.0f`; `sanitizeScan`.

## Architecture Decisions
| Option | Tradeoff | Decision |
|---|---|---|
| Integer counter plus rebase | Exact and block-split invariant; one branch per spawn | **Chosen** |
| Per-block float phase accumulation | Drifts and depends on block size (breaks R-GR-6) | Rejected |
| Plugin-side `position` driver | Quantised to blocks; fights automation; no determinism | Rejected |
| Reuse the scheduler's `blockStart_` | Couples the scheduler API to the engine; private | Rejected; own counter |
| Fold every anchor | position 1.0 would become 0.0, so scan 0 is no longer identical | Fold only outside [0,1] |
| Append `Id::Scan` after `Gain` | Slider sits at the bottom; existing indices stay stable | **Chosen** over inserting after `Position` |
| `Spec` adds trailing `bool symmetric = false; int versionHint = 1;` | The 9 existing entries are untouched | **Chosen**; scan: `{"scan","Scan","len/s",-1,1,0,0.4f,true,2}` |

`toNormalised`/`fromNormalised` gain JUCE's symmetric branch (`d = 2p-1`, `(1 + |d|^skew * sgn d)/2`, inverse `exp(log|d|/skew)`, d==0 → exact midpoint), verified against `juce_NormalisableRange.h` 9.0.0 (the pinned version). `createLayout` passes `NormalisableRange(min,max,0,skew,symmetric)` and `ParameterID{id, versionHint}`.

## Legacy State (verified, JUCE 9.0.0 source)
`replaceState` → `updateParameterConnectionsToChildTrees`: when a parameter has no `PARAM` child, JUCE creates a new child and writes the **current live value**. It does not write the default. So loading a 9-parameter state keeps whatever `scan` was set to before. **Mitigation (required):** before `replaceState`, `setStateInformation` appends a `PARAM{id, value=def}` child for every `kSpecs` id missing from the incoming tree (unknown ids are already ignored).

## Editor Layout (verified)
`GenericAudioProcessorEditor` rows are 40 px and its height is `jlimit(125, 400, rows*40)`. `GrainsEditor` already sizes from `generic_.getHeight()`. With 10 rows the editor reaches exactly 400 px and the window grows by 40 px with no code change. An 11th parameter would scroll. This needs a manual check.

## Data Flow
    APVTS raw_[10] -> toEngineParams -> setParams (rebase) -> process
        scheduler.advance(cb: spawn(elapsed_+off)) -> offsetAt -> fold -> jitter clamp

## File Changes
| File | Action | Description |
|---|---|---|
| `Source/core/include/grains/ParamLimits.h` | Modify | `kScanMax`, `sanitizeScan` |
| `Source/core/include/grains/EngineParams.h` | Modify | `scan` field and clamp |
| `Source/core/include/grains/GranularEngine.h`/`.cpp` | Modify | counter, rebase, resets, fold, `scanOffset()` test introspection |
| `Source/core/include/grains/ParamSpec.h` | Modify | `Id::Scan`, Spec fields, symmetric maths, map |
| `Source/plugin/PluginProcessor.cpp` | Modify | layout args, missing-param defaults |
| `tests/test_engine_scan.cpp` (+CMake) | Create | scan suite |
| `tests/test_param_spec.cpp`, `test_engine_realtime.cpp` | Modify | 10 params, symmetric, alloc |

## Testing Strategy (Strict TDD)
| # | RED test | GREEN change |
|---|---|---|
| 0 | Characterisation, run on the base commit first: golden snapshot positions (pitch 0, exact IEEE operations) incl. position 1.0 and jitter | none (must stay green) |
| 1 | `clamped()`: NaN, ±inf, ±1e9 scan | limits, EngineParams |
| 2 | `scanOffset()` == 0.25 after 1 s at 44.1/48/96 kHz | counter and offsetAt |
| 3 | scan 0.37: blocks 512 / 37 / single are bit-identical | spawn(t) path |
| 4 | Wrap at both ends via snapshot (0.9 +0.5; 0.05 −0.5); jitter at the seam stays in [0,1] | fold |
| 5 | Rebase: offset equal across a scan change | setParams |
| 6 | Play, stopNow, setSource reset; second Play bit-identical | resets |
| 7 | Zero allocations at non-zero scan | none expected |
| 8 | Spec: 10 ids, `scan` last, default ↔ 0.5 exact, symmetric round-trip vs JUCE formula | ParamSpec |
| 9 | Manual: legacy state gives scan 0; slider visible; host shows the range | plugin |

## Threat Matrix
N/A: no routing, shell, subprocess, VCS/PR automation, executable-file classification, or process-integration boundary.

## Slices (budget 800)
| Slice | Content | Estimate |
|---|---|---|
| 1 core + specs | limits, EngineParams, engine, tests 0-7, spec deltas | ~430 (code+tests ~330, specs ~100) |
| 2 plugin | ParamSpec, test_param_spec, PluginProcessor | ~150 |

`ParamSpec` goes in slice 2: `createLayout` iterates `kSpecs`, so adding it in slice 1 would expose the parameter without the symmetric range. Slice 1 has no effect for users. The total (~580) also fits a single PR. Splitting is still recommended for review load.

## Migration / Rollout
No data migration. The missing-parameter defaulting covers old sessions. Older builds ignore `scan`.

## Open Questions
- [ ] Skew 0.4 is provisional; tune by ear.
- [ ] Unit label `len/s`: acceptable in hosts?
