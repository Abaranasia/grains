# Design: Soft output limiter

## Technical Approach

Replace the anonymous `finalize()` hard clamp (`Source/core/src/GranularEngine.cpp:12-18`) with a public static `GranularEngine::softLimit(float) noexcept`. It is the complete per-sample output stage: flush, pre-clamp, identity below the knee, then a rational curve. `process()` calls it per sample (lines 57-60). The curve is stateless, so it adds no latency and keeps block-split invariance (R-GR-6). It also adds no allocation (R-GR-8). Specs: R-GR-7 (modified), R-GR-10 (added), R-PS-4 (modified).

## Architecture Decisions

| Decision | Options | Tradeoff | Choice |
|---|---|---|---|
| Placement | (a) public static member, declared in `GranularEngine.h` and defined in `GranularEngine.cpp`; (b) free function in a new `grains/SoftLimiter.h`; (c) inline in the header | (b) adds a header, and its constants would drift from `kOutputCeiling`, which tests already use. (c) puts `<cmath>` logic in a widely included header. | **(a)**. The constants stay next to `kOutputCeiling`. The same TU lets `process()` inline it. Tests link `grains_core`. |
| Knee T | 0.7 / 0.8 / 0.9 | At 0.9 the span is 0.1, which gives a sharper knee for the same ceiling. The 0.924 sine peaks (below) exceed both 0.8 and 0.9, so raising T buys nothing. | **T = 0.8** |
| Formula | `T + s·u/(1+u)` (proposal) vs `C − s²/((a−T)+s)` with `s = C−T` | The two are equal in exact arithmetic. The proposal form divides two increasing quantities, which is not monotone under rounding. The chosen form is a chain of single monotone IEEE ops. | **Second form, followed by `max(T, ·)`.** It is monotone in float and the ops cannot form an FMA. |
| Arithmetic | float vs double | The engine is float. Double gains nothing because the proof below holds in float. | **float** |
| Pre-clamp | 64 | Needed for `inf`: otherwise s²/inf = 0, so y would be exactly 1.0f. | **64** (≥ 4 orders of magnitude of margin) |

## Interfaces / Contracts

`GranularEngine.h`, replacing line 36 (header comment rewrite below):

```cpp
static constexpr float kOutputCeiling = 1.0f;
static constexpr float kLimiterKnee = 0.8f;            // R-GR-10 T, 0.7 <= T <= 0.9
static constexpr float kLimiterPreClamp = 64.0f;
static constexpr float kLimiterSpan = kOutputCeiling - kLimiterKnee;   // exact (Sterbenz)
static constexpr float kLimiterSpanSq = kLimiterSpan * kLimiterSpan;
// Output stage: NaN and |y| < kFlushThreshold -> +0; |y| <= knee -> y bit-exact; else soft knee, |result| < 1.
static float softLimit(float y) noexcept;
```

`GranularEngine.cpp` (delete `finalize`; `process()` calls `softLimit`):

```cpp
float GranularEngine::softLimit(float y) noexcept
{
    const float mag = std::fabs(y);
    if (!(mag >= kFlushThreshold)) return y == 0.0f ? y : 0.0f; // +-0 kept; NaN, denormals, tiny -> +0
    const float a = std::min(mag, kLimiterPreClamp);       // +-inf, FLT_MAX -> 64
    if (a <= kLimiterKnee) return y;                       // transparent, bit-exact
    const float m = std::max(kLimiterKnee, kOutputCeiling - kLimiterSpanSq / ((a - kLimiterKnee) + kLimiterSpan));
    return y < 0.0f ? -m : m;
}
```

**Proof sketch (float).** Write q = s²/den. Then m = 1 − q, and m rounds to 1.0f only if q ≤ 2⁻²⁵ ≈ 3e-8, which needs den ≥ 1.34e6. With a ≤ 64, den ≤ 63.4 and q ≥ 6.31e-4. So `softLimit(64) ≈ 0.999369`, which is about 10,500 ulps below 1. Every step is a correctly rounded monotone operation, so 0.999369 is the global maximum. The `max(T, ·)` removes the possible 1-ulp dip just above the knee, so identity ≤ T ≤ curve and the whole function is monotone. Oddness is exact, because the result is a magnitude plus a sign. The slope is s²/den² ≤ 1, with equal to 1 only at the knee (C¹). The function uses no libm beyond `fabs`, and no multiply happens at runtime, so MSVC and clang give bit-identical results.

## Data Flow

    grains --renderSegment--> L/R float sums --softLimit per sample--> host buffer
                                                                  (plugin mono: right = scratch, discarded)

Plugin (`Source/plugin/PluginProcessor.cpp:48-82`): **no code change**. Both channels are limited inside the engine.

## Test Audit

| Location | Assertion | Verdict |
|---|---|---|
| `test_engine_extremes.cpp:81,200,225` | `peak <= kCeiling` | Change to `<`: the spec requires strict. |
| `test_engine_extremes.cpp:261-298` | Probe at 0.25 unclamped; `peak == kCeiling`; clip count; `/2`, `/4` | **Rewrite** (below) |
| `test_engine_bounds.cpp:132` | `peak <= 4.0f` (peak 0.566 < T) | Use `< GranularEngine::kOutputCeiling`. The 0.566 check at line 135 is unchanged. |
| `test_engine_bounds.cpp:252,264` | Comment "+-4"; `abs <= 4.0f` | Use `< kOutputCeiling`. Add `CHECK(peak(o) > 0.95f)`: about 128 in-phase grains give ≈ 5.5 before the limiter, which maps to ≈ 0.99. |
| `test_engine_loudness.cpp:51-65` | Within 3 dB | **Holds.** Sparse (density 20, 50 ms, norm 1, pan ≤ sin 3π/8): peak 0.924 is limited to 0.877, but the RMS loss is < 0.05 dB. Dense: about 0.8% of samples exceed the knee (≈ 2.7σ). |
| `test_engine_loudness.cpp:71-103` | Step ≤ slope bound; `maxLevel > 0.5` | **Holds.** The limiter is 1-Lipschitz (± 1 ulp, against a slack of 1e-4). The single-grain peak of 0.707 is below T. |
| `test_grain_render.cpp`, determinism, pool_full, realtime peaks | Relative or unbounded checks | Unaffected |

