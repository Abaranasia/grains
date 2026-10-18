#include <grains/GrainScheduler.h>

#include <grains/ParamLimits.h>

namespace grains {

void GrainScheduler::reset() noexcept
{
    blockStart_ = 0;
    nextTime_ = 0.0;
}

void GrainScheduler::configure(double sampleRate, float density, float timingJitter) noexcept
{
    sampleRate_ = limits::sanitizeSampleRate(sampleRate);
    density_ = limits::sanitizeDensity(density);
    jitter_ = limits::sanitizeUnit(timingJitter, 0.0f);
}

} // namespace grains
