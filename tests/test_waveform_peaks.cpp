// SPDX-License-Identifier: AGPL-3.0-or-later
// R-WD-1: peaks contract and column folding.
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <grains/ChannelView.h>
#include <grains/WaveformPeaks.h>

#include <cmath>
#include <cstddef>
#include <cstring>
#include <limits>
#include <vector>

using Catch::Approx;
using grains::ChannelView;
using grains::columnPeak;
using grains::makeWaveformPeaks;
using grains::PeakBucket;
using grains::WaveformPeaks;

namespace {
bool allFiniteAndOrdered(const WaveformPeaks& p)
{
    for (const PeakBucket& b : p.buckets)
        if (!std::isfinite(b.min) || !std::isfinite(b.max) || b.min > b.max) return false;
    return true;
}

bool bitIdentical(const WaveformPeaks& a, const WaveformPeaks& b)
{
    return a.buckets.size() == b.buckets.size() &&
           std::memcmp(a.buckets.data(), b.buckets.data(), a.buckets.size() * sizeof(PeakBucket)) == 0;
}

std::vector<float> ramp(std::size_t frames) // sample i == i, exact in float for frames < 2^24
{
    std::vector<float> v(frames);
    for (std::size_t i = 0; i < frames; ++i) v[i] = static_cast<float>(i);
    return v;
}
} // namespace

TEST_CASE("peaks: stereo +1/-1 mixes to silence in every bucket", "[waveform][peaks]")
{
    const std::vector<float> left(512, 1.0f), right(512, -1.0f);
    const ChannelView view[] = {{left.data(), left.size()}, {right.data(), right.size()}};
    for (const std::size_t n : {std::size_t{1}, std::size_t{7}, std::size_t{64}}) {
        const auto p = makeWaveformPeaks(view, 2, n);
        REQUIRE(p != nullptr);
        REQUIRE(p->buckets.size() == n);
        for (const PeakBucket& b : p->buckets) {
            CHECK(b.min == 0.0f);
            CHECK(b.max == 0.0f);
        }
    }
}

TEST_CASE("peaks: mixed mono is the arithmetic mean of the channels", "[waveform][peaks]")
{
    const std::vector<float> left(100, 1.0f), right(100, 0.5f);
    const ChannelView view[] = {{left.data(), left.size()}, {right.data(), right.size()}};
    const auto p = makeWaveformPeaks(view, 2, 4);
    REQUIRE(p != nullptr);
    REQUIRE(p->buckets.size() == 4);
    for (const PeakBucket& b : p->buckets) {
        CHECK(b.min == Approx(0.75f));
        CHECK(b.max == Approx(0.75f));
    }
}

TEST_CASE("peaks: a single spike lands in exactly one of 10 buckets", "[waveform][peaks]")
{
    std::vector<float> in(1000, 0.0f);
    in[500] = 0.9f;
    const ChannelView view[] = {{in.data(), in.size()}};
    const auto p = makeWaveformPeaks(view, 1, 10);
    REQUIRE(p != nullptr);
    REQUIRE(p->buckets.size() == 10);
    int spikes = 0;
    for (std::size_t b = 0; b < p->buckets.size(); ++b) {
        if (p->buckets[b].max == 0.9f) {
            ++spikes;
            CHECK(b == 5); // frames 500..599
        } else {
            CHECK(p->buckets[b].max == 0.0f);
        }
    }
    CHECK(spikes == 1);
    CHECK(allFiniteAndOrdered(*p));
}

TEST_CASE("peaks: buckets cover every frame with no gaps or overlaps", "[waveform][peaks]")
{
    // With sample i == i, bucket b holds frames [lo, hi]: min == lo and max == hi. Coverage means the
    // first min is 0, each next min is the previous max + 1, and the last max is frames - 1.
    for (const std::size_t frames : {std::size_t{1000}, std::size_t{1001}, std::size_t{4099}}) {
        const std::vector<float> in = ramp(frames);
        const ChannelView view[] = {{in.data(), in.size()}};
        for (const std::size_t n : {std::size_t{1}, std::size_t{7}, std::size_t{10}, std::size_t{64}}) {
            const auto p = makeWaveformPeaks(view, 1, n);
            REQUIRE(p != nullptr);
            REQUIRE(p->buckets.size() == n);
            CHECK(p->buckets.front().min == 0.0f);
            CHECK(p->buckets.back().max == static_cast<float>(frames - 1));
            for (std::size_t b = 1; b < n; ++b) CHECK(p->buckets[b].min == p->buckets[b - 1].max + 1.0f);
            CHECK(allFiniteAndOrdered(*p));
        }
    }
}

