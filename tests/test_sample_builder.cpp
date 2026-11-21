// SPDX-License-Identifier: AGPL-3.0-or-later
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <grains/SampleBuilder.h>

#include <cmath>
#include <cstddef>
#include <cstring>
#include <functional>
#include <limits>
#include <vector>

using grains::BuildError;
using grains::ChannelView;
using grains::buildSampleBuffer;

namespace {

std::vector<float> ramp(std::size_t n, float base)
{
    std::vector<float> v(n);
    for (std::size_t i = 0; i < n; ++i) v[i] = base + static_cast<float>(i);
    return v;
}

} // namespace

TEST_CASE("builder: mono and stereo are kept unchanged", "[builder]")
{
    const auto l = ramp(100, 0.0f), r = ramp(100, 1000.0f);
    const ChannelView mono[] = {{l.data(), l.size()}};
    auto m = buildSampleBuffer(mono, 1, 44100.0);
    REQUIRE(m.buffer != nullptr);
    CHECK(m.error == BuildError::None);
    CHECK_FALSE(m.truncated);
    CHECK(m.buffer->numChannels() == 1);
    CHECK(m.buffer->numFrames() == 100);
    CHECK(m.buffer->sampleRate() == 44100.0);
    CHECK(m.buffer->channel(0)[99] == 99.0f);

    const ChannelView stereo[] = {{l.data(), l.size()}, {r.data(), r.size()}};
    auto s = buildSampleBuffer(stereo, 2, 48000.0);
    REQUIRE(s.buffer != nullptr);
    CHECK(s.buffer->numChannels() == 2);
    CHECK(s.buffer->channel(1)[5] == 1005.0f);
}

TEST_CASE("builder: more than two channels keep channels 0 and 1", "[builder]")
{
    std::vector<std::vector<float>> data;
    std::vector<ChannelView> views;
    for (int c = 0; c < 6; ++c) data.push_back(ramp(10, 100.0f * static_cast<float>(c)));
    for (const auto& d : data) views.push_back({d.data(), d.size()});
    auto b = buildSampleBuffer(views.data(), views.size(), 44100.0);
    REQUIRE(b.buffer != nullptr);
    CHECK(b.buffer->numChannels() == 2);
    CHECK(b.buffer->channel(0)[0] == 0.0f);
    CHECK(b.buffer->channel(1)[0] == 100.0f);
}

TEST_CASE("builder: the length cap truncates to maxSourceFrames", "[builder]")
{
    CHECK(grains::maxSourceFrames(10.0) == 3000);
    CHECK(grains::maxSourceFrames(0.0) == 0);

    const auto over = ramp(3001, 0.0f), exact = ramp(3000, 0.0f);
    const ChannelView a[] = {{over.data(), over.size()}};
    auto t = buildSampleBuffer(a, 1, 10.0);
    REQUIRE(t.buffer != nullptr);
    CHECK(t.truncated);
    CHECK(t.buffer->numFrames() == 3000);
    CHECK(t.buffer->channel(0)[2999] == 2999.0f);

    const ChannelView b[] = {{exact.data(), exact.size()}};
    auto f = buildSampleBuffer(b, 1, 10.0);
    REQUIRE(f.buffer != nullptr);
    CHECK_FALSE(f.truncated);
    CHECK(f.buffer->numFrames() == 3000);
}

