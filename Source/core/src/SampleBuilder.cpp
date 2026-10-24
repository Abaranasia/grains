// SPDX-License-Identifier: AGPL-3.0-or-later
#include <grains/SampleBuilder.h>

#include <algorithm>
#include <cmath>
#include <climits>
#include <new>
#include <utility>
#include <vector>

namespace grains {

std::size_t maxSourceFrames(double sampleRate) noexcept
{
    if (!std::isfinite(sampleRate) || sampleRate <= 0.0) return 0;
    const double frames = std::floor(kMaxSourceSeconds * sampleRate);
    return frames >= static_cast<double>(INT_MAX) ? static_cast<std::size_t>(INT_MAX) : static_cast<std::size_t>(frames);
}

BuildResult buildSampleBuffer(const ChannelView* channels, std::size_t numChannels, double sampleRate) noexcept
{
    BuildResult result;
    if (channels == nullptr || numChannels == 0) {
        result.error = BuildError::NoChannels;
        return result;
    }
    for (std::size_t c = 0; c < numChannels; ++c) {
        if (channels[c].data == nullptr) {
            result.error = BuildError::NoChannels;
            return result;
        }
    }
    const std::size_t cap = maxSourceFrames(sampleRate);
    if (cap == 0) {
        result.error = BuildError::BadSampleRate;
        return result;
    }
    const std::size_t frames = channels[0].frames;
    for (std::size_t c = 1; c < numChannels; ++c) {
        if (channels[c].frames != frames) {
            result.error = BuildError::MismatchedLengths;
            return result;
        }
    }
    if (frames == 0) {
        result.error = BuildError::NoFrames;
        return result;
    }
    const std::size_t keep = std::min(frames, cap);
    try {
        std::vector<float> left(channels[0].data, channels[0].data + keep);
        std::vector<float> right;
        if (numChannels > 1) right.assign(channels[1].data, channels[1].data + keep);
        result.buffer = std::make_unique<SampleBuffer>(std::move(left), std::move(right), sampleRate);
        result.truncated = keep < frames;
    } catch (const std::bad_alloc&) {
        result.buffer.reset();
        result.error = BuildError::OutOfMemory;
    }
    return result;
}

} // namespace grains
