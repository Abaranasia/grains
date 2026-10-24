#pragma once

#include <grains/SampleBuffer.h>

#include <cstddef>
#include <memory>

namespace grains {

inline constexpr double kMaxSourceSeconds = 300.0;

// Non-owning view of one decoded channel.
struct ChannelView {
    const float* data;
    std::size_t frames;
};

enum class BuildError { None, NoChannels, NoFrames, BadSampleRate, MismatchedLengths, OutOfMemory };

struct BuildResult {
    std::unique_ptr<SampleBuffer> buffer; // null when error != None
    bool truncated = false;               // the input was longer than maxSourceFrames
    BuildError error = BuildError::None;
};

// floor(kMaxSourceSeconds * sampleRate), clamped to INT_MAX; 0 when the rate is not finite or <= 0.
std::size_t maxSourceFrames(double sampleRate) noexcept;

// Copies channels 0 and 1 (mono stays mono, extra channels are dropped) truncated to
// maxSourceFrames. Never throws; allocation failure is reported as OutOfMemory.
BuildResult buildSampleBuffer(const ChannelView* channels, std::size_t numChannels, double sampleRate) noexcept;

} // namespace grains
