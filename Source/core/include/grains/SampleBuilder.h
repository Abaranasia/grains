// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <grains/ChannelView.h>
#include <grains/SampleBuffer.h>

#include <cstddef>
#include <functional>
#include <memory>

namespace grains {

// Longest source kept. 300 s of 48 kHz stereo float is about 115 MB (peak about twice that while the
// decode buffer and the builder's copy coexist), and it keeps frame counts far below
// juce::AudioBuffer's INT_MAX limit. maxSourceFrames still clamps to INT_MAX for extreme rates.
inline constexpr double kMaxSourceSeconds = 300.0;

enum class BuildError { None, NoChannels, NoFrames, BadSampleRate, MismatchedLengths, OutOfMemory, Aborted };

struct BuildResult {
    std::unique_ptr<SampleBuffer> buffer; // null when error != None
    bool truncated = false;               // the input or the resampled output hit its length cap
    BuildError error = BuildError::None;
};

// floor(kMaxSourceSeconds * sampleRate), clamped to INT_MAX; 0 when the rate is not finite or <= 0.
std::size_t maxSourceFrames(double sampleRate) noexcept;

// Copies channels 0 and 1 (mono stays mono, extra channels are dropped) truncated to
// maxSourceFrames. Never throws; allocation failure is reported as OutOfMemory.
BuildResult buildSampleBuffer(const ChannelView* channels, std::size_t numChannels, double sampleRate) noexcept;

// As above, and then converts to targetRate (see Resampler.h) so the published buffer reports
// targetRate. The input is capped at maxSourceFrames(fileRate) and the output at
// maxSourceFrames(targetRate); either cut sets truncated. targetRate == fileRate keeps the samples
// bit-exact. A target that is not finite and positive (or whose cap is 0) is BadSampleRate.
// shouldAbort is polled between resampling chunks; true yields Aborted and no buffer.
BuildResult buildSampleBuffer(const ChannelView* channels, std::size_t numChannels, double fileRate,
                              double targetRate, const std::function<bool()>& shouldAbort) noexcept;

} // namespace grains
