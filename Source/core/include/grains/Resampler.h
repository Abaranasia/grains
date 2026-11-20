// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <grains/ChannelView.h>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <vector>

namespace grains {

// Output frames rendered between two polls of the abort predicate.
inline constexpr std::size_t kResampleChunkFrames = 65536;

enum class ResampleStatus { Ok, Aborted, InvalidArgument, OutOfMemory };

struct ResampleOptions {
    std::size_t maxFrames = SIZE_MAX;                // output cap per channel; the output is cut here
    std::size_t chunkFrames = kResampleChunkFrames;  // abort-poll granularity (never changes the output)
};

struct ResampleResult {
    ResampleStatus status = ResampleStatus::InvalidArgument;
    std::vector<std::vector<float>> channels; // empty unless status == Ok
    bool capped = false;                      // the output was cut at maxFrames
};

// max(1, llround(n * dst / src)); 0 when n is 0 or either rate is not finite and positive.
std::size_t resampledFrames(std::size_t n, double src, double dst) noexcept;

// Offline band-limited conversion from src to dst Hz (Kaiser-windowed sinc, zero delay). One or two
// channels of equal length. shouldAbort is polled before every chunk; true => Aborted with no
// channels. Never throws; allocation failure is reported as OutOfMemory. src == dst copies bytes.
ResampleResult resample(const ChannelView* channels, std::size_t numChannels, double src, double dst,
                        const std::function<bool()>& shouldAbort, ResampleOptions options = {}) noexcept;

} // namespace grains
