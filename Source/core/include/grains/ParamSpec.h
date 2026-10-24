#pragma once

#include <grains/EngineParams.h>
#include <grains/ParamLimits.h>

#include <array>
#include <cmath>

namespace grains::params {

// Slot order of the user-facing parameters; also the index into kSpecs.
enum class Id : int {
    GrainSize,
    Density,
    Position,
    PositionJitter,
    Pitch,
    PitchJitter,
    TimingJitter,
    PanSpread,
    Gain,
    Count
};

inline constexpr int kCount = static_cast<int>(Id::Count);

// Static description of one parameter. `id` is the stable host-automation identity.
struct Spec {
    const char* id;
    const char* name;
    const char* unit;
    float min;
    float max;
    float def;
    float skew; // 1 = linear, power-curve exponent: plain = min + span * n^(1/skew)
};

namespace detail {
inline constexpr EngineParams kDefaults{};
inline constexpr float kSkewWide = 0.3f; // wide log-like ranges (size, density)
} // namespace detail

// Indexed by Id. Ranges and defaults derive from grains::limits and EngineParams{}.
// IDs are saved in host state: never rename them once released.
inline constexpr std::array<Spec, kCount> kSpecs{{
    {"grainSize", "Grain Size", "ms", limits::kSizeMsMin, limits::kSizeMsMax,
     detail::kDefaults.grainSizeMs, detail::kSkewWide},
    {"density", "Density", "gr/s", 0.0f, limits::kDensityMax, detail::kDefaults.density,
     detail::kSkewWide},
    {"position", "Position", "", 0.0f, 1.0f, detail::kDefaults.position, 1.0f},
    {"positionJitter", "Position Jitter", "", 0.0f, 1.0f, detail::kDefaults.positionJitter, 1.0f},
    {"pitch", "Pitch", "st", -limits::kPitchSemitonesMax, limits::kPitchSemitonesMax,
     detail::kDefaults.pitchSemitones, 1.0f},
    {"pitchJitter", "Pitch Jitter", "st", 0.0f, limits::kPitchJitterSemitonesMax,
     detail::kDefaults.pitchJitterSemitones, 1.0f},
    {"timingJitter", "Timing Jitter", "", 0.0f, 1.0f, detail::kDefaults.timingJitter, 1.0f},
    {"panSpread", "Pan Spread", "", 0.0f, 1.0f, detail::kDefaults.panSpread, 1.0f},
    {"gain", "Gain", "", 0.0f, limits::kGainMax, detail::kDefaults.gain, 1.0f},
}};

// Plain value -> [0, 1] knob position. Clamps to [min, max]; NaN is treated as the default.
// Same maths as a skewed normalisable range (as used by common plugin frameworks).
inline float toNormalised(const Spec& s, float plain) noexcept
{
    if (std::isnan(plain)) plain = s.def;
    if (plain <= s.min) return 0.0f;
    if (plain >= s.max) return 1.0f;
    const float n = (plain - s.min) / (s.max - s.min);
    return std::pow(n, s.skew);
}

// [0, 1] knob position -> plain value. Clamps to [0, 1]; NaN yields the default.
inline float fromNormalised(const Spec& s, float norm) noexcept
{
    if (std::isnan(norm)) return s.def;
    if (norm <= 0.0f) return s.min;
    if (norm >= 1.0f) return s.max;
    return s.min + (s.max - s.min) * std::exp(std::log(norm) / s.skew);
}

// Plain values (indexed by Id) -> engine parameters, already clamped. Density in (0, kDensityMin)
// snaps up to kDensityMin; only an exact 0 means "off".
inline EngineParams toEngineParams(const std::array<float, kCount>& plain) noexcept
{
    auto at = [&plain](Id id) { return plain[static_cast<std::size_t>(id)]; };
    EngineParams e;
    e.grainSizeMs = at(Id::GrainSize);
    e.density = at(Id::Density);
    e.position = at(Id::Position);
    e.positionJitter = at(Id::PositionJitter);
    e.pitchSemitones = at(Id::Pitch);
    e.pitchJitterSemitones = at(Id::PitchJitter);
    e.timingJitter = at(Id::TimingJitter);
    e.panSpread = at(Id::PanSpread);
    e.gain = at(Id::Gain);
    return e.clamped();
}

} // namespace grains::params
