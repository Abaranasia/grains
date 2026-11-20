// SPDX-License-Identifier: AGPL-3.0-or-later
#include <grains/SampleBuilder.h>
#include <grains/Resampler.h>

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

namespace {

// Shared argument checks. Returns BuildError::None and the kept (input-capped) frame count when valid.
BuildError validate(const ChannelView* channels, std::size_t numChannels, double sampleRate, std::size_t& keep,
                    std::size_t& frames) noexcept
{
    if (channels == nullptr || numChannels == 0) return BuildError::NoChannels;
    for (std::size_t c = 0; c < numChannels; ++c)
        if (channels[c].data == nullptr) return BuildError::NoChannels;
    const std::size_t cap = maxSourceFrames(sampleRate);
    if (cap == 0) return BuildError::BadSampleRate;
    frames = channels[0].frames;
    for (std::size_t c = 1; c < numChannels; ++c)
        if (channels[c].frames != frames) return BuildError::MismatchedLengths;
    if (frames == 0) return BuildError::NoFrames;
    keep = std::min(frames, cap);
    return BuildError::None;
}

BuildResult fail(BuildError e) noexcept
{
    BuildResult r;
    r.error = e;
    return r;
}

} // namespace

BuildResult buildSampleBuffer(const ChannelView* channels, std::size_t numChannels, double sampleRate) noexcept
{
    std::size_t keep = 0, frames = 0;
    if (const BuildError e = validate(channels, numChannels, sampleRate, keep, frames); e != BuildError::None)
        return fail(e);
    BuildResult result;
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

BuildResult buildSampleBuffer(const ChannelView* channels, std::size_t numChannels, double fileRate,
                              double targetRate, const std::function<bool()>& shouldAbort) noexcept
{
    const std::size_t outCap = maxSourceFrames(targetRate);
    if (outCap == 0) return fail(BuildError::BadSampleRate); // also covers NaN, Inf and non-positive targets
    std::size_t keep = 0, frames = 0;
    if (const BuildError e = validate(channels, numChannels, fileRate, keep, frames); e != BuildError::None)
        return fail(e);
    if (targetRate == fileRate) return buildSampleBuffer(channels, numChannels, fileRate);

    ChannelView capped[2] = {{channels[0].data, keep}, {numChannels > 1 ? channels[1].data : nullptr, keep}};
    ResampleOptions options;
    options.maxFrames = outCap;
    ResampleResult rs = resample(capped, std::min<std::size_t>(numChannels, 2), fileRate, targetRate, shouldAbort, options);
    switch (rs.status) {
    case ResampleStatus::Ok: break;
    case ResampleStatus::Aborted: return fail(BuildError::Aborted);
    case ResampleStatus::OutOfMemory: return fail(BuildError::OutOfMemory);
    case ResampleStatus::InvalidArgument: return fail(BuildError::BadSampleRate); // ratio beyond the kernel limit
    }

    BuildResult result;
    try {
        std::vector<float> right;
        if (rs.channels.size() > 1) right = std::move(rs.channels[1]);
        result.buffer = std::make_unique<SampleBuffer>(std::move(rs.channels[0]), std::move(right), targetRate);
        result.truncated = keep < frames || rs.capped;
    } catch (const std::bad_alloc&) {
        result.buffer.reset();
        result.error = BuildError::OutOfMemory;
    }
    return result;
}

} // namespace grains
