# Proposal: Play and Stop Buttons

## Intent

Once a sample loads, Grains drones forever (R-PS-4). Users cannot silence it without unloading or bypassing. Add Play and Stop buttons that start and stop the granular stream cleanly, with no clicks and with every Play sounding identical.

## Scope

### In Scope
- JUCE-free `TransportGate` in core: linear ramp (~20 ms, named constant), Stop fades out then resets grain pool and scheduler; Play reseeds the rng and fades in from a fresh start.
- Non-parameter atomic `playing` flag in the processor, read once per block (block-quantised toggles).
- Default Stopped after every load, restore, and `prepare`; state not persisted.
- Editor top bar (40 px): Load (100 px), Play (~64 px), Stop (~64 px), status label; two `TextButton`s in a radio group, disabled until a sample loads.
- Keep the per-block grain snapshot publish while stopped, so display dots clear.
- Catch2 tests for every rule below; existing tests that expect sound after load send an explicit Play.

### Out of Scope
- Following the host transport (backlog; Standalone has no playhead).
- Pause/resume, natural tail, automatable or persisted Play state, keyboard shortcuts, MIDI gating.

## Capabilities

### New Capabilities
- `transport-gate`: Play/Stop state machine, ramp shape and length, fade-then-reset, rng reseed on Play, exact zeros while stopped, block-split determinism across a toggle, zero allocation.

### Modified Capabilities
- `plugin-shell`: R-PS-4 (render only while Playing; default Stopped), R-PS-6 (Play/Stop buttons and enablement), Out-of-scope and Assumptions wording ("drone always on"). R-PS-3 (9 parameters) and R-PS-7 unchanged.
- `waveform-display`: R-WD-7 (markers clear when stopped).

## Approach

Explore Approach 1. The gate multiplies the grain sum before `softLimit` (keeps |y| < 1 and NaN/denormal handling exact); the multiply is skipped while steadily playing so output stays bit-identical to today. `prepare` recomputes the ramp length for the sample rate and resets to Stopped. While stopped the engine skips scheduling and rendering (no rng draws, zero CPU) but still publishes an empty snapshot.

## Affected Areas

| Area | Impact | Description |
|------|--------|-------------|
| `Source/core/` (TransportGate, GranularEngine) | New/Modified | Gate, reset and reseed hooks |
| `Source/PluginProcessor.*` | Modified | `playing` atomic, default Stopped |
| `Source/PluginEditor.*` | Modified | Buttons, layout, enablement |
| `tests/` | New/Modified | Gate, determinism, realtime, explicit Play |
| `openspec/specs/` | Modified | Deltas above |

## Risks

| Risk | Likelihood | Mitigation |
|------|------------|------------|
| Default Stopped breaks tests that expect sound after load | High | Explicit Play in fixtures |
| Block-quantised toggle breaks R-GR-6 split test | Med | Toggle at the same sample in both runs |
| Ramp alters steady-state output | Low | Bit-identical test vs. today |
| Spec ordering with the live-grain-display change | Med | Build on its merged R-WD-7 and R-PS-4 text |

## Rollback Plan

Revert the single PR. Nothing is persisted, so there is no state migration and old projects load unchanged.

## Dependencies

- `live-grain-display` merged first (owns R-WD-5..7, snapshot publish, R-GR-11).

## Success Criteria

- [ ] Stop reaches exact zeros within the ramp; Play fades in from 0 with bounded steps; |y| < 1 throughout.
- [ ] Two Plays give bit-identical output; Playing output bit-identical to today.
- [ ] Block-split invariance holds across a toggle; zero allocations in realtime tests.
- [ ] Manual: buttons disabled before load, plugin starts Stopped after load and restore, dots clear on Stop.
