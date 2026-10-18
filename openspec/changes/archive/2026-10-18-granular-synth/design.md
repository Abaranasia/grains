# Design: Granular Synth Foundation

## Technical Approach

Additive CMake build next to the untouched Projucer files. `grains_core` is a JUCE-free C++17 static lib; `grains_tests` (Catch2 v3) links it and runs under `ctest`. The engine is instance-based, allocates only in `prepare()`, and renders a global grain cloud from an immutable, non-owned `SampleBuffer`. Every source of randomness flows through one seeded `Rng`, so output is bit-identical for identical inputs, regardless of how the host splits blocks.

## Architecture Decisions

| Topic | Options / tradeoff | Decision |
|---|---|---|
| C++ standard | C++20 (`std::span`, concepts) vs C++17 (JUCE 9 minimum, matches proposal) | **C++17**, `EXTENSIONS OFF`. Use pointer+size instead of span; bump later if needed |
| Layout | Use `core/` at root vs `Source/core/` | **`Source/core/{include/grains,src}`**, `tests/`. Matches the proposal and JUCE's `Source/` convention |
| Test fetch | git tag vs vendored | **FetchContent `v3.8.1` (`GIT_SHALLOW`, `SYSTEM`)**, the version Pamplejuce uses. Needs network on the first configure |
| Plugin option | stub target vs fail | **`GRAINS_BUILD_PLUGIN=OFF`**. `ON` stops with `FATAL_ERROR "roadmap #1"` so nobody gets a half-built plugin |
| Interpolator | virtual call per sample vs static policy | **Static policy struct** (`LinearInterpolator::read`), render loop templated on it, `InterpolationMode` enum checked once per block. Adding Hermite later = new struct + enum value. Virtual calls rejected because they cost a call per sample |
| Rng | `std::mt19937` + distributions (output differs between standard libraries) vs own PCG32 | **Own PCG32**, concrete type passed by reference (injectable, no virtual) |
| Scheduling | spawn everything, then render vs event-ordered segments | **Event-ordered**: render active grains up to the next spawn offset, release finished grains, spawn, continue. Pool-full behavior then does not depend on block size. *Superseded by A4 (see Implementation amendments): spawns are rendered inside the scheduler callback; there is no offset list.* |
| Overflow | steal oldest vs skip | **Skip** (confirmed). The scheduler still advances, so timing is unchanged |
| Normalization | active-count `1/√N` (needs smoothing, pumps) vs from params | **`1/√max(1, density·sizeSec)`, fixed per grain at spawn**. Deterministic, no zipper noise on parameter changes |
| Boundaries | wrap / zero-pad / truncate vs shrink grain | **Shrink grain length to the available source span** (`len·rate`) before the window is computed, so every window completes (D1 confirmed). Roadmap #8 playback modes (wrap/loop, reverse, random position) must remain addable via the read/interpolator interface without rewriting the engine |
| Source ownership | engine owns vs non-owning | **`const SampleBuffer*`**. Roadmap #1 adds atomic publish + deferred free outside the engine |

## Data Flow

    EngineParams (POD, per block) ──clamp──→ GranularEngine::process(L, R, n)
                                                  │
    GrainScheduler(Rng&) ──spawn offsets──→ segment loop ──→ GrainPool[128]
                                                  │              │
    SampleBuffer (const*) ←─Interp::read── render grain × WindowTable(Hann LUT) × gains
                                                  └──→ overlap-add into L/R

> Superseded by A4 (see Implementation amendments): the scheduler does not emit a list of spawn offsets; the engine renders segments inside the `GrainScheduler::advance` callback.

Sample rate: `rate = 2^(st/12) · sourceSR / engineSR`; `prepare()` resets all state.

## File Changes

