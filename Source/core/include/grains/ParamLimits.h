#pragma once

#include <cmath>

namespace grains::limits {

// Parameter ranges shared by the scheduler and the engine (single source of truth).
constexpr float kDensityMin = 0.1f;   // gr/s; values below clamp up, 0 means "no spawns"
constexpr float kDensityMax = 1000.0f;
constexpr float kDensityDefault = 20.0f;

constexpr float kSizeMsMin = 1.0f;
constexpr float kSizeMsMax = 1000.0f;
constexpr float kSizeMsDefault = 80.0f;

constexpr float kPitchSemitonesMax = 24.0f;
constexpr float kPitchJitterSemitonesMax = 12.0f;
constexpr float kGainMax = 2.0f;
constexpr float kGainDefault = 1.0f;

constexpr double kSampleRateMin = 8000.0;
constexpr double kSampleRateMax = 384000.0;
constexpr double kSampleRateDefault = 48000.0;

namespace detail {
template <class T>
constexpr T clampTo(T v, T lo, T hi) noexcept
{
    return v < lo ? lo : (v > hi ? hi : v);
}
} // namespace detail

// Density in grains/second. NaN -> default, <= 0 -> 0 (off), (0, min) -> min, > max -> max.
inline float sanitizeDensity(float v) noexcept
{
    if (std::isnan(v)) return kDensityDefault;
    if (v <= 0.0f) return 0.0f;
    return detail::clampTo(v, kDensityMin, kDensityMax);
}

inline float sanitizeSizeMs(float v) noexcept
{
    if (std::isnan(v)) return kSizeMsDefault;
    return detail::clampTo(v, kSizeMsMin, kSizeMsMax);
}

// Position, position jitter, timing jitter, pan spread: [0, 1].
inline float sanitizeUnit(float v, float nanDefault) noexcept
{
    if (std::isnan(v)) return nanDefault;
    return detail::clampTo(v, 0.0f, 1.0f);
}

inline float sanitizePitchSemitones(float v) noexcept
{
    if (std::isnan(v)) return 0.0f;
    return detail::clampTo(v, -kPitchSemitonesMax, kPitchSemitonesMax);
}

inline float sanitizePitchJitterSemitones(float v) noexcept
{
    if (std::isnan(v)) return 0.0f;
    return detail::clampTo(v, 0.0f, kPitchJitterSemitonesMax);
}

inline float sanitizeGain(float v) noexcept
{
    if (std::isnan(v)) return kGainDefault;
    return detail::clampTo(v, 0.0f, kGainMax);
}

inline double sanitizeSampleRate(double v) noexcept
{
    if (std::isnan(v)) return kSampleRateDefault;
    return detail::clampTo(v, kSampleRateMin, kSampleRateMax);
}

} // namespace grains::limits
