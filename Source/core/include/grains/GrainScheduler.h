#pragma once

#include <grains/Rng.h>

#include <cmath>
#include <cstdint>

namespace grains {

// Decides WHEN grains start. Time is tracked as an absolute double sample index, so the spawn
// times never depend on how the host splits blocks. The Rng is drawn once per spawn (always,
// even at jitter 0) to pick the next interval.
class GrainScheduler {
public:
    explicit GrainScheduler(Rng& rng) noexcept : rng_(rng) {}

    // Restarts the timeline: the first spawn lands on sample 0. Parameters are kept.
    void reset() noexcept;

    // Values are clamped with grains::limits (NaN -> default). Call between blocks.
    void configure(double sampleRate, float density, float timingJitter) noexcept;

    // Calls onSpawn(offset) in time order for every spawn due in the next numSamples,
    // with 0 <= offset < numSamples.
    template <class F>
    void advance(int numSamples, F&& onSpawn)
    {
        if (numSamples <= 0) return;
        const std::int64_t blockEnd = blockStart_ + numSamples;
        if (density_ > 0.0f) {
            // After an idle (density 0) stretch the stored time is in the past: do not burst.
            if (nextTime_ < static_cast<double>(blockStart_)) nextTime_ = static_cast<double>(blockStart_);
            while (true) {
                const auto spawnSample = static_cast<std::int64_t>(std::floor(nextTime_));
                if (spawnSample >= blockEnd) break;
                onSpawn(static_cast<int>(spawnSample - blockStart_));
                nextTime_ += nextInterval();
            }
        }
        blockStart_ = blockEnd;
    }

private:
    double nextInterval() noexcept
    {
        const double period = sampleRate_ / static_cast<double>(density_);
        const double scaled = period * (1.0 + static_cast<double>(jitter_) * static_cast<double>(rng_.bipolar()));
        return scaled < 1.0 ? 1.0 : scaled;
    }

    Rng& rng_;
    double sampleRate_ = 48000.0;
    float density_ = 20.0f;
    float jitter_ = 0.0f;
    std::int64_t blockStart_ = 0; // absolute index of the first sample of the next block
    double nextTime_ = 0.0;       // absolute (fractional) time of the next spawn
};

} // namespace grains