| File | Action | Description |
|---|---|---|
| `CMakeLists.txt` | Create | min 3.25, C++17, options, subdirs |
| `cmake/GrainsWarnings.cmake` | Create | `grains_set_warnings(tgt)` |
| `Source/core/CMakeLists.txt` | Create | `grains_core` STATIC, PUBLIC include dir |
| `Source/core/include/grains/{SampleBuffer,WindowTable,Rng,Grain,GrainPool,GrainScheduler,Interpolator,EngineParams,GranularEngine}.h` | Create | Public API |
| `Source/core/src/{SampleBuffer,WindowTable,GrainScheduler,GranularEngine}.cpp` | Create | Implementations |
| `tests/CMakeLists.txt` | Create | Catch2 fetch, `grains_tests`, `catch_discover_tests` |
| `tests/support/AllocationCounter.{h,cpp}` | Create | Replaces global `operator new` and counts allocations |
| `tests/test_*.cpp` | Create | window, rng, pool, scheduler, sample_buffer, params, engine_bounds, engine_determinism, engine_realtime |
| `.gitignore` | Modify | add `build/` |
| `openspec/config.yaml` | Modify | `runner: ctest`, unit layer true |

Compiler flags: MSVC `/W4 /WX /permissive- /utf-8 /Zc:__cplusplus /EHsc`, default `/fp:precise` (never `/fp:fast`). GCC/Clang `-Wall -Wextra -Wpedantic -Werror`.

## Interfaces / Contracts

```cpp
namespace grains {
class SampleBuffer { // immutable after construction
public: SampleBuffer(std::vector<float> l, std::vector<float> r /*empty=mono*/, double sr);
  int numChannels() const noexcept; int numFrames() const noexcept;
  double sampleRate() const noexcept; const float* channel(int c) const noexcept; };
class WindowTable { public: static constexpr int kSize = 4096;
  WindowTable(); float at(float phase01) const noexcept; }; // Hann, at(0)=at(1)=0
class Rng { public: explicit Rng(uint64_t seed) noexcept; void seed(uint64_t) noexcept;
  uint32_t nextU32() noexcept; float uniform01() noexcept; float bipolar() noexcept; };
struct Grain { double pos, rate; int length, age, startOffset; float gainL, gainR; bool active; };
class GrainPool { public: static constexpr int kCapacity = 128;
  Grain* acquire() noexcept; /*nullptr when full*/ void release(Grain&) noexcept;
  int activeCount() const noexcept; void reset() noexcept;
  template <class F> void forEachActive(F&&) noexcept; /*index order*/ };
class GrainScheduler { public: explicit GrainScheduler(Rng&) noexcept;
  void reset() noexcept; void configure(double sr, float density, float timingJitter) noexcept;
  template <class F> void advance(int numSamples, F&& onSpawn /*(int offset)*/) noexcept; };
struct LinearInterpolator { static float read(const float* d, int frames, double pos) noexcept; };
enum class InterpolationMode { Linear };
struct EngineParams { float grainSizeMs=80, density=20, position=0.5f, positionJitter=0,
  pitchSemitones=0, pitchJitterSemitones=0, timingJitter=0, panSpread=0, gain=1; };
class GranularEngine { public: void prepare(double sr, uint64_t seed); // may allocate
  void setSource(const SampleBuffer*) noexcept; void setParams(const EngineParams&) noexcept;
  void process(float* L, float* R, int n) noexcept; /*overwrites, zero alloc*/ };
}
```

**Clamping** (`setParams`; NaN gets the default): size 1–1000 ms (length ≥ 2 samples), density 0.1–1000 gr/s, position/jitters/panSpread 0–1, pitch ±24 st, pitch jitter 0–12 st, gain 0–2.

**Scheduler**: `samplesUntilNext` is a double. Each interval is `T·(1 + j·bipolar())` with T = sr/density, floored at 1 sample. Spawn offset = `floor`. Each block subtracts an integer count, which is exact for these magnitudes, so the timing does not change when blocks are split differently. The Rng is only called at spawn.

> Superseded by A1, A2 and A3 (see Implementation amendments): the scheduler tracks absolute time (`double nextTime_`, `int64 blockStart_`) instead of `samplesUntilNext`; it draws `bipolar()` exactly once per spawn, also at jitter 0; density 0 freezes the next spawn time.

**Spawn bounds**: clamp start to [0, frames−1]. Then `len = min(req, floor((frames−1−start)/rate))`. If `len < 2`, move start back to fit `2·rate`; if the source is still too short, skip the spawn. Linear reads of `i` and `i+1` therefore always stay ≤ frames−1.

