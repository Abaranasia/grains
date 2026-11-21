// SPDX-License-Identifier: AGPL-3.0-or-later
#include <grains/WaveformPeaks.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <utility>

namespace grains {

namespace {
double sanitized(float v) noexcept { return std::isfinite(v) ? static_cast<double>(v) : 0.0; }

float mixedFrame(const ChannelView* channels, std::size_t numChannels, std::uint64_t frame) noexcept
{
    double sum = 0.0;
    for (std::size_t c = 0; c < numChannels; ++c) sum += sanitized(channels[c].data[frame]);
    return static_cast<float>(sum / static_cast<double>(numChannels));
}

// Frames usable across all channels; 0 when any channel is null.
std::uint64_t commonFrames(const ChannelView* channels, std::size_t numChannels) noexcept
{
    std::uint64_t frames = 0;
    for (std::size_t c = 0; c < numChannels; ++c) {
        if (channels[c].data == nullptr) return 0;
        frames = c == 0 ? channels[c].frames : std::min<std::uint64_t>(frames, channels[c].frames);
    }
    return frames;
}
} // namespace

std::shared_ptr<const WaveformPeaks> makeWaveformPeaks(const ChannelView* channels, std::size_t numChannels,
                                                       std::size_t numBuckets) noexcept
{
    try {
        auto peaks = std::make_shared<WaveformPeaks>();
        const std::uint64_t frames = channels != nullptr ? commonFrames(channels, numChannels) : 0;
        if (numChannels == 0 || frames == 0 || numBuckets == 0) return peaks;

        peaks->buckets.resize(numBuckets);
        const std::uint64_t n = numBuckets;
        for (std::uint64_t b = 0; b < n; ++b) {
            const std::uint64_t start = b * frames / n;
            const std::uint64_t end = std::max((b + 1) * frames / n, start + 1); // F < N: repeat `start`
            float lo = mixedFrame(channels, numChannels, start);
            float hi = lo;
            for (std::uint64_t f = start + 1; f < end; ++f) {
                const float v = mixedFrame(channels, numChannels, f);
                lo = std::min(lo, v);
                hi = std::max(hi, v);
            }
            peaks->buckets[static_cast<std::size_t>(b)] = {lo, hi};
        }
        return peaks;
    } catch (...) {
        return nullptr;
    }
}

PeakBucket columnPeak(const WaveformPeaks& peaks, int column, int width) noexcept
{
    const std::uint64_t n = peaks.buckets.size();
    if (n == 0 || width <= 0 || column < 0 || column >= width) return {};
    const std::uint64_t w = static_cast<std::uint64_t>(width);
    const std::uint64_t c = static_cast<std::uint64_t>(column);
    const std::uint64_t start = c * n / w;
    const std::uint64_t end = std::max((c + 1) * n / w, start + 1); // width > N: repeat `start`
    PeakBucket out = peaks.buckets[static_cast<std::size_t>(start)];
    for (std::uint64_t b = start + 1; b < end; ++b) {
        out.min = std::min(out.min, peaks.buckets[static_cast<std::size_t>(b)].min);
        out.max = std::max(out.max, peaks.buckets[static_cast<std::size_t>(b)].max);
    }
    return out;
}

bool WaveformState::apply(bool loaded, bool cleared, std::shared_ptr<const WaveformPeaks> p) noexcept
{
    if (loaded) {
        const bool changed = !hasSample || peaks != p;
        hasSample = true;
        peaks = std::move(p);
        return changed;
    }
    if (cleared) {
        const bool changed = hasSample || peaks != nullptr;
        hasSample = false;
        peaks.reset();
        return changed;
    }
    return false;
}

WaveformDisplay WaveformState::display() const noexcept
{
    if (!hasSample) return WaveformDisplay::Empty;
    if (peaks == nullptr || peaks->buckets.empty()) return WaveformDisplay::NoWaveform;
    return WaveformDisplay::Waveform;
}

} // namespace grains