TEST_CASE("builder: invalid input is rejected without throwing", "[builder]")
{
    const auto a = ramp(10, 0.0f), b = ramp(7, 0.0f);
    const ChannelView good[] = {{a.data(), a.size()}};
    const ChannelView nullData[] = {{nullptr, 10}};
    const ChannelView empty[] = {{a.data(), 0}};
    const ChannelView mismatch[] = {{a.data(), a.size()}, {b.data(), b.size()}};
    const double inf = std::numeric_limits<double>::infinity();
    const double nan = std::numeric_limits<double>::quiet_NaN();

    struct Case {
        const char* name;
        const ChannelView* ch;
        std::size_t n;
        double rate;
        BuildError expected;
    };
    const Case cases[] = {
        {"zero channels", good, 0, 44100.0, BuildError::NoChannels},
        {"null channel array", nullptr, 1, 44100.0, BuildError::NoChannels},
        {"null data", nullData, 1, 44100.0, BuildError::NoChannels},
        {"no frames", empty, 1, 44100.0, BuildError::NoFrames},
        {"rate zero", good, 1, 0.0, BuildError::BadSampleRate},
        {"rate negative", good, 1, -44100.0, BuildError::BadSampleRate},
        {"rate NaN", good, 1, nan, BuildError::BadSampleRate},
        {"rate infinite", good, 1, inf, BuildError::BadSampleRate},
        {"mismatched lengths", mismatch, 2, 44100.0, BuildError::MismatchedLengths},
    };
    for (const Case& c : cases) {
        INFO(c.name);
        const auto r = buildSampleBuffer(c.ch, c.n, c.rate);
        CHECK(r.buffer == nullptr);
        CHECK(r.error == c.expected);
        CHECK_FALSE(r.truncated);
    }
}

TEST_CASE("builder: maxSourceFrames floors, rejects sub-frame caps and clamps to INT_MAX", "[builder]")
{
    constexpr auto kIntMax = static_cast<std::size_t>(std::numeric_limits<int>::max());
    CHECK(grains::maxSourceFrames(10.0013) == 3000); // 3000.39 floors
    CHECK(grains::maxSourceFrames(0.003) == 0);      // 0.9 floors to zero (rate < 1/300)
    CHECK(grains::maxSourceFrames(1e8) == kIntMax);  // 3e10 clamps
    CHECK(grains::maxSourceFrames(std::numeric_limits<double>::max()) == kIntMax); // product is +inf
    const auto a = ramp(10, 0.0f);
    const ChannelView v[] = {{a.data(), a.size()}};
    const auto r = buildSampleBuffer(v, 1, 0.003);
    CHECK(r.buffer == nullptr);
    CHECK(r.error == BuildError::BadSampleRate);
}

namespace {

const std::function<bool()> kNeverAbort = [] { return false; };

std::vector<float> tone(std::size_t n, double hz, double rate)
{
    std::vector<float> v(n);
    for (std::size_t i = 0; i < n; ++i)
        v[i] = static_cast<float>(std::sin(6.283185307179586 * hz * static_cast<double>(i) / rate));
    return v;
}

} // namespace

TEST_CASE("builder: resamples to the target rate and publishes at it", "[builder]")
{
    const auto l = tone(96000, 1000.0, 96000.0), r = tone(96000, 2000.0, 96000.0);
    const ChannelView stereo[] = {{l.data(), l.size()}, {r.data(), r.size()}};
    const auto b = buildSampleBuffer(stereo, 2, 96000.0, 48000.0, kNeverAbort);
    REQUIRE(b.buffer != nullptr);
    CHECK(b.error == BuildError::None);
    CHECK_FALSE(b.truncated);
    CHECK(b.buffer->numChannels() == 2);
    CHECK(b.buffer->numFrames() == 48000);
    CHECK(b.buffer->sampleRate() == 48000.0);
    // The 1 kHz tone keeps its pitch: a quarter period (12 frames at 48 kHz) after 0 is the peak.
    CHECK(b.buffer->channel(0)[12] == Catch::Approx(1.0f).margin(1e-3));
    CHECK(b.buffer->channel(0)[36] == Catch::Approx(-1.0f).margin(1e-3));
    // The 2 kHz tone on channel 1 peaks at frame 6.
    CHECK(b.buffer->channel(1)[6] == Catch::Approx(1.0f).margin(1e-3));

    const ChannelView mono[] = {{l.data(), l.size()}};
    const auto m = buildSampleBuffer(mono, 1, 96000.0, 48000.0, kNeverAbort);
    REQUIRE(m.buffer != nullptr);
    CHECK(m.buffer->numChannels() == 1);
    CHECK(m.buffer->numFrames() == 48000);
}