> Confirmed as built by A15 (see Implementation amendments).

**Pan**: equal-power `cos/sin`, computed once at spawn. A mono source feeds both channels.

**Determinism**: same binary, flags, seed, params sequence and source give bit-identical output for any block split. Results are not guaranteed to match across compilers.

## Testing Strategy

| Layer | What | Approach |
|---|---|---|
| Unit | Window endpoints/symmetry/COLA at 50% overlap, Rng sequence, pool skip-on-full, scheduler spacing/drift over 10⁶ samples, jitter bounds, clamping | Pure Catch2, `Approx` with tolerances |
| Integration | Block-split invariance (1×4096 vs random splits), same seed gives the same output, no NaN/Inf at extremes (2-sample grains, 1000 gr/s, ±24 st), no reads past the buffer edges, no clicks (window fully applied when a grain is shrunk), RMS stays roughly constant across densities | Engine tests on synthetic buffers (DC, ramp, sine) |
| RT | Zero allocations in `process()`, including with a full pool | Counter in `AllocationCounter` (replaces `new/new[]/delete`, also `align_val_t` versions), counts only inside an RAII scope wrapped around the call, never across `REQUIRE` |

No golden files; tests use invariants and tolerances.

## Threat Matrix

N/A: there is no routing, shell, subprocess, VCS/PR automation, executable-file classification or process-integration boundary.

## Migration / Rollout

No migration required. Purely additive; Projucer files stay until roadmap #1.

**Line forecast**: core about 600 lines (headers 300, src 300), CMake about 90, tests about 450, so roughly **1100 lines**, above the 800 budget. Suggested chain: PR1 = harness + SampleBuffer/WindowTable/Rng/Pool/Scheduler + tests (about 550); PR2 = Interpolator/Params/Engine + engine tests (about 550).

> Superseded by A14 (see Implementation amendments): delivered as six slices (PR1, PR2, PR3a, PR3b, PR4a, PR4b).

## Open Questions

- [ ] Add `CMakePresets.json`? The VS2026 generator needs a recent CMake. Default: none; use the Ninja or VS generator documented in the README later.

## Implementation amendments

Recorded at archive time (2026-10-18) from the as-built code at `1e58e1e`. The sections above remain the original plan; where they differ, this section is authoritative.

