# Verification Report: soft-limiter

**Change**: soft-limiter | **Mode**: Strict TDD (ctest + Catch2 v3.8.1) | **Store**: hybrid
**Commit under test**: ad0351c (feat/soft-limiter) plus uncommitted tasks.md checkbox edits (3.2, 4.4)
**Verdict**: PASS WITH WARNINGS (0 CRITICAL, 3 WARNING, 2 SUGGESTION)

## Completeness
Tasks: 17/17 checked. No unchecked task. Artifacts read: proposal, design, 3 delta specs, tasks, apply-progress (#611).
Delta spec scope: 2 modified requirements groups in grain-rendering (R-GR-7, 8 scenarios) plus 1 added (R-GR-10, 7 scenarios); plugin-shell R-PS-4 (5 manual scenarios); sample-source out-of-scope (no scenarios).

## Build and tests (executed by verifier)
- Build: `cmake --build build --config Debug` -> exit 0, all targets built (grains_core, grains_tests, grains_realtime_tests).
- Tests: `ctest --test-dir build -C Debug --output-on-failure` -> **131/131 passed (100%)**, 12.88 s.
- Coverage tool: not available (skipped). Linter/type checker: MSVC /W4 /WX in build (0 warnings reported by apply; build clean now).
- clang UBSan: NOT runnable on this host. See UNVERIFIED rows.

## Spec compliance matrix
| Requirement / scenario | Evidence | Status |
|---|---|---|
| R-GR-7 Extremes grid (|y|<1.0) | engine_extremes: parameter grid stays finite and within the ceiling (#69) | PASS |
| R-GR-7 Extremes worst-case subset | engine_extremes: 1 s of raw worst-case values stays finite and bounded (#72) | PASS |
| R-GR-7 Clamping equivalence | params: pitch 100 clamps to the same value as 24 (#57) | PASS |
| R-GR-7 Engine-level pitch clamp bit-identical | engine_extremes: pitch beyond +-24 st renders bit-identical (#70), jitter clamp (#71) | PASS |
| R-GR-7 Sample-rate range | engine_extremes: sample rates 22.05 kHz to 192 kHz (#73) | PASS |
| R-GR-7 Grain length scales with engine rate | engine_bounds: grain length follows size and engine sample rate (#61) | PASS |
| R-GR-7 Float-to-index casts overflow-safe (UBSan) | CI linux-sanitizers job green on ad0351c, maintainer-reported; not observed here | UNVERIFIED (maintainer-reported) |
| R-GR-10 Worst-case coherent input < ceiling, peak > 0.95 | engine_extremes: worst-case coherent DC saturates softly below the ceiling (#75), line 289 `> 0.95f`; engine_bounds line 266 | PASS |
| R-GR-10 Transparent below knee | soft_limiter: bit-exact transparency (#80), sub-threshold gives 0 (#81) | PASS |
| R-GR-10 Curve properties over sweep | soft_limiter: monotone, odd, below 1, slope <= 1 (#82) | PASS |
| R-GR-10 Out-of-domain inputs | soft_limiter: NaN, infinities, extremes (#83); subnormals (#84) | PASS |
| R-GR-10 Block-split invariance above knee | soft_limiter: block-split invariance holds above the knee (#85) | PASS |
| R-GR-10 Limiter adds no allocation | realtime: zero allocations while the limiter is engaged (#125) | PASS |
| R-GR-10 Standard constants (C=1.0, 0.7<=T<=0.9, static fn, stateless) | soft_limiter: ceiling 1.0 and knee in [0.7,0.9] (#79, static_assert); source: GranularEngine.cpp:14-26, process() applies softLimit to both channels | PASS |
| R-GR-10 Loud material saturates softly in Standalone [Manual] | Maintainer listened at heavy overlap (1000 ms, ~150 gr/s, gain 1.68) and said "sounds good". No explicit dBFS meter readings were stated. | UNVERIFIED for the "never above 0 dBFS on a meter" and "quiet unchanged" clauses (covered at runtime by Catch2 #75, #80); manual evidence is subjective only |
| R-PS-4 bounded by engine soft limiter, no extra plugin limiter | Source/plugin/PluginProcessor.cpp contains no limiter/clamp stage; bound proven in core by #69/#72/#75 | PASS (static + core tests) |
| R-PS-4 manual scenarios (silence, sound after load, source set on change, param sweep, multi-rate prepare) | Not exercised in this change; no behavior change beyond the ceiling | N/A (unchanged behavior; not re-run) |
| sample-source out-of-scope delta | Text-only; present in delta | PASS (static) |

Existing regression guard: engine_loudness (#76, #77) passes unchanged (within 3 dB, step slope).

## Strict TDD compliance
| Check | Result | Details |
|---|---|---|
| TDD evidence reported | PASS | TDD Cycle Evidence table in apply-progress #611 |
| Test files exist | PASS | test_soft_limiter.cpp (new), extremes, bounds, realtime edited |
| RED confirmed | PASS | RED run: 7 failed of 131 before the curve (ceiling 4); 3.1 then green |
| GREEN confirmed | PASS | 131/131 now, re-run by verifier |
| Triangulation | PASS | multi-input sweeps, both signs, nextafter edges |
| Safety net for modified files | PASS | 123/123 before refactor |
Assertion quality: no tautologies found (rg for CHECK(true)/REQUIRE(true) empty); probe + full + softLimit equality in coherent-DC test; peak > 0.95 and > knee guards prove limiter engagement. No CRITICAL.
Test layers: all Unit except the realtime alloc-counter case (Integration). Total 131 across the suite.

## Design coherence
Design decisions match code: knee T=0.8, rational curve `C - s^2/((a-T)+s)` with sign restore, pre-clamp at 64, flush at 1e-20 (NaN via negated comparison), `max(T, ...)` monotonicity guard, static `softLimit`, header comment updated (coherent sum about 9.3 at gain 2), README updated. No deviation. The earlier apply note that softLimit(-0.0f) returns +0 is resolved: code returns `y` for exact zero (preserves -0), matching "y == x" bit-exactly.

## Issues
CRITICAL: none.

WARNING
1. UBSan (task 3.2, R-GR-7 float-cast scenario) is maintainer-reported only; not observed. Archive should rely on the maintainer statement or a visible CI run link.
2. Manual Standalone check (task 4.4, R-GR-10 manual scenario) has no recorded dBFS readings; evidence is the maintainer's subjective approval ("sounds good") only.
3. Archive sync risk: delta specs use `###` headings for "Out of scope (explicit)" and "Assumptions (pending user confirmation)" under MODIFIED Requirements, but main specs use `##` (plugin-shell.md lines 107, 110; sample-source.md line 261) and these are not requirement blocks. A requirement-based merge will not find them, so they need a manual edit at archive. Also main plugin-shell spec lines 46, 108 (via R-PS-4) and 111 still say the +-4 ceiling / no soft limiter until merged.

SUGGESTION
1. apply-progress says test_soft_limiter.cpp has 8 cases; ctest lists 7 soft_limiter tests (#79-#85) plus 1 realtime test, which is the 8 new (131 = 123 + 8). Cosmetic.
2. Focused command in tasks.md (`ctest -R "soft_limiter|engine_"`) misses `realtime:` tests; use `soft_limiter|engine_|realtime`.
Housekeeping: tasks.md checkbox edits (3.2, 4.4) are uncommitted; commit before archive. Total commit diff is 803 insertions including openspec docs; code and tests are well under the 800-line review budget.

## Verdict
PASS WITH WARNINGS. All runnable evidence is green (131/131); remaining gaps are non-observable-from-host items (UBSan CI, manual dBFS) and an archive merge risk.