TEST_CASE("peaks: mono input is the channel itself", "[waveform][peaks]")
{
    const std::vector<float> in = {0.5f, -0.25f, 0.125f, -1.0f, 0.75f};
    const ChannelView view[] = {{in.data(), in.size()}};
    const auto p = makeWaveformPeaks(view, 1, in.size()); // one frame per bucket
    REQUIRE(p != nullptr);
    REQUIRE(p->buckets.size() == in.size());
    for (std::size_t i = 0; i < in.size(); ++i) {
        CHECK(p->buckets[i].min == in[i]);
        CHECK(p->buckets[i].max == in[i]);
    }
}

TEST_CASE("peaks: empty input gives a non-null result with no buckets", "[waveform][peaks]")
{
    const std::vector<float> in(16, 0.5f);
    const ChannelView zeroFrames[] = {{in.data(), 0}};
    const ChannelView withData[] = {{in.data(), in.size()}};
    const ChannelView nullData[] = {{nullptr, 16}};

    const auto noFrames = makeWaveformPeaks(zeroFrames, 1, 32);
    REQUIRE(noFrames != nullptr);
    CHECK(noFrames->buckets.empty());

    const auto noChannels = makeWaveformPeaks(nullptr, 0, 32);
    REQUIRE(noChannels != nullptr);
    CHECK(noChannels->buckets.empty());

    const auto noBuckets = makeWaveformPeaks(withData, 1, 0);
    REQUIRE(noBuckets != nullptr);
    CHECK(noBuckets->buckets.empty());

    const auto nullChannel = makeWaveformPeaks(nullData, 1, 32);
    REQUIRE(nullChannel != nullptr);
    CHECK(nullChannel->buckets.empty());

    // Triangulation: the same call with real data is not empty.
    const auto real = makeWaveformPeaks(withData, 1, 8);
    REQUIRE(real != nullptr);
    CHECK(real->buckets.size() == 8);
}

TEST_CASE("peaks: fewer frames than buckets still yields N finite ordered buckets", "[waveform][peaks]")
{
    const std::vector<float> in = {0.5f, -0.5f, 0.25f};
    const ChannelView view[] = {{in.data(), in.size()}};
    const auto p = makeWaveformPeaks(view, 1, 64);
    REQUIRE(p != nullptr);
    REQUIRE(p->buckets.size() == 64);
    CHECK(allFiniteAndOrdered(*p));
    // Bucket b starts at frame b*3/64; an empty bucket repeats that frame (clamped to the last).
    CHECK(p->buckets.front().min == 0.5f);
    CHECK(p->buckets.front().max == 0.5f);
    CHECK(p->buckets.back().min == 0.25f);
    CHECK(p->buckets.back().max == 0.25f);
    // Every frame value shows up somewhere (no frame is skipped).
    bool seen[3] = {false, false, false};
    for (const PeakBucket& b : p->buckets)
        for (std::size_t i = 0; i < 3; ++i)
            if (b.min == in[i] && b.max == in[i]) seen[i] = true;
    CHECK(seen[0]);
    CHECK(seen[1]);
    CHECK(seen[2]);
}

TEST_CASE("peaks: NaN and Inf samples count as 0", "[waveform][peaks]")
{
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float inf = std::numeric_limits<float>::infinity();
    const std::vector<float> in = {nan, inf, -inf, 0.5f};
    const ChannelView mono[] = {{in.data(), in.size()}};
    const auto p = makeWaveformPeaks(mono, 1, 4);
    REQUIRE(p != nullptr);
    REQUIRE(p->buckets.size() == 4);
    CHECK(p->buckets[0].min == 0.0f);
    CHECK(p->buckets[1].max == 0.0f);
    CHECK(p->buckets[2].min == 0.0f);
    CHECK(p->buckets[3].max == 0.5f);
    CHECK(allFiniteAndOrdered(*p));

    // One bad channel contributes 0 to the mean: (NaN->0 + 1.0) / 2.
    const std::vector<float> bad(8, nan), good(8, 1.0f);
    const ChannelView stereo[] = {{bad.data(), bad.size()}, {good.data(), good.size()}};
    const auto q = makeWaveformPeaks(stereo, 2, 2);
    REQUIRE(q != nullptr);
    CHECK(q->buckets[0].max == Approx(0.5f));
    CHECK(allFiniteAndOrdered(*q));
}

