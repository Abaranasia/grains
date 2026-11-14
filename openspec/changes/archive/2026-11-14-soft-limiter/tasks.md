# Tasks: Soft output limiter

## Review Workload Forecast

| Field | Value |
|-------|-------|
| Estimated changed lines | 280-350 |
| 800-line budget risk | Low |
| Chained PRs recommended | No |
| Suggested split | Single PR, single commit |
| Delivery strategy | ask-on-risk |
| Chain strategy | pending (not needed) |

Decision needed before apply: No
Chained PRs recommended: No
Chain strategy: pending
800-line budget risk: Low

### Suggested Work Units

| Unit | Goal | Likely PR | Focused test command | Runtime harness | Rollback boundary |
|------|------|-----------|----------------------|-----------------|-------------------|
| 1 | `feat(core): soft-limit engine output to a 1.0 ceiling` | PR 1 | `ctest -R "soft_limiter|engine_"` (grains_tests) | Standalone manual check (R-GR-10 manual scenario) | Revert the single commit |

## Phase 1: Refactor (green, ceiling still 4)

- [x] 1.1 `GranularEngine.h`: add `kLimiterKnee` 0.8, `kLimiterPreClamp` 64, `kLimiterSpan`, `kLimiterSpanSq`, and `static float softLimit(float) noexcept`.
- [x] 1.2 `GranularEngine.cpp`: replace anonymous `finalize` with `softLimit` (flush, pre-clamp, hard clamp at 4); `process()` calls it. Existing tests stay green.

## Phase 2: RED tests (R-GR-7, R-GR-10)

- [x] 2.1 Create `tests/test_soft_limiter.cpp` (LF, SPDX AGPL-3.0-or-later) and add to `tests/CMakeLists.txt` (CRLF): `static_assert` knee in [0.7, 0.9]; runtime `kOutputCeiling == 1.0f`.
- [x] 2.2 Same file, transparency: bit-exact over [0.5, T] via `nextafter`, strided from `kFlushThreshold` to 0.5 (step 4099), both signs, 0, ±T; sub-threshold gives 0.
- [x] 2.3 Same file, curve sweep [-64, 64] step 2^-10 plus `nextafter` around ±T, ±1: monotone, odd, |y| < 1, step bound, `softLimit(8) > 0.99f`, `softLimit(64) > 0.999f`.
- [x] 2.4 Same file, out-of-domain: NaN (both signs) gives +0; ±inf and ±FLT_MAX give ±`softLimit(64)`; subnormals and ±1e-25 give 0.
- [x] 2.5 Same file, block-split: DC source, gain 2, blocks 512 / 37 / one block bit-identical; `peak > kLimiterKnee`.
- [x] 2.6 `tests/test_engine_extremes.cpp` (LF): change `peak <= kCeiling` to `<` at lines 81, 200, 225; rewrite coherent-DC test (lines 261-298) per design (probe at gain 2/32, scale by 32, `peak(full) == softLimit(pre)`, `> 0.95f`).
- [x] 2.7 `tests/test_engine_bounds.cpp` (CRLF): line 132 and 252/264 use `< kOutputCeiling`; fix "+-4" comment; add `CHECK(peak(o) > 0.95f)`.
- [x] 2.8 `tests/test_engine_realtime.cpp` (CRLF): add no-allocation case, DC source, gain 2, density 200, size 1000, `allocs == 0`, `peak > kLimiterKnee`.
- [x] 2.9 Build and run `grains_tests`; confirm the new and edited tests fail against the clamp at 4 (RED).

## Phase 3: GREEN

- [x] 3.1 `GranularEngine.cpp`: implement the curve `max(T, C - s^2 / ((a - T) + s))` with sign restore; set `kOutputCeiling = 1.0f` in the header.
- [x] 3.2 Run the full suite; confirm `test_engine_loudness.cpp` (within 3 dB, step slope) passes unchanged, then run the clang UBSan build. Local MSVC suite green (131/131, loudness unchanged); clang UBSan covered by the CI linux-sanitizers job, green on ad0351c (reported by the maintainer; not observed from this host).

## Phase 4: Refactor and docs

- [x] 4.1 `GranularEngine.h` (CRLF): rewrite comment at lines 19-28 to describe `softLimit` and the coherent sum (about 9.3 at gain 2).
- [x] 4.2 `README.md:31` (CRLF): replace "no limiter" with the soft 0 dBFS ceiling.
- [x] 4.3 Resolve spec open questions: state the transparency range (0 and `kFlushThreshold` <= |x| <= T) in R-GR-10; delta or archive-time edit for `openspec/specs/sample-source/spec.md:262`.
- [x] 4.4 Manual: Standalone with heavy overlap, confirm never above 0 dBFS and quiet passages unchanged. Commit with `feat(core): soft-limit engine output to a 1.0 ceiling`. Standalone (build/plugin-jf2, Debug) checked by the maintainer with heavy overlap (1000 ms, ~150 gr/s, gain 1.68); committed as ad0351c.
