#pragma once

#include <vector>

namespace grains {

// Immutable source audio (mono or stereo). Owns its data; never modified after construction.
class SampleBuffer {
public:
    // Empty `left` -> empty buffer. Empty `right` -> mono. A `right` whose size differs
    // from `left` is ignored (treated as mono).
    SampleBuffer(std::vector<float> left, std::vector<float> right, double sampleRate);

    int numChannels() const noexcept { return numChannels_; }
    int numFrames() const noexcept { return numFrames_; }
    double sampleRate() const noexcept { return sampleRate_; }

    // nullptr when `c` is out of range.
    const float* channel(int c) const noexcept;

private:
    std::vector<float> left_;
    std::vector<float> right_;
    double sampleRate_;
    int numChannels_ = 0;
    int numFrames_ = 0;
};

} // namespace grains