TEST_CASE("peaks: channels of different length use the shortest", "[waveform][peaks]")
{
    const std::vector<float> longer(100, 1.0f), shorter(10, 1.0f);
    const ChannelView view[] = {{longer.data(), longer.size()}, {shorter.data(), shorter.size()}};
    const auto p = makeWaveformPeaks(view, 2, 5);
    REQUIRE(p != nullptr);
    REQUIRE(p->buckets.size() == 5);
    for (const PeakBucket& b : p->buckets) CHECK(b.max == Approx(1.0f));
}

TEST_CASE("peaks: identical input and N give bit-identical output", "[waveform][peaks]")
{
    std::vector<float> l(3000), r(3000);
    for (std::size_t i = 0; i < l.size(); ++i) {
        l[i] = std::sin(0.01f * static_cast<float>(i));
        r[i] = std::cos(0.0173f * static_cast<float>(i));
    }
    const ChannelView view[] = {{l.data(), l.size()}, {r.data(), r.size()}};
    const auto a = makeWaveformPeaks(view, 2, 100);
    const auto b = makeWaveformPeaks(view, 2, 100);
    REQUIRE(a != nullptr);
    REQUIRE(b != nullptr);
    REQUIRE(a->buckets.size() == 100);
    CHECK(bitIdentical(*a, *b));
    const auto c = makeWaveformPeaks(view, 2, 101); // a different N is a different result
    REQUIRE(c != nullptr);
    CHECK(c->buckets.size() == 101);
}

TEST_CASE("columnPeak: columns partition the buckets and keep min/max", "[waveform][peaks]")
{
    WaveformPeaks p;
    for (int i = 0; i < 8; ++i) p.buckets.push_back({-static_cast<float>(i) * 0.1f, static_cast<float>(i) * 0.1f});
    const PeakBucket c0 = columnPeak(p, 0, 2); // buckets 0..3
    const PeakBucket c1 = columnPeak(p, 1, 2); // buckets 4..7
    CHECK(c0.min == Approx(-0.3f));
    CHECK(c0.max == Approx(0.3f));
    CHECK(c1.min == Approx(-0.7f));
    CHECK(c1.max == Approx(0.7f));

    // One column covers everything.
    const PeakBucket all = columnPeak(p, 0, 1);
    CHECK(all.min == Approx(-0.7f));
    CHECK(all.max == Approx(0.7f));
}

TEST_CASE("columnPeak: a spike appears in exactly one column", "[waveform][peaks]")
{
    WaveformPeaks p;
    p.buckets.assign(2048, PeakBucket{});
    p.buckets[1000] = {-0.8f, 0.8f};
    for (const int width : {100, 333, 640}) {
        int hits = 0;
        for (int col = 0; col < width; ++col) {
            const PeakBucket c = columnPeak(p, col, width);
            if (c.max == 0.8f) {
                ++hits;
                CHECK(c.min == -0.8f);
            }
        }
        INFO("width=" << width);
        CHECK(hits == 1);
    }
}

TEST_CASE("columnPeak: more columns than buckets repeats the nearest bucket", "[waveform][peaks]")
{
    WaveformPeaks p;
    for (int i = 0; i < 4; ++i) p.buckets.push_back({0.0f, static_cast<float>(i + 1)});
    const float expectedMax[10] = {1, 1, 1, 2, 2, 3, 3, 3, 4, 4}; // bucket = col * 4 / 10
    for (int col = 0; col < 10; ++col) {
        INFO("col=" << col);
        CHECK(columnPeak(p, col, 10).max == expectedMax[col]);
    }
}

TEST_CASE("columnPeak: width 0, bad column and empty peaks give a flat zero column", "[waveform][peaks]")
{
    WaveformPeaks p;
    p.buckets.assign(16, PeakBucket{-0.5f, 0.5f});
    CHECK(columnPeak(p, 0, 0).max == 0.0f);
    CHECK(columnPeak(p, 0, 0).min == 0.0f);
    CHECK(columnPeak(p, 0, -4).max == 0.0f);
    CHECK(columnPeak(p, -1, 8).max == 0.0f);
    CHECK(columnPeak(p, 8, 8).max == 0.0f);
    CHECK(columnPeak(p, 7, 8).max == 0.5f); // the valid edge column still works

    const WaveformPeaks empty;
    CHECK(columnPeak(empty, 0, 8).max == 0.0f);
    CHECK(columnPeak(empty, 0, 8).min == 0.0f);
}

