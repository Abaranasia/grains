// SPDX-License-Identifier: AGPL-3.0-or-later
#include <grains/SampleBuffer.h>

#include <utility>

namespace grains {

SampleBuffer::SampleBuffer(std::vector<float> left, std::vector<float> right, double sampleRate)
    : left_(std::move(left)), right_(std::move(right)), sampleRate_(sampleRate)
{
    if (right_.size() != left_.size())
        right_.clear();
    numFrames_ = static_cast<int>(left_.size());
    numChannels_ = left_.empty() ? 0 : (right_.empty() ? 1 : 2);
}

const float* SampleBuffer::channel(int c) const noexcept
{
    if (c < 0 || c >= numChannels_) return nullptr;
    return c == 0 ? left_.data() : right_.data();
}

} // namespace grains
