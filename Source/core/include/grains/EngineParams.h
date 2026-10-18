#pragma once

#include <grains/ParamLimits.h>

namespace grains {

// Per-block engine parameters (plain data). GranularEngine::setParams clamps with clamped().
struct EngineParams {
    float grainSizeMs = limits::kSizeMsDefault;  // 1-1000 ms
    float density = limits::kDensityDefault;     // grains/s, 0 = no spawns, max 1000
    float position = 0.5f;                       // 0-1 of the source length
    float positionJitter = 0.0f;                 // 0-1
    float pitchSemitones = 0.0f;                 // +-24
    float pitchJitterSemitones = 0.0f;           // 0-12
    float timingJitter = 0.0f;                   // 0-1
    float panSpread = 0.0f;                      // 0-1, 0 = centre
    float gain = limits::kGainDefault;           // 0-2

    // Every field clamped to its documented range; NaN becomes the field default.
    EngineParams clamped() const noexcept
    {
        const EngineParams d;
        EngineParams c;
        c.grainSizeMs = limits::sanitizeSizeMs(grainSizeMs);
        c.density = limits::sanitizeDensity(density);
        c.position = limits::sanitizeUnit(position, d.position);
        c.positionJitter = limits::sanitizeUnit(positionJitter, d.positionJitter);
        c.pitchSemitones = limits::sanitizePitchSemitones(pitchSemitones);
        c.pitchJitterSemitones = limits::sanitizePitchJitterSemitones(pitchJitterSemitones);
        c.timingJitter = limits::sanitizeUnit(timingJitter, d.timingJitter);
        c.panSpread = limits::sanitizeUnit(panSpread, d.panSpread);
        c.gain = limits::sanitizeGain(gain);
        return c;
    }
};

} // namespace grains