// ---- WaveformState (R-WD-3 policy, D4/D5) ----

using grains::WaveformDisplay;
using grains::WaveformState;

namespace {
std::shared_ptr<const WaveformPeaks> peaksWith(float max)
{
    auto p = std::make_shared<WaveformPeaks>();
    p->buckets.push_back({-max, max});
    return p;
}
} // namespace

TEST_CASE("state: starts empty", "[waveform][state]")
{
    const WaveformState s;
    CHECK(s.display() == WaveformDisplay::Empty);
    CHECK(s.peaks == nullptr);
    CHECK_FALSE(s.hasSample);
}

TEST_CASE("state: a loaded result sets the peaks", "[waveform][state]")
{
    WaveformState s;
    const auto a = peaksWith(0.5f);
    CHECK(s.apply(true, false, a));
    CHECK(s.display() == WaveformDisplay::Waveform);
    CHECK(s.peaks == a);
    CHECK(s.hasSample);
}

TEST_CASE("state: a cleared result empties the display", "[waveform][state]")
{
    WaveformState s;
    s.apply(true, false, peaksWith(0.5f));
    REQUIRE(s.display() == WaveformDisplay::Waveform);
    CHECK(s.apply(false, true, nullptr));
    CHECK(s.display() == WaveformDisplay::Empty);
    CHECK(s.peaks == nullptr);
    CHECK_FALSE(s.hasSample);
    CHECK_FALSE(s.apply(false, true, nullptr)); // clearing an already empty state changes nothing
}

TEST_CASE("state: a failed later load keeps the current peaks", "[waveform][state]")
{
    WaveformState s;
    const auto a = peaksWith(0.5f);
    s.apply(true, false, a);
    CHECK_FALSE(s.apply(false, false, nullptr)); // neither loaded nor cleared: the playing source stays
    CHECK(s.peaks == a);
    CHECK(s.display() == WaveformDisplay::Waveform);

    // Triangulation: a failed load on an empty state stays empty.
    WaveformState empty;
    CHECK_FALSE(empty.apply(false, false, nullptr));
    CHECK(empty.display() == WaveformDisplay::Empty);
}

TEST_CASE("state: null peaks on a loaded result show NoWaveform", "[waveform][state]")
{
    WaveformState s;
    CHECK(s.apply(true, false, nullptr));
    CHECK(s.display() == WaveformDisplay::NoWaveform);
    CHECK(s.hasSample);

    // Peaks with no buckets (an empty sample) also have nothing to draw.
    WaveformState e;
    e.apply(true, false, std::make_shared<WaveformPeaks>());
    CHECK(e.display() == WaveformDisplay::NoWaveform);

    // A later loaded result with peaks recovers the waveform.
    CHECK(s.apply(true, false, peaksWith(0.25f)));
    CHECK(s.display() == WaveformDisplay::Waveform);
}

TEST_CASE("state: apply reports whether anything changed", "[waveform][state]")
{
    WaveformState s;
    const auto a = peaksWith(0.5f);
    CHECK(s.apply(true, false, a));
    CHECK_FALSE(s.apply(true, false, a));            // same peaks, same state
    CHECK(s.apply(true, false, peaksWith(0.5f)));    // a redelivery is a new object: repaint
    CHECK(s.apply(true, false, nullptr));            // peaks dropped while the sample stays
}

TEST_CASE("state: results applied in commit order supersede earlier ones", "[waveform][state]")
{
    WaveformState s;
    const auto a = peaksWith(0.25f);
    const auto b = peaksWith(0.75f);
    s.apply(true, false, a);
    s.apply(true, false, b); // B committed after A
    CHECK(s.peaks == b);
    s.apply(false, false, nullptr); // a failed later load never replaces B
    CHECK(s.peaks == b);
    s.apply(false, true, nullptr);  // a Restore clear commits after B
    CHECK(s.display() == WaveformDisplay::Empty);
    s.apply(true, false, a);        // and a later load shows A again
    CHECK(s.peaks == a);
}
