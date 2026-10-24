# Explore: granular-synth-followups (a-f)

Source: follow-ups a-f from `openspec/changes/archive/2026-10-18-granular-synth/archive-report.md`.

## Current state

- No `.codegraph/`, no `.github/`, no CI.
- `CMakeLists.txt` (root): `GRAINS_BUILD_PLUGIN` defaults OFF (FATAL_ERROR if ON), so CI needs no JUCE; pass `-DGRAINS_BUILD_PLUGIN=OFF` explicitly.
- `cmake/GrainsWarnings.cmake`: MSVC `/W4 /WX`; otherwise `-Wall -Wextra -Wpedantic -Werror`, applied per target (Catch2 is fetched as SYSTEM and is unaffected).
- `tests/CMakeLists.txt`: FetchContent Catch2 v3.8.1 at a pinned SHA (`GRAINS_USE_SYSTEM_CATCH2` opt-in); two executables, `grains_tests` and `grains_realtime_tests`.
- `Source/core/CMakeLists.txt`: JUCE-free guard.

## Per follow-up

### (a) Grain length scales with sample rate (~35 lines)
- File: `tests/test_engine_bounds.cpp` (or extremes).
- DC source 1.0 at the engine sample rate (rate 1), density 0.1 (exactly one grain at sample 0), size 10 ms, position 0.
- Length = last non-zero index + 1 must equal `floor(0.010 * sr + 0.5)` for sr 22050/44100/48000/96000/192000 (221, 441, 480, 960, 1920).
- Extra case: source sample rate != engine sample rate; length is unchanged.

### (b) Engine pitch +100 == +24 (~35 lines)
- File: `tests/test_engine_extremes.cpp` (or determinism).
- Two engines, same seed, noise source, pitch jitter on; compare +100 vs +24 and -100 vs -24.
- L/R bit-identical and not all zero; negative control 24 vs 23 must differ.
- The params-level proof already exists in `tests/test_params.cpp`.

### (c) Realtime extremes active-grain assert (~30 lines)
- File: `tests/test_engine_realtime.cpp`.
- An end-of-run `activeGrainCount()` check is brittle: 1 ms grains release inside the block, and the density 0.1 grain at sample 0 is gone after block 1.
- Make `allocationsWhileProcessing` return `{allocs, maxActive, sounded}`: `maxActive` polled after each block inside the scope (non-allocating), `sounded` from a post-scope scan of the output buffers.
- Density 0: `maxActive == 0` and silence. Density > 0: aggregate "sounding > N" threshold like the extremes grid, or per-combo if empirically stable. Verify under TDD.

### (d) Window values in [0,1] (~12 lines)
- File: `tests/test_window.cpp`.
- Table is private: sweep ~100k phases in [0,1] through `at()` and `REQUIRE(0 <= v && v <= 1)`.
- Table peak is ~1 - 3.7e-8 (rounds to <= 1); only the interpolation `a + (b - a) * frac` could produce a one-ulp overshoot.

### (e) CI, option A (~85-95 lines)
- New `.github/workflows/ci.yml` (~70 lines): triggers push to main and pull_request; `permissions: contents: read`; concurrency cancels superseded runs.
  - Windows: windows-latest, MSVC, `-A x64`, `--config Debug`, `ctest -C Debug` (matches verified baseline).
  - Linux: ubuntu-latest, clang, Ninja, `CMAKE_BUILD_TYPE=Debug`, `GRAINS_SANITIZE=ON`, `GRAINS_BUILD_PLUGIN=OFF`, `ctest --output-on-failure`.
- Root `CMakeLists.txt`: option `GRAINS_SANITIZE` (OFF, non-MSVC only), `add_compile_options`/`add_link_options` with `-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer`, set before `add_subdirectory` so Catch2 is instrumented too (~12 lines).
- Optional: harden the AllocationCounter self-test (~6 lines).
- Open decision: `-fsanitize=float-cast-overflow` (not in UBSan defaults; may surface real findings).

### (f) Extremes duration: hybrid recommended (~25 test + ~6 spec lines)
- Keep the 0.25 s grid; add a worst-case subset (~4 combos: size 1000, density 1000, pitch -24/+24, max jitters, gain 2) rendered for 1 s.
- Amend R-GR-7 "Extremes grid" in `openspec/specs/grain-rendering/spec.md`: every combination >= 0.25 s (pool saturates at ~0.13 s = 128/1000) plus a 1 s worst-case subset.
- Cost: grid 300 x 12000 = 3.6M samples; full 1 s grid = 14.4M (4x); subset adds ~192k (+5%).
- Only 1 s exercises release/reacquire churn at a saturated pool: 1000 ms grains first finish at ~1 s; the 20000-frame source caps length at ~0.42 s at rate 1 but ~1 s at pitch -24.

## Estimate

Tests ~140, CI + CMake ~85-95, spec/docs ~10-20 (incl. proposed R-BH-5 CI/sanitizer requirement). Total ~230-260 changed lines: single PR, two commits (tests; CI).

## Portability risks (unverified until first CI run)

1. AllocationCounter + ASan: the counter replaces all 8 new and 12 delete variants, so ASan's operator new/delete member should not be pulled (no duplicate symbols); malloc/aligned_alloc/free stay intercepted; only alloc-dealloc-mismatch detection is lost. Fallback: `ctest -E realtime` in the ASan job, keep it on Windows.
2. Allocation elision: clang/GCC at -O1+ may elide `new int(1)`/`delete` in the counter self-test (`CHECK(inside >= 2)`). Mitigation: Linux Debug (-O0), optionally escape the pointer.
3. `-Werror` under clang never exercised; budget 0-20 lines of fixes. MSVC runner version drift may add new `/W4` warnings.
4. Headers rely partly on transitive includes (`std::size_t`); libstdc++ vs MSVC STL. Low risk.
5. Newer Ubuntu runners with high `vm.mmap_rnd_bits` need clang >= 18 or `sudo sysctl vm.mmap_rnd_bits=28`.
6. Determinism is per binary/compiler: never compare outputs across jobs.
7. `catch_discover_tests` runs executables at build time under sanitizers; fine on hosted runners.
