// SPDX-License-Identifier: AGPL-3.0-or-later
#include <catch2/catch_test_macros.hpp>

#include <grains/SampleBuffer.h>

#include <type_traits>
#include <vector>

TEST_CASE("buffer: mono accessors", "[buffer]")
{
    grains::SampleBuffer b({0.1f, 0.2f, 0.3f}, {}, 44100.0);
    CHECK(b.numChannels() == 1);
    CHECK(b.numFrames() == 3);
    CHECK(b.sampleRate() == 44100.0);
    REQUIRE(b.channel(0) != nullptr);
    CHECK(b.channel(0)[2] == 0.3f);
    CHECK(b.channel(1) == nullptr);
    CHECK(b.channel(-1) == nullptr);
}

TEST_CASE("buffer: stereo accessors", "[buffer]")
{
    grains::SampleBuffer b({1.0f, 2.0f}, {3.0f, 4.0f}, 48000.0);
    CHECK(b.numChannels() == 2);
    CHECK(b.numFrames() == 2);
    CHECK(b.channel(0)[1] == 2.0f);
    CHECK(b.channel(1)[0] == 3.0f);
    CHECK(b.channel(2) == nullptr);
}

TEST_CASE("buffer: mismatched right channel falls back to mono", "[buffer]")
{
    grains::SampleBuffer b({1.0f, 2.0f, 3.0f}, {1.0f}, 48000.0);
    CHECK(b.numChannels() == 1);
    CHECK(b.numFrames() == 3);
}

TEST_CASE("buffer: empty source", "[buffer]")
{
    grains::SampleBuffer b({}, {}, 48000.0);
    CHECK(b.numChannels() == 0);
    CHECK(b.numFrames() == 0);
    CHECK(b.channel(0) == nullptr);
}

TEST_CASE("buffer: one frame source", "[buffer]")
{
    grains::SampleBuffer b({0.5f}, {}, 48000.0);
    CHECK(b.numFrames() == 1);
    CHECK(b.channel(0)[0] == 0.5f);
}

TEST_CASE("buffer: immutable and owns its data", "[buffer]")
{
    static_assert(std::is_same_v<decltype(std::declval<const grains::SampleBuffer&>().channel(0)), const float*>);
    std::vector<float> src{1.0f, 2.0f};
    grains::SampleBuffer b(src, {}, 48000.0);
    src[0] = 9.0f;
    CHECK(b.channel(0)[0] == 1.0f);
}
