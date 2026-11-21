// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <grains/ChannelView.h>

#include <cstddef>
#include <memory>
#include <vector>

namespace grains {

// R-WD-1: fixed-size min/max overview of a sample, independent of the view width. Immutable once
// shared: the loader thread builds it and the message thread only reads it.
struct PeakBucket {
    float min = 0.0f;
    float max = 0.0f;
};

struct WaveformPeaks {
    std::vector<PeakBucket> buckets;
};

inline constexpr std::size_t kDefaultPeakBuckets = 2048;

// Folds the channels into mono (arithmetic mean per frame; non-finite samples count as 0) and
// summarises `numBuckets` equal frame ranges. Bucket b covers frames [b*F/N, (b+1)*F/N) in 64-bit
// integers, F being the shortest channel length; an empty range (F < N) repeats frame
// min(b*F/N, F-1). Empty input (no channels, no frames, a null channel or N == 0) gives a non-null
// result with no buckets. Returns nullptr only when allocation fails; never throws.
std::shared_ptr<const WaveformPeaks> makeWaveformPeaks(const ChannelView* channels, std::size_t numChannels,
                                                       std::size_t numBuckets) noexcept;

// Min/max over the buckets that pixel column `column` of a `width`-column view covers (the columns
// partition the buckets; with more columns than buckets a column repeats the nearest bucket). A
// width <= 0, an out-of-range column or empty peaks give a flat zero column.
PeakBucket columnPeak(const WaveformPeaks& peaks, int column, int width) noexcept;

enum class WaveformDisplay { Empty, NoWaveform, Waveform };

// What the waveform view shows, owned by the message thread. Results are applied in the order loads
// take effect on the audio path (commit order), so a superseded load can never replace the peaks of
// a later-committed source, and a failed load that leaves the playing source alone changes nothing.
struct WaveformState {
    std::shared_ptr<const WaveformPeaks> peaks;
    bool hasSample = false;

    // `loaded`: a source was committed (peaks may be null if computing them failed). `cleared`: a clear
    // was committed. Neither: the load failed and the current source keeps playing. If both are set,
    // `loaded` wins. Returns true if the state changed (the view must repaint).
    bool apply(bool loaded, bool cleared, std::shared_ptr<const WaveformPeaks> p) noexcept;

    WaveformDisplay display() const noexcept;
};

} // namespace grains