Rewritten coherent-DC test (LF file):

```cpp
// Coherent DC through a saturated pool is the worst case for the ceiling. Below the knee the
// limiter is the identity, so the pre-limit peak is measured at gain 2/32 (about 0.29) and scaled
// by 32, an exact power of two. The gain-2 output must then equal softLimit(pre-limit peak).
TEST_CASE("engine_extremes: worst-case coherent DC saturates softly below the ceiling", "[engine_extremes]")
{
    /* dc + run lambda unchanged */
    const Out probe = run(2.0f / 32.0f);
    REQUIRE(peak(probe) <= GranularEngine::kLimiterKnee);
    const double preLimitPeak = static_cast<double>(peak(probe)) * 32.0;
    CHECK(preLimitPeak > 9.0);   // measured 9.28
    CHECK(preLimitPeak < 9.6);

    const Out full = run(2.0f);
    REQUIRE(allFinite(full));
    CHECK(peak(full) < kCeiling);
    CHECK(peak(full) > 0.95f);   // about 0.9954
    CHECK(peak(full) == GranularEngine::softLimit(static_cast<float>(preLimitPeak)));
}
```

## File Changes

| File | EOL | Action |
|---|---|---|
| `Source/core/include/grains/GranularEngine.h` | CRLF | Constants and declaration. Rewrite lines 19-28 as follows: output passes `softLimit` (flush, then identity up to `kLimiterKnee`, then a rational knee towards `kOutputCeiling`, |y| < 1). Coherent material still sums linearly before the limiter (≈ 9.3 at gain 2, density 200, size 1 s). |
| `Source/core/src/GranularEngine.cpp` | CRLF | `softLimit` replaces `finalize` |
| `tests/test_soft_limiter.cpp` | LF, SPDX `AGPL-3.0-or-later` | Create |
| `tests/CMakeLists.txt` | CRLF | Add it to `grains_tests` |
| `tests/test_engine_extremes.cpp` | LF | As audited |
| `tests/test_engine_bounds.cpp` | CRLF | As audited |
| `tests/test_engine_realtime.cpp` | CRLF | Add a case: DC source, gain 2, density 200, size 1000, `allocs == 0`, `peak > kLimiterKnee` |
| `README.md:31` | CRLF | Replace "no limiter" with the soft 0 dBFS ceiling |

## Testing Strategy (`tests/test_soft_limiter.cpp`)

1. **Constants:** `static_assert` that the knee is in [0.7, 0.9]. `CHECK(kOutputCeiling == 1.0f)` at runtime, so it is RED rather than a compile error.
2. **Transparency (bit-exact):** walk every float in [0.5, T] with `std::nextafter`, about 5M values. Stride by `memcpy` bit patterns from `kFlushThreshold` to 0.5, step 4099. Cover both signs, plus 0, ±`kFlushThreshold`, ±T and `nextafter(T, 0)`. Every value must satisfy `softLimit(x) == x`.
3. **Curve:** sweep [-64, 64] in steps of h = 2⁻¹⁰, plus 1000 `nextafter` steps around ±T and ±1. Check monotone, exact oddness, |y| < 1, adjacent difference ≤ step + 2⁻²², `softLimit(8) > 0.99f`, and `softLimit(64) > 0.999f`.
4. **Out-of-domain:** NaN, including a negative NaN, gives +0. `±inf` and `±FLT_MAX` give `±softLimit(64)`. Subnormals, `denorm_min` and ±1e-25 give 0.
5. **Block split above the knee:** DC source, gain 2. Render with blocks of 512, 37 and one block; the outputs must be bit-identical. Also `REQUIRE(peak > kLimiterKnee)`.

## Strict TDD Order and Commit

1. Refactor (green): add the constants and `softLimit` as the existing hard clamp, with the ceiling still 4.
2. RED: add the new tests, the extremes and bounds edits, and the realtime case. The ceiling, curve, inf and coherent-DC checks fail against the clamp at 4.
3. GREEN: implement the curve and set `kOutputCeiling = 1.0f`.
4. Refactor: header comment and README.

Single commit: `feat(core): soft-limit engine output to a 1.0 ceiling`. That is about 280 changed lines, which is low risk against the 800-line budget.

## Threat Matrix

N/A: the change has no routing, shell, subprocess, VCS/PR automation, executable-file classification, or process-integration boundary.

## Migration / Rollout

No migration is needed. Parameters (9, v1) and state are unchanged. Rollback is to revert the commit.

## Open Questions

- [x] R-GR-10 conflicts with itself: it requires "transparency for |x| ≤ T" and also says subnormals flush to 0. The code is transparent only for x = 0 and kFlushThreshold ≤ |x| ≤ T. The spec should state that range. Resolved: the R-GR-10 delta states `x == 0` and `1e-20 <= |x| <= T`; exact ±0 keeps its sign.
- [x] `openspec/specs/sample-source/spec.md:262` still lists "soft limiter" as out of scope. It needs a delta or an archive-time edit. Resolved: the sample-source delta replaces that "Out of scope" entry.
