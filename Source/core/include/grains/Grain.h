#pragma once

#include <grains/WindowTable.h>

#include <algorithm>

namespace grains {

// Plain per-grain state.
struct Grain {
    double pos = 0.0;     // fractional read position in source frames
    double rate = 1.0;    // source frames advanced per output sample
    int length = 0;       // grain length in output samples
    int age = 0;          // output samples rendered so far
    int startOffset = 0;  // sample offset inside the current block where rendering starts
    float gainL = 0.0f;
    float gainR = 0.0f;
    bool active = false;
};

// Adds up to `count` samples of grain `g` into outL/outR (overlap-add, buffers are not cleared)
// and advances g.age / g.pos. Output = Interp::read(src, pos) * window(age/length) * gain.
// srcR may alias srcL (mono source): then the value is read once and fed to both channels, so a
// mono source is bit-identical to a stereo source with equal channels. The caller guarantees the
// grain was spawned so its whole span fits in the source; the interpolator still clamps.
// Returns the number of samples rendered (0 once age reaches length).
template <class Interp>
int renderGrain(Grain& g, const float* srcL, const float* srcR, int frames, const WindowTable& window,
                float* outL, float* outR, int count) noexcept
{
    const int n = std::min(count, g.length - g.age);
    if (n <= 0) return 0;
    const float len = static_cast<float>(g.length);
    const bool mono = srcR == srcL;
    for (int i = 0; i < n; ++i) {
        const float w = window.at(static_cast<float>(g.age) / len);
        const float l = Interp::read(srcL, frames, g.pos) * w;
        const float r = mono ? l : Interp::read(srcR, frames, g.pos) * w;
        outL[i] += l * g.gainL;
        outR[i] += r * g.gainR;
        ++g.age;
        g.pos += g.rate;
    }
    return n;
}

} // namespace grains
