#pragma once

#include <cmath>

namespace grains {

// Selects the interpolation policy once per block. Adding Hermite later means one new policy
// struct plus one enum value; the render loop is templated on the policy (no virtual calls).
enum class InterpolationMode { Linear };

// Linear interpolation with an index guard: the position is clamped to [0, frames - 1] and the
// upper neighbour never exceeds frames - 1, so reads never leave the source.
struct LinearInterpolator {
    // frames <= 0 (or a null source) reads as silence. NaN positions read frame 0.
    static float read(const float* d, int frames, double pos) noexcept
    {
        if (d == nullptr || frames <= 0) return 0.0f;
        const double last = static_cast<double>(frames - 1);
        if (!(pos > 0.0)) return d[0];
        if (pos >= last) return d[frames - 1];
        const int i = static_cast<int>(pos);
        const float frac = static_cast<float>(pos - static_cast<double>(i));
        const float a = d[i];
        const float b = d[i + 1]; // i < frames - 1 here
        return a + (b - a) * frac;
    }
};

} // namespace grains
