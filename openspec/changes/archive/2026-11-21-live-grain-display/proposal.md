# Proposal: Live Grain Display

> **Superseded by design**: the numbers in this document are superseded. See `design.md` (Proposal Corrections) for the corrected values: clear after 8 non-fresh ticks (~267 ms, not 250 ms), 16-byte `GrainMark` (not 12), flicker only for blocks above ~233 ms (~10k samples at 44.1 kHz, not 8192).

## Intent
Users cannot see what the engine plays; position and jitter are abstract. Show active grains live under the waveform so moving `position`/`jitter` gives visual feedback, without touching audio output or real-time safety.

## Scope
### In Scope
- Core: generic wait-free triple-buffer mailbox; `GrainSnapshot` (versioned, marks carry pool slot index, head position, progress, level); `GranularEngine::snapshotGrains(GrainSnapshot&) const noexcept` via const pool iteration; JUCE-free `GrainDisplayModel` (fresh/stale, clear after ~250 ms).
- Plugin: publish after render on mono and stereo paths plus an empty snapshot after `prepare`; lane-dot strip under the waveform (one dot per slot, rows); 30 Hz timer only while showing; waveform cached in an image.

### Out of Scope
- TSAN CI job (separate micro-PR); audio/interaction real-time feedback (meters, scope); extent bars, direction, roadmap #8; new parameters; markers in "no waveform"/empty states.

## Capabilities
### New Capabilities
- None (folded into existing specs).

### Modified Capabilities
- `waveform-display`: modify R-WD-4 (30 Hz poll timer while showing; static layers stay change-driven); add R-WD-5 snapshot contract, R-WD-6 publication, R-WD-7 live lane-dot view; update Purpose/out-of-scope.
- `plugin-shell`: R-PS-4 publish step; R-PS-5 lock-free, allocation-free publish.
- `grain-rendering`: add R-GR-11 introspection must not alter output, RNG or pool, nor allocate.

## Approach
Explore option 1: triple buffer (tear-free, wait-free both sides, `SourceExchange` style), snapshot every block (<=128 iterations), UI polls at one 30 Hz constant, repaints only on fresh frame or stale transition. Design settles strip height and slot-to-row mapping.

## Affected Areas
| Area | Impact | Description |
|------|--------|-------------|
| `Source/core/include/grains/` (mailbox, snapshot, display model, `GrainPool.h`, `GranularEngine.h`) | New/Modified | Snapshot and publication types |
| `Source/core/src/GranularEngine.cpp` | Modified | `snapshotGrains` |
| `Source/plugin/PluginProcessor.*` | Modified | Publish hook, stereo early-return restructure |
| `Source/plugin/WaveformView.*` | Modified | Timer, strip, image cache |
| `tests/` | New | Snapshot, mailbox, model, bit-identity, zero-alloc |

## Risks
| Risk | Likelihood | Mitigation |
|------|------------|------------|
| Reverses archived R-WD-4 "no timer" | Med | Explicit delta; timer stops when hidden |
| `processBlock` restructure lacks plugin-level test | Med | Core bit-identical render test; manual A/B |
| Sub-block grains invisible | High | Documented; representative sample |
| One-poll mismatch on source swap | Low | Accepted; cleared next frame |
| Stale flicker with >8192-sample blocks | Low | Threshold is a named constant |

## Rollback Plan
Revert plugin PR (view and hook) independently; core PR is additive and inert if unused, revert separately if needed.

## Dependencies
- Archived `waveform-position-ui` (on branch).

## Success Criteria
- [ ] Render bit-identical with/without snapshot; zero allocations in snapshot+publish.
- [ ] Snapshot `count == activeGrainCount()`; mailbox tear-free under two-thread stress.
- [ ] Markers clear ~250 ms after transport stops; none in "no waveform".
- [ ] Manual only: painting, timer lifecycle, DAW stopped transport, resize/DPI, multi-instance CPU.

## Delivery forecast (budget 800)
| PR | Lines | Risk |
|----|-------|------|
| 1 core | ~650-700 | Medium |
| 2 plugin | ~200-300 | Low |

## Proposal question round
Resolved in engram `decision/live-grain-display-product`; no open product questions.
