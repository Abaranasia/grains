// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <grains/EngineParams.h>
#include <grains/Grain.h>
#include <grains/GrainPool.h>
#include <grains/GrainSnapshot.h>
#include <grains/GrainScheduler.h>
#include <grains/Interpolator.h>
#include <grains/Rng.h>
#include <grains/SampleBuffer.h>
#include <grains/TransportGate.h>
#include <grains/WindowTable.h>

#include <cstdint>

namespace grains {

// Transport state right after prepare(). Plugins always start Stopped; tests that expect sound use
// Playing, which skips the fade-in and keeps the render bit-identical to an ungated engine.
enum class InitialTransport : std::uint8_t { Stopped, Playing };

// Offline-friendly granular engine. Allocation-free: all storage is member data.
//
// Output bounds: every output sample passes softLimit(): flushed to 0 below kFlushThreshold
// (denormals, NaN), then the identity up to kLimiterKnee, then a rational knee towards
// kOutputCeiling, so |y| < kOutputCeiling (1.0, 0 dBFS) strictly. Per-grain gain is
// gain * 1/sqrt(max(1, density * sizeSeconds)) times the constant-power pan factor (<= 1). For
// uncorrelated material (noise, music) the overlap sum stays mostly below the knee and the
// limiter is transparent. Strongly correlated material (constant/DC-like sources) with many
// overlapping grains still sums linearly before the limiter: the sum grows like the number of
// overlapping grains (up to the 128-slot pool) while the normalisation only divides by
// sqrt(density * size). Measured with a constant-1 source, position 0, pan centre: density
// 200 gr/s, size 1000 ms, gain 2 reaches a pre-limit peak of about 9.3 (gain 1: about 4.6), which
// the limiter maps to about 0.995. Gain behaviour is deliberately unchanged (design: fixed
// per-grain gain); the soft ceiling is the documented bound of R-GR-7 and R-GR-10. The limiter
// is stateless (no latency), so block-split invariance holds.
//
// Segment loop: process() renders active grains up to each spawn offset, releases finished
// grains, then spawns. The scheduler callback does this inline (rendering interleaved inside the
// callback) rather than collecting offsets first, because the number of spawns per block is
// unbounded (up to one per sample) and a fixed offset buffer could overflow.
class GranularEngine {
public:
    static constexpr float kOutputCeiling = 1.0f;
    static constexpr float kFlushThreshold = 1.0e-20f;
    static constexpr int kMinGrainLength = 2;
    static constexpr float kLimiterKnee = 0.8f; // R-GR-10 T, 0.7 <= T <= 0.9
    // Inputs are clamped to this magnitude before the knee curve: with +-inf or FLT_MAX the
    // division would round to exactly kOutputCeiling, breaking |y| < 1. At 64 the output is ~0.9994.
    static constexpr float kLimiterPreClamp = 64.0f;
    static constexpr float kLimiterSpan = kOutputCeiling - kLimiterKnee;
    static constexpr float kLimiterSpanSq = kLimiterSpan * kLimiterSpan;

    // Output stage: +-0 -> y bit-exact; NaN and 0 < |y| < kFlushThreshold -> +0; |y| <= knee -> y bit-exact;
    // else soft knee, |result| < 1.
    static float softLimit(float y) noexcept;

    // The scheduler references rng_, so the engine is neither copyable nor movable.
    GranularEngine() noexcept : rng_(1), scheduler_(rng_) {}
    GranularEngine(const GranularEngine&) = delete;
    GranularEngine& operator=(const GranularEngine&) = delete;

    // Resets all state (pool, scheduler, rng, transport) and stores the seed that every Play
    // restarts from. Sample rate is clamped to [8000, 384000].
    void prepare(double sampleRate, std::uint64_t seed, InitialTransport initial = InitialTransport::Stopped) noexcept;

    // Level-triggered Play/Stop target, evaluated once at the start of the next process() call.
    // Play from Stopped is a full restart (rng reseed, fresh pool and scheduler) with a ~20 ms
    // fade-in; Stop fades out and then empties the pool; Stopped blocks are exact zeros, draw no
    // random numbers and render no grains. A Play during the fade-out waits for it to finish.
    void setPlaying(bool play) noexcept { target_ = play; }

    // Hard stop with no fade: transport Stopped, target false, pool and scheduler reset. The loaded
    // source is kept. setSource() itself never changes the transport.
    void stopNow() noexcept;

    // Read-only introspection (tests): the rng state, unchanged while Stopped (no draws).
    std::uint64_t rngState() const noexcept { return rng_.state(); }

    TransportGate::State transportState() const noexcept { return gate_.state(); }

    // Non-owning. The pointee must outlive its use and stay unchanged. Clears active grains.
    void setSource(const SampleBuffer* source) noexcept;

    // Clamps (NaN -> default) and applies from the next process() call.
    void setParams(const EngineParams& params) noexcept;

    // Overwrites L and R with n samples. Zero heap allocation.
    void process(float* left, float* right, int n) noexcept;

    // Read-only introspection (tests): grains currently playing, at most GrainPool::kCapacity.
    int activeGrainCount() const noexcept { return pool_.activeCount(); }

    // Display introspection (R-GR-11, R-WD-5): copies the active grains into `out` (count ==
    // activeGrainCount()). A pure reader of the pool, source and window: it never changes the
    // render, the RNG or the pool, and it never allocates. Call it between process() calls.
    void snapshotGrains(GrainSnapshot& out) const noexcept;

private:
    void renderSegment(float* left, float* right, int offset, int count) noexcept;
    void spawn() noexcept;

    Rng rng_;
    GrainScheduler scheduler_;
    GrainPool pool_;
    WindowTable window_;
    EngineParams params_;
    const SampleBuffer* source_ = nullptr;
    double sampleRate_ = limits::kSampleRateDefault;
    TransportGate gate_;
    std::uint64_t seed_ = 1;
    bool target_ = false;
};

} // namespace grains