| # | As-built decision | Rationale |
|---|---|---|
| A1 | Scheduler tracks absolute `double` time plus an `int64` block start; offset = `floor(nextTime) - blockStart`. | Gives bit-identical block-split invariance (R-GS-2, R-GR-6), because the per-block subtraction stays exact. |
| A2 | Exactly one `bipolar()` RNG draw per spawn, even when timing jitter is 0. | Keeps the RNG sequence aligned with spawn count, so the random stream does not depend on whether jitter is active (R-GS-5). |
| A3 | At density 0 the scheduler spawns nothing but still advances its block start every block; only the next spawn time is frozen. When density becomes non-zero again, a next spawn time that lies in the past is clamped forward to the current block start, so there is no backlog burst and the first spawn after re-enabling lands at offset 0 of that block. | Re-enabling density must not release a burst of backlog spawns. |
| A4 | The engine renders grains interleaved inside the `GrainScheduler::advance` callback; there is no per-block offset buffer. | Spawns per block are not bounded by a small constant (density up to 1000 gr/s over large blocks), so a fixed buffer would need an arbitrary cap. Interleaving keeps memory fixed. |
| A5 | Three RNG draws per spawn (position, pitch, pan), taken before any bounds check or pool-full skip. | A skipped spawn must leave the random sequence of later spawns unchanged (R-GS-6). |
| A6 | Final semitones (pitch + pitch jitter) are clamped to ±24 after jitter is applied. The output is bounded the same way in a final pass over each block: every sample that is NaN or has \|x\| < 1e-20 (`kFlushThreshold`) is flushed to 0.0, then the result is clamped to ±4 (`kOutputCeiling`). | Keeps the playback rate within the documented range (R-GR-7) even when pitch and jitter are both at their limits, and keeps the output finite, denormal-free and within the R-GR-7 ceiling. |
| A7 | `setSource()` resets the grain pool. | Prevents grains from holding read positions into a previously set source. The engine keeps only a non-owning pointer. |
| A8 | A null or empty source yields exact `0.0` output and does not advance the scheduler; the schedule resumes when a source is set. | Silence without a source needs no held state. The behaviour is explicit so tests can pin it. |
| A9 | `EngineParams::clamped()` holds all range clamping; `setParams()` calls it. | One place for limits, testable without an engine instance. |
| A10 | `GranularEngine::activeGrainCount() const noexcept` is a read-only accessor. | Lets tests assert pool saturation and active grains without reaching into internals. |
| A11 | The global `operator new`/`delete` replacement (`AllocationCounter`) lives in a separate `grains_realtime_tests` executable. | Replacing global operators in the main test binary would count Catch2 and unrelated test allocations. |
| A12 | Catch2 is fetched pinned by commit SHA (v3.8.1 = `2b60af89e23d28eefc081bc930831ee9d45ea58b`). `GRAINS_USE_SYSTEM_CATCH2=ON` is an explicit opt-in to an installed Catch2 3.x. | A moved tag cannot change the build; an installed Catch2 is never picked up implicitly. |
| A13 | The JUCE-free link guard (`grains_assert_juce_free`) is deferred with `cmake_language(DEFER DIRECTORY)` and inspects `LINK_LIBRARIES` and `INTERFACE_LINK_LIBRARIES` recursively. | Links added after the target is created must still be checked. An immediate check was a no-op at the point it ran (PR1 review finding). |
| A14 | Delivery split: PR3 is delivered as PR3a (`feat/granular-synth-03a-render`, `f33d347`) and PR3b (`feat/granular-synth-03b-engine`, `4e47e19`); PR4 as PR4a (`feat/granular-synth-04a-hardening`, `365cde1`) and PR4b (`feat/granular-synth-04b-realtime`, `1e58e1e`). | Each slice must stay within the 800-line review budget (forecast target under 400 lines per slice) and independently green under `ctest`. |
| A15 | Spawn bounds as built match the original "Spawn bounds" rule: start = position × (frames−1), with position clamped to [0, 1]; `len = min(requested, floor((frames−1−start)/rate))`; if `len < 2`, start moves back to `frames−1−2·rate` and `len = 2`; if that start is below 0 (source too short), the spawn is skipped. The three RNG draws (A5) are taken before this check. | Every window completes over the shrunk length, and linear reads stay within the source (R-GR-3). |

### Known limitations

The same set is listed in `verify-report.md` and `archive-report.md`.

1. **Coherent DC clipping.** A constant-1 source at density 200, size 1000 ms, gain 2 produces a pre-clamp peak of about 9.3 (about 4.6 at gain 1). The output is hard-clipped at ±4. This is R-GR-7 compliant (4.0 is the documented ceiling) but audible as distortion on DC-like material. A soft limiter is deferred to the plugin stage (roadmap #1), not an engine change. Test 70 pins this behaviour.
2. **Allocation counter scope.** `AllocationCounter` replaces the global `operator new`/`new[]` (and aligned forms) only. Direct `malloc`/`calloc` or third-party allocators are not counted.
3. **Realtime extremes test** does not assert that grains are active per combination; pool saturation is asserted only in the dedicated pool-full test.
4. **JUCE-free guard scope.** `grains_assert_juce_free` inspects only `LINK_LIBRARIES` and `INTERFACE_LINK_LIBRARIES` of `grains_core` (recursively), not include paths or sources. It was verified by a temporary mutation, not by a permanent test.
5. **Extremes grid duration.** Each combination runs 0.25 s instead of the 1 s in the R-GR-7 scenario. The pool saturates at about 0.13 s, so the worst case is reached.
6. **Weak R-GR-7 coverage.** Sample-rate grain-length scaling and the pitch +100 == +24 identity at engine level are only weakly covered (the latter is proven at params level).
7. **Determinism scope.** Bit-identical output holds per binary, compiler, and flags. Cross-compiler output is not guaranteed (design, Determinism).
8. **No sanitizer or coverage runs.** R-GR-3 out-of-bounds safety is evidenced by guarded buffers and exact-value tests, not by ASan/UBSan. No coverage tool was run.
