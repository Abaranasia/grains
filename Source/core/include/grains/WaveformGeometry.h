// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cmath>

namespace grains {

// R-WD-2: pixel <-> normalised position mapping for the waveform view. Header-only and JUCE-free.
// Non-finite input is treated as 0 and every result is finite and inside its documented range.

namespace detail {
inline float finiteOr0(float v) noexcept { return std::isfinite(v) ? v : 0.0f; }
inline float clamp01(float v) noexcept { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }
inline bool usableWidth(float width) noexcept { return std::isfinite(width) && width > 0.0f; }
} // namespace detail

// x / width clamped to [0, 1]. A width <= 0 (or non-finite) gives 0.
inline float xToPosition(float x, float width) noexcept
{
    if (!detail::usableWidth(width)) return 0.0f;
    return detail::clamp01(detail::finiteOr0(x) / width);
}

// p * width with p clamped to [0, 1]. A width <= 0 (or non-finite) gives 0.
inline float positionToX(float p, float width) noexcept
{
    if (!detail::usableWidth(width)) return 0.0f;
    return detail::clamp01(detail::finiteOr0(p)) * width;
}

struct Band {
    float lo;
    float hi;
};

// [position - jitter, position + jitter] clamped to [0, 1]; lo <= hi always (a negative jitter is
// treated as 0).
inline Band jitterBand(float position, float jitter) noexcept
{
    const float p = detail::clamp01(detail::finiteOr0(position));
    const float j = detail::finiteOr0(jitter) < 0.0f ? 0.0f : detail::finiteOr0(jitter);
    return {detail::clamp01(p - j), detail::clamp01(p + j)};
}

} // namespace grains
