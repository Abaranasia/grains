# Proposal: Soft output limiter

## Intent

Today the engine hard-clamps its output at ±4 (+12 dBFS), and the drone is always on. Coherent or DC-like material grows linearly with grain overlap and reaches that ceiling (about 9.3 before the clamp at gain 2). The result is harsh clipping and output well above 0 dBFS reaching the host. This change resolves roadmap item P1.

## Scope

### In Scope
- A fixed, stateless, per-sample rational soft clipper in `GranularEngine::finalize()`. It flushes NaN and denormals first, then pre-clamps to about ±64, then applies the knee.
- `kOutputCeiling` goes from 4 to 1.0. The knee is T = 0.8. Design may adjust T within [0.7, 0.9] after checking the source amplitude in `test_engine_loudness.cpp`.
- The curve is exposed as a static function so it can be tested directly.
- Tests: worst-case bound, transparency below the knee, curve properties, out-of-domain input, and a rewrite of the coherent-DC test.
- Spec deltas.

### Out of Scope
- A lookahead limiter (approach b), which could be a future change.
- A user-adjustable ceiling or drive, and an output gain parameter.
- Oversampling or anti-aliasing for the clipper. Aliasing is an accepted tradeoff.
- P2, P3, P4 and P6.

## Capabilities

### New Capabilities
None.

### Modified Capabilities
- `grain-rendering`:
  - R-GR-7 MODIFIED: the ceiling becomes 1.0, approached asymptotically.
  - ADDED "Soft output limiting": transparent for |y| ≤ T; above the knee it is monotone, odd and C¹; |y| < 1 for any input.
- `plugin-shell`:
  - R-PS-4 MODIFIED: output is bounded by the engine's soft 1.0 ceiling, and "Sound after load" expects peak ≤ 1.
  - Remove the soft limiter from Out of Scope and from Assumptions.
- `parameter-mapping`: unchanged.

## Approach and work units

The curve is `sign(y)·(T + (C−T)·u/(1+u))` with `u = (|y|−T)/(C−T)`. It uses only `+`, `−` and `/`, so results are bit-identical on MSVC and clang. It adds no allocation and no latency, and it keeps block-split invariance. Parameters (9, version 1) and state stay unchanged.

| WU | Commit | Lines |
|---|---|---|
| 1 | `feat(core): soft-limit engine output to a 1.0 ceiling` (code, tests, spec deltas) | ~200-250 |

Delivery forecast:
- Decision needed before apply: No
- Chained PRs: No
- 800-line budget risk: Low

## Affected Areas

| Area | Impact |
|---|---|
| `Source/core/include/grains/GranularEngine.h`, `Source/core/src/GranularEngine.cpp` | Modified |
| `tests/test_engine_extremes.cpp`, `tests/test_engine_loudness.cpp` (review) | Modified |
| New test for the curve, e.g. `tests/test_soft_limiter.cpp` | New |

## Risks

| Risk | Likelihood | Mitigation |
|---|---|---|
| Audible change: loud material now saturates near 0 dBFS | High (intended) | Documented. Use the knee transparency test. |
| Loudness test shifts if its source peaks above T | Med | Design verifies and tunes T |
| inf/inf if the pre-clamp ordering is wrong | Low | Test with ±inf and NaN |
| Tests that reference `kOutputCeiling` | Med | Audit every assertion that uses it |

## Rollback Plan

Revert the single commit. This restores the ±4 hard clamp and the old spec text. No state migration is needed.

## Success Criteria

- [ ] Worst-case coherent input stays finite with |y| < 1 and peak > 0.95.
- [ ] Output with |y| ≤ T is bit-identical to the input.
- [ ] The existing block-split and no-allocation tests pass.
- [ ] The spec deltas match the code.

## Proposal question round

Auto mode. These assumptions are for the user to review:
1. A 1.0 ceiling is an intended audible change: coherent material saturates softly instead of clamping at +12 dBFS.
2. Users get no control over the ceiling or drive.
3. Aliasing from the clipper is acceptable without oversampling.
4. The lookahead limiter is deferred and not scheduled.
