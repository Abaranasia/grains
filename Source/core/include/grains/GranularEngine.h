#pragma once

#include <grains/EngineParams.h>
#include <grains/Grain.h>
#include <grains/GrainPool.h>
#include <grains/GrainScheduler.h>
#include <grains/Interpolator.h>
#include <grains/Rng.h>
#include <grains/SampleBuffer.h>
#include <grains/WindowTable.h>

#include <cstdint>

namespace grains {

// Offline-friendly granular engine. Allocation-free: all storage is member data.
//
// Output bounds: every output sample is flushed to 0 below kFlushThreshold (denormals, NaN) and
// clamped to +-kOutputCeiling. Per-grain gain is gain * 1/sqrt(max(1, density * sizeSeconds))
// times the constant-power pan factor (<= 1), so for inputs in [-1, 1] the clamp is a safety net
// and not part of normal operation.
//
// Segment loop: process() renders active grains up to each spawn offset, releases finished
// grains, then spawns. The scheduler callback does this inline (rendering interleaved inside the
// callback) rather than collecting offsets first, because the number of spawns per block is
// unbounded (up to one per sample) and a fixed offset buffer could overflow.
class GranularEngine {
public:
    static constexpr float kOutputCeiling = 4.0f;
    static constexpr float kFlushThreshold = 1.0e-20f;
    static constexpr int kMinGrainLength = 2;

    // The scheduler references rng_, so the engine is neither copyable nor movable.
    GranularEngine() noexcept : rng_(1), scheduler_(rng_) {}
    GranularEngine(const GranularEngine&) = delete;
    GranularEngine& operator=(const GranularEngine&) = delete;

    // Resets all state (pool, scheduler, rng). Sample rate is clamped to [8000, 384000].
    void prepare(double sampleRate, std::uint64_t seed) noexcept;

    // Non-owning. The pointee must outlive its use and stay unchanged. Clears active grains.
    void setSource(const SampleBuffer* source) noexcept;

    // Clamps (NaN -> default) and applies from the next process() call.
    void setParams(const EngineParams& params) noexcept;

    // Overwrites L and R with n samples. Zero heap allocation.
    void process(float* left, float* right, int n) noexcept;

private:
    void renderSegment(float* left, float* right, int offset, int count) noexcept;
    void spawn(int offset) noexcept;

    Rng rng_;
    GrainScheduler scheduler_;
    GrainPool pool_;
    WindowTable window_;
    EngineParams params_;
    const SampleBuffer* source_ = nullptr;
    double sampleRate_ = limits::kSampleRateDefault;
};

} // namespace grains
