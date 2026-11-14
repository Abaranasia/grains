# Exploration: soft-limiter

Source: roadmap item P1 in `openspec/changes/archive/2026-11-14-loader-hardening/archive-report.md` ("Soft limiter decision: output can reach the ±4 ceiling with the drone always on"). Engram mirror: `sdd/soft-limiter/explore` (obs #605).

## Current state

- **Ceiling and clamp.** `GranularEngine::kOutputCeiling = 4.0f` is defined in `Source/core/include/grains/GranularEngine.h:36`. `finalize()` in `Source/core/src/GranularEngine.cpp:12` flushes NaN and values below 1e-20 to 0, then hard-clamps to ±4. `process()` calls it once per output sample, after `renderSegment` has summed the grains.
- **Gain compensation already exists.** `spawn()` scales each grain by `gain * 1/sqrt(max(1, density*size)) * panFactor`.
  - This keeps uncorrelated material low.
  - Coherent or DC-like material grows linearly with grain overlap. A constant-1 source at density 200 and size 1000 ms peaks at about 9.3 at gain 2, and about 4.64 at gain 1.
  - `test_engine_extremes.cpp:261-298` documents this case.
- **Specs that state the ±4 bound.**
  - `grain-rendering` R-GR-7: "bounded (|y| <= a documented ceiling, default 4.0)".
  - `plugin-shell` R-PS-4: "bounded by ±4 (no limiter is applied)". Its "Sound after load" scenario says "peak ≤ 4", and the out-of-scope list and Assumptions both mention the soft limiter.
- **Tests that depend on the ceiling.**
  - `tests/test_engine_extremes.cpp` compares against `kCeiling` at lines 81, 200 and 225.
  - The coherent-DC test at lines 265-298 hard-codes the clamp. It asserts `peak == kCeiling` and counts clipped samples.
  - `tests/test_engine_loudness.cpp` checks that dense and sparse output stay within 3 dB, and bounds the step between adjacent points of a density sweep.
- **Parameters and state.** There are exactly 9 parameters (R-PS-3), created with `ParameterID{id, 1}`. State is the APVTS XML plus the sample path. A fixed limiter changes neither.

## Approaches

| Approach | Pros | Cons | Effort |
|---|---|---|---|
| (a) Static soft clipper in the engine | Zero latency and no state. Bit-identical across block sizes, allocation-free, easy to test. | Colors loud peaks. It saturates rather than controlling level. | Low |
| (b) Lookahead peak limiter | Transparent gain reduction | Needs host latency reporting, a delay line, release and reset logic. Overkill for this engine. | High |
| (c) Gain scaled by active-grain count | sqrt(N) scaling already exists in `spawn()` | Does not help coherent material, and would make loudness pump as grain count changes. | Not worth it |
| (d) Hard clamp | This is what ships today | Distorts harshly. A ±4 ceiling (+12 dBFS) does not protect the host. | None |

## Recommendation

Use approach (a): a fixed limiter with no user parameter, inside `finalize()` in `GranularEngine.cpp`.

`finalize(y)` would run these steps in order:

1. Flush NaN and values below `kFlushThreshold` to 0 (unchanged).
2. Hard pre-clamp to ±`kPreClamp` (about 64). This avoids `inf/inf` in step 4.
3. If `|y| <= T`, return `y` unchanged. The knee `T` is 0.8, about −1.9 dBFS.
4. Otherwise return `sign(y) * (T + (C - T) * u / (1 + u))`, where `u = (|y| - T) / (C - T)` and `C = kOutputCeiling = 1.0`.

Properties of this curve:

- It is odd and monotone.
- It is C¹ at the knee: the slope is 1 on both sides.
- Its slope never exceeds 1, and it stays strictly below `C`.
- It uses only `+`, `−` and `/`, so it gives the same bits on MSVC and clang. `tanh` would depend on each platform's libm.

Mapping examples: 9.3 → about 0.995, and 4.64 → about 0.99.

Because it is stateless and per-sample, it keeps block-split invariance. It adds no allocation, locks or latency, and channels are limited independently.

## Expected spec changes

- **grain-rendering R-GR-7 (MODIFIED):** the ceiling becomes 1.0, approached asymptotically.
- **grain-rendering (ADDED) "Soft output limiting":**
  - Output is transparent for |y| ≤ 0.8.
  - Above the knee it is monotone, continuous and odd.
  - Output is bounded by 1.0 for any input.
- **plugin-shell R-PS-4 (MODIFIED):** the output bound becomes the engine's soft-limited 1.0 ceiling, and the "Sound after load" peak becomes ≤ 1.
- **plugin-shell (edited text):** remove the soft limiter from the out-of-scope list and from Assumptions.
- **parameter-mapping:** no change.

## Test strategy (Strict TDD)

1. **Worst-case bound.** Constant-1 source, density 200, size 1000 ms, gain 2. Every sample must be finite with `|y| < 1`, and the peak must exceed 0.95.
2. **Transparency.** Output below the knee must be bit-identical to the unlimited sum.
3. **Curve properties.** Expose the curve as a static function and sweep x over [−64, 64]. Check that it is monotone, odd and exact for |x| ≤ 0.8, has slope ≤ 1 + ε, and is continuous at the knee.
4. **Out-of-domain input.** ±inf, NaN, 1e30 and subnormals must give finite output within ±1, with subnormals flushed to zero.
5. **Block-split invariance.** The existing test still passes.
6. **No allocation.** The existing realtime test covers `process`.
7. **Rewrite the coherent-DC test** in `test_engine_extremes.cpp:261-298` for the new curve.

## Estimate

About 30 lines in the core, 120-170 lines of tests and 40 lines of specs: roughly 200-250 in total, one work unit.

## Risks

- **Intended sound change.** Material peaking above 0.8 is now shaped, and gain above 1 saturates.
- **Unverified loudness test.** The source amplitude in `test_engine_loudness.cpp` has not been checked. If it peaks above 0.8, the 3 dB comparison may shift. Check this before fixing the knee at 0.8.
- **Pre-clamp ordering.** The pre-clamp must come before the rational term, and a test must cover ±inf.
- **Ceiling used by tests.** `kOutputCeiling` is public and the tests use it, so every assertion that references it needs review.
- **Not a true limiter.** The follow-up for that would be approach (b) with `setLatencySamples`.