TEST_CASE("builder: an equal target rate keeps the samples bit-exact", "[builder]")
{
    const auto l = ramp(500, 0.25f), r = ramp(500, -3.5f);
    const ChannelView stereo[] = {{l.data(), l.size()}, {r.data(), r.size()}};
    const auto b = buildSampleBuffer(stereo, 2, 48000.0, 48000.0, kNeverAbort);
    REQUIRE(b.buffer != nullptr);
    CHECK(b.buffer->sampleRate() == 48000.0);
    REQUIRE(b.buffer->numFrames() == 500);
    CHECK(std::memcmp(b.buffer->channel(0), l.data(), l.size() * sizeof(float)) == 0);
    CHECK(std::memcmp(b.buffer->channel(1), r.data(), r.size() * sizeof(float)) == 0);
}

TEST_CASE("builder: the input is capped at the file rate and the output at the target rate", "[builder]")
{
    // File at 10 Hz: the input cap is 3000 frames. 3001 frames are cut to 3000 and reported.
    const auto over = ramp(3001, 0.0f);
    const ChannelView a[] = {{over.data(), over.size()}};
    const auto in = buildSampleBuffer(a, 1, 10.0, 20.0, kNeverAbort);
    REQUIRE(in.buffer != nullptr);
    CHECK(in.truncated);
    CHECK(in.buffer->numFrames() == 6000); // 3000 kept frames doubled

    // Exactly the cap in, target 40 Hz: 12000 frames out equals floor(300 * 40), so nothing is cut.
    const auto exact = ramp(3000, 0.0f);
    const ChannelView b[] = {{exact.data(), exact.size()}};
    const auto fit = buildSampleBuffer(b, 1, 10.0, 40.0, kNeverAbort);
    REQUIRE(fit.buffer != nullptr);
    CHECK_FALSE(fit.truncated);
    CHECK(fit.buffer->numFrames() == 12000);

    // Target 10.0017 Hz: the resampled length rounds to 3001 but floor(300 * 10.0017) is 3000, so the
    // output cap bites and truncation is reported.
    const ChannelView c[] = {{exact.data(), exact.size()}};
    const auto cut = buildSampleBuffer(c, 1, 10.0, 10.0017, kNeverAbort);
    REQUIRE(cut.buffer != nullptr);
    CHECK(cut.truncated);
    CHECK(cut.buffer->numFrames() == 3000);
    CHECK(cut.buffer->sampleRate() == 10.0017);
}

TEST_CASE("builder: an invalid target rate is rejected", "[builder]")
{
    const auto a = ramp(100, 0.0f);
    const ChannelView v[] = {{a.data(), a.size()}};
    const double inf = std::numeric_limits<double>::infinity();
    const double nan = std::numeric_limits<double>::quiet_NaN();
    for (const double target : {0.0, -1.0, nan, inf, 0.001}) {
        INFO("target " << target);
        const auto r = buildSampleBuffer(v, 1, 44100.0, target, kNeverAbort);
        CHECK(r.buffer == nullptr);
        CHECK(r.error == BuildError::BadSampleRate);
        CHECK_FALSE(r.truncated);
    }
    // Control: the same data with a valid target builds, so the rejections are about the target.
    CHECK(buildSampleBuffer(v, 1, 44100.0, 48000.0, kNeverAbort).buffer != nullptr);
    // The existing input validation still applies on the new overload.
    CHECK(buildSampleBuffer(v, 0, 44100.0, 48000.0, kNeverAbort).error == BuildError::NoChannels);
    CHECK(buildSampleBuffer(v, 1, 0.0, 48000.0, kNeverAbort).error == BuildError::BadSampleRate);
}

TEST_CASE("builder: an abort request yields Aborted and no buffer", "[builder]")
{
    const auto a = tone(200000, 440.0, 96000.0);
    const ChannelView v[] = {{a.data(), a.size()}};
    const std::function<bool()> abortNow = [] { return true; };
    const auto r = buildSampleBuffer(v, 1, 96000.0, 48000.0, abortNow);
    CHECK(r.buffer == nullptr);
    CHECK(r.error == BuildError::Aborted);
    CHECK_FALSE(r.truncated);
    CHECK(buildSampleBuffer(v, 1, 96000.0, 48000.0, kNeverAbort).buffer != nullptr); // control
}
