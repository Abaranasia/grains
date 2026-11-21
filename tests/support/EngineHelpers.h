// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

// Shared engine test helpers (offline rendering, deterministic test signals, output predicates).

#include <grains/GranularEngine.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace grains_test {

constexpr double kPi = 3.14159265358979323846;

struct Out {
    std::vector<float> L, R;
};

// Pre-fills the output with 7.0 so a test also proves that process() overwrites.
// block <= 0 renders in one block.
inline Out render(grains::GranularEngine& e, int n, int block = 0)
{
    Out o{std::vector<float>(static_cast<std::size_t>(n), 7.0f), std::vector<float>(static_cast<std::size_t>(n), 7.0f)};
    if (block <= 0) block = n;
    for (int done = 0; done < n; done += block) {
        const int b = std::min(block, n - done);
        e.process(o.L.data() + done, o.R.data() + done, b);
    }
    return o;
}

// Deterministic white-ish noise in [-1, 1).
inline std::vector<float> noise(int frames, std::uint32_t seed)
{
    std::vector<float> v(static_cast<std::size_t>(frames));
    std::uint32_t s = seed;
    for (float& x : v) {
        s = s * 1664525u + 1013904223u;
        x = static_cast<float>(static_cast<double>(s >> 8) / 8388608.0 - 1.0);
    }
    return v;
}

inline bool allFinite(const Out& o)
{
    for (std::size_t i = 0; i < o.L.size(); ++i)
        if (!std::isfinite(o.L[i]) || !std::isfinite(o.R[i])) return false;
    return true;
}

inline bool allZero(const Out& o)
{
    for (std::size_t i = 0; i < o.L.size(); ++i)
        if (o.L[i] != 0.0f || o.R[i] != 0.0f) return false;
    return true;
}

// Largest |sample| over both channels.
inline float peak(const Out& o)
{
    float m = 0.0f;
    for (std::size_t i = 0; i < o.L.size(); ++i) m = std::max({m, std::abs(o.L[i]), std::abs(o.R[i])});
    return m;
}

} // namespace grains_test
