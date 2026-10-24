# Design: Granular Synth Follow-ups (a-f)

## Technical Approach

These are characterization tests and CI that prove the existing contract. The engine does not change unless `float-cast-overflow` finds a real bug, and then only if the fix is 30 lines or fewer. Tests a-d and f extend existing files and use `tests/support/EngineHelpers.h` as it is. Follow-up e adds one CMake option and one workflow. There are two commits: `test(engine): ...` (a-d, f, R-GR-7 delta) and `ci: ...` (e, R-BH-5 delta).

## Architecture Decisions

| Topic | Choice | Rejected | Rationale |
|---|---|---|---|
| `GRAINS_SANITIZE` placement | Root `CMakeLists.txt`, after `include(GrainsWarnings)` and before `add_subdirectory`. Uses directory-scoped `add_compile_options`/`add_link_options`. | Per-target inside `grains_set_warnings` | Catch2 (FetchContent, under `tests/`) and `grains_core` are both instrumented, so there are no partially instrumented STL/new paths. |
| Sanitizer on MSVC | `FATAL_ERROR` if ON under MSVC | Silent no-op; `/fsanitize=address` | A silent no-op would make a green job meaningless. MSVC ASan is out of scope. |
| Flags | `-fsanitize=address,undefined,float-cast-overflow -fno-sanitize-recover=all -fno-omit-frame-pointer` | UBSan defaults only | Float-to-int casts (`Interpolator.h`, `WindowTable.cpp`, `GranularEngine.cpp` length, scheduler) are what the extremes tests target. `-fno-sanitize-recover` makes any report fail ctest. |
| Linux build type | Debug (`-O0 -g`) | RelWithDebInfo | Matches the Windows baseline. It also stops the compiler from eliding the allocation in the counter self-test and gives readable sanitizer traces. |
| Linux toolchain | `ubuntu-24.04` + `clang++-18` (preinstalled), Ninja via apt | `ubuntu-latest` + generic `clang++` | An explicit image and compiler keep the job reproducible. Clang 18 has the `mmap_rnd_bits` fix. |
| `mmap_rnd_bits` | One step: `sudo sysctl -w vm.mmap_rnd_bits=28` | Rely on the clang version only | Costs one idempotent line and removes a runner-image drift risk. |
| Action pinning | Only `actions/checkout`, pinned to the full commit SHA of the current v5 release with a `# v5.x.y` comment. Resolve the SHA at apply time with `gh api repos/actions/checkout/git/ref/tags/<tag>`. | Major tag `@v5`; third-party setup actions | A SHA cannot move. Avoiding other actions keeps the trust surface to one. |
| Triggers | `push: branches: [main]` + `pull_request` (no branch filter) | Push on all branches | Matches the proposal. Avoids duplicate push and PR runs. Stacked PRs that target non-main bases are still covered. |
| (c) observation | `allocationsWhileProcessing` returns `RunStats{allocs, maxActive, peak}`. Each block is followed by an in-scope poll of `activeGrainCount()` and a `max(|l|, |r|)` scan of that block (plain loops, no allocation). | End-of-run `activeGrainCount()`; post-scope scan; "any non-zero sample" | 1 ms grains release inside a block, and the buffers are overwritten every block, so only per-block observation is reliable. A peak threshold matches the R-GR-8 scenario ("sounds", not just "non-zero"). |
| (c) assertions | Per combination: density 0 gives `maxActive == 0 && peak == 0.0f` (exact). Density > 0 gives `peak > kSoundThreshold` (`1e-3f`). Deterministic saturation check: size 1000, density 1000, jitter 0, position 0 gives `maxActive == GrainPool::kCapacity` for both pitches. | Aggregate "sounding > N" | The scheduler always spawns at sample 0 and the noise source fits every fallback, so per-combination results are deterministic. Fall back to an aggregate only if TDD shows instability. The threshold is a single constant, not tuned per combination. |
| (f) subset | 4 combinations using **raw** grid values: size `1.0e9f` × density `1.0e9f` × pitch {`-100`, `+100`} × position {`-1`, `2`}, all jitters at max, gain 2, rendered for 48000 samples (1 s) in blocks of 1000. Same assertions as the grid (finite, at most the ceiling, no subnormals), plus `!allZero`. | Post-clamp equivalents (1000/1000/±24/0..1); the full grid at 1 s (4x cost) | Raw values send the whole sanitize-then-cast path through ASan/UBSan, the way a host would. Clamping happens in `setParams` before any float-to-int cast, and the test proves that ordering holds at saturation. Release and reacquire at a saturated pool only start after about 1 s. This adds about 5% run time. |
| (d) sweep | Every table-entry boundary phase `k / float(WindowTable::kSize - 1)` for k = 0..kSize-1, plus `i / 100000.0f` for i = 0..100000, plus `std::nextafter(1.0f, 0.0f)`. Each value must be `std::isfinite(v) && 0 <= v && v <= 1`. | Random phases | Deterministic. The boundary phases hit `frac == 0` and the last-interval edge exactly; the dense sweep covers the interpolation in between. ASan would also flag an `i + 1` overrun. |
| (b) spawn-side clamp | Extra test: a constant-1 source of 8001 frames, density 0.1 (one grain at sample 0), size 1000 ms, position 0, pitch jitter 12, seeds 1..16. At +24, every grain must have `len >= 2000` (rate ≤ 4, so `floor(8000/4)`), and at least one seed must have exactly 2000. At -24, every grain must have `len <= 32000`, and at least one seed must have exactly 32000. `len` = last non-zero index + 1. | Rely on the ±100 test only | `setParams` already clamps ±100 to ±24, so the ±100 test never reaches the `spawn()` clamp. Pitch ±24 plus jitter is the only path into it. The grain length shrunk near the source end exposes the clamped rate directly. |
| ASan + AllocationCounter | Run the full ctest first. If they conflict, use `ctest -E "^realtime:"` in the Linux job only. | Excluding up front | The replacement new/delete should win over ASan's. Windows always runs the realtime tests. |

