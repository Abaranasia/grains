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
| Scheduling | spawn everything, then render vs event-ordered segments | **Event-ordered**: render active grains up to the next spawn offset, release finished grains, spawn, continue. Pool-full behavior then does not depend on block size |
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

**Spawn bounds**: clamp start to [0, frames−1]. Then `len = min(req, floor((frames−1−start)/rate))`. If `len < 2`, move start back to fit `2·rate`; if the source is still too short, skip the spawn. Linear reads of `i` and `i+1` therefore always stay ≤ frames−1.

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

## Open Questions

- [ ] Add `CMakePresets.json`? The VS2026 generator needs a recent CMake. Default: none; use the Ninja or VS generator documented in the README later.
