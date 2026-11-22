# Proposal: Position Scan (backlog item 3)

## Intent
`position` is static: grains always read around one point unless the user moves it by hand or automation. Add a signed `scan` control that moves the read point through the sample by itself, sample-exact and deterministic, with 0 keeping today's behaviour.

## Scope
### In Scope
- Core: `EngineParams::scan` in sample lengths per second, range [-1, +1]; engine-side scan offset added to the `position` anchor, folded by wrap BEFORE the jitter clamp in `GranularEngine::spawn()`.
- Offset resets to 0 on every Play and every new source load (restart identical, rng reseeded as today).
- Plugin: 10th APVTS parameter `scan`, default 0, symmetric skew for fine resolution near 0; generic editor fits it.
- Waveform marker stays at the anchor; dragging still sets `position`.

### Out of Scope (non-goals)
- Live scan/read-point indicator on the waveform (follow-up).
- Ping-pong or clamp end modes; host transport/tempo sync; playback modes (roadmap #8).

## Capabilities
### New Capabilities
- None.
### Modified Capabilities
- `grain-rendering`: ADDED scan requirement (offset, wrap-before-jitter, reset); MODIFIED R-GR-2 scenario ("no scan speed control"); R-GR-6 must still hold with constant scan.
- `parameter-mapping`: MODIFIED R-PM-1, R-PM-3 (10 parameters, `scan` row).
- `plugin-shell`: MODIFIED R-PS-3 and scenarios citing 9 parameters.
- `transport-gate`: ADDED/MODIFIED scenario: Play resets the scan offset.
- `waveform-display`: unchanged (R-WD-4/5/7 marker = anchor).

## Approach
Exploration option (a). Offset derived from an integer elapsed-sample counter since reset, rebased when `scan` changes: offset = base + scan * elapsed / sampleRate. Evaluated at each spawn inside scheduler segments, so it is sample-exact, block-size independent (no float accumulation per block) and sample-rate independent. Plugin-side driving rejected (block-quantised, fights automation).

## Affected Areas
| Area | Impact | Description |
|------|--------|-------------|
| `Source/core/include/grains/EngineParams.h`, `ParamSpec.h` | Modified | `scan` field, clamp, 10th spec |
| `Source/core/src/GranularEngine.cpp`, `GranularEngine.h` | Modified | Offset state, spawn fold, resets |
| `Tests/` | Modified/New | Scan, wrap, reset, block-split, rate tests |
| `Source/plugin/PluginProcessor.cpp`, `PluginEditor.*` | Modified | Layout, state compat, window height |
| `openspec/specs/*` (above) | Modified | Deltas |

## Risks
| Risk | Likelihood | Mitigation |
|------|------------|------------|
| Block-split invariance broken by accumulation | Med | Integer counter; 512/37/1-block tests at non-zero scan |
| 9-parameter saved state loads with non-default `scan` | Med | Test restore of legacy state yields 0 |
| Host display/automation of skewed bipolar range | Low | Symmetric skew; manual host check |
| 10th slider clipped by fixed window height | Med | Adjust height; manual check |
| Determinism across Play/load | Low | Reset tests with fixed seed |

## Rollback Plan
Revert the PR(s). Sessions saved with `scan` lose only that value on older builds; scan 0 is bit-identical to current output.

## Dependencies
- Base on `main` containing `transport-gate` and `waveform-display` specs (play-stop-buttons archived).

## Success Criteria
- [ ] scan 0 output bit-identical to today.
- [ ] Constant scan: block-split and sample-rate tests pass; wrap at both ends.
- [ ] Play and new load restart identically.
- [ ] 10 parameters; legacy state loads with scan 0; slider visible.

## Size Forecast
Core+tests ~350-450, plugin ~250-350, specs ~100: ~700-900 vs 800 budget (risk: High). Recommendation: two chained slices (1: core + specs, inert at scan 0; 2: parameter + plugin), preferred over size:exception.

## Open assumptions (user review)
- Product round complete (Engram `sdd/position-scan/product-decisions`); no further questions.