## Data Flow

    push(main)/PR ─> ci.yml ─┬─> windows: cmake -A x64 ─> build Debug ─> ctest -C Debug
                             └─> linux: sysctl ─> cmake -G Ninja Debug GRAINS_SANITIZE=ON
                                                ─> build (core + Catch2 instrumented) ─> ctest

## File Changes

| File | Action | Description |
|---|---|---|
| `tests/test_engine_bounds.cpp` | Modify | (a) Length table 221/441/480/960/1920. Source rate ≠ engine rate keeps 480. |
| `tests/test_engine_extremes.cpp` | Modify | (b) Pitch ±100 equals ±24 bit-identical, with a 24 vs 23 control, plus the spawn-side clamp test. (f) 1 s raw-value worst-case subset. Update the grid comment. |
| `tests/test_engine_realtime.cpp` | Modify | (c) `RunStats` helper and the new assertions. Existing callers use `.allocs`. |
| `tests/test_window.cpp` | Modify | (d) Boundary-phase and dense sweeps; values must be finite and in [0,1]. |
| `CMakeLists.txt` | Modify | `GRAINS_SANITIZE` option (about 12 lines). |
| `.github/workflows/ci.yml` | Create | Two jobs (about 60 lines). |
| `Source/core/**` | Conditional | Only for a `float-cast-overflow` finding of 30 lines or fewer. |

## Interfaces / Contracts

```cpp
struct RunStats { std::size_t allocs; int maxActive; float peak; };
RunStats allocationsWhileProcessing(GranularEngine&, int block, int blocks);
```

CI workflow settings: `on: push: branches: [main]` plus `pull_request`. `permissions: contents: read`. `concurrency: {group: ${{ github.workflow }}-${{ github.ref }}, cancel-in-progress: true}`. `timeout-minutes: 30`. `-DGRAINS_BUILD_PLUGIN=OFF`. `ctest --test-dir build --output-on-failure`. Linux env: `ASAN_OPTIONS=detect_leaks=1:abort_on_error=1`, `UBSAN_OPTIONS=print_stacktrace=1`.

## Testing Strategy (Strict TDD)

The behavior already exists, so each RED step is shown by a temporary sabotage that is never committed. The order is: write the test, apply the sabotage and confirm the test fails, revert the sabotage and confirm it passes.

| Test | Sabotage that must turn it RED |
|---|---|
| (a) | `+ 0.5` changed to `+ 0.0` in `requested`, or length computed from `srcSr` |
| (b) ±100 | Skip the pitch clamp in `EngineParams::clamped()` |
| (b) spawn clamp | Remove `clampTo` around `semis` in `GranularEngine::spawn()` |
| (c) | `spawn()` returns early, so the peak threshold and saturation fail |
| (d) | Interpolation multiplied by `1.0001f` |
| (f) | `finalize()` without the ceiling clamp |
| (e) | First CI run on the PR. A local Linux/WSL run is optional. Configuring MSVC with `-DGRAINS_SANITIZE=ON` must fail. |

Use literal expectations in (a) so the test does not repeat the engine's own formula.

## Threat Matrix

The workflow runs fixed shell commands on GitHub-hosted runners. It does no VCS writes, PR automation or path classification.

| Boundary | Applicability |
|---|---|
| Documentation-like paths | N/A: nothing classifies or executes files by path |
| Git repository selection | N/A: only `actions/checkout` runs, on the default ref |
| Commit state / Push state | N/A: read-only job, never commits or pushes |
| PR commands | N/A: no `gh` or PR mutation. The workflow uses `pull_request` (not `pull_request_target`), no secrets, and `contents: read`. |

## Migration / Rollout

No migration is required. To roll back, revert the `ci:` commit (it removes the workflow and the option) and/or the `test:` commit. Making CI a required check comes later and is not part of this PR.

## Open Questions

- [ ] The exact checkout SHA is resolved at apply time and does not block.
- [ ] (a) The 22.05 kHz case sits on a .5 tie (220.5). The literal 221 relies on `0.01` being stored slightly above its exact value. TDD confirms this. If it fails, record the observed value as an engine rounding finding. Do not loosen the test.
- [ ] (b) The spawn-clamp test needs at least one seed in 1..16 with a positive pitch draw (for +24) and one with a negative draw (for -24). If one is missing, widen the seed range; do not drop the exact-bound check.
