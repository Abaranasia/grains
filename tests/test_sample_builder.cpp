// SPDX-License-Identifier: AGPL-3.0-or-later
#include <catch2/catch_test_macros.hpp>

#include <grains/SampleBuilder.h>

#include <cmath>
#include <cstddef>
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
