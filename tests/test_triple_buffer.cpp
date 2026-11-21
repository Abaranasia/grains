// SPDX-License-Identifier: AGPL-3.0-or-later
#include <catch2/catch_test_macros.hpp>

#include <grains/TripleBuffer.h>

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <thread>

using grains::TripleBuffer;

namespace {

// 512 bytes, every word equal to one sequence number: a torn read mixes two numbers.
struct Frame {
    static constexpr std::size_t kWords = 64;
    std::array<std::uint64_t, kWords> w{};

    void fill(std::uint64_t seq) noexcept
    {
        for (std::uint64_t& x : w) x = seq;
    }
    bool consistent() const noexcept
    {
        for (std::uint64_t x : w)
            if (x != w[0]) return false;
        return true;
    }
};
static_assert(sizeof(Frame) == 512, "stress frame is 512 bytes");

} // namespace

TEST_CASE("triple buffer: front is a default T before any publish", "[triple_buffer]")
{
    TripleBuffer<int> intBuf;
    CHECK(intBuf.front() == 0);

    TripleBuffer<Frame> frameBuf;
    CHECK(frameBuf.front().w[0] == 0);
    CHECK(frameBuf.front().consistent());
}

TEST_CASE("triple buffer: update reports not fresh before any publish", "[triple_buffer]")
{
    TripleBuffer<int> buf;
    CHECK_FALSE(buf.update());
    CHECK_FALSE(buf.update());
    CHECK(buf.front() == 0);
}

TEST_CASE("triple buffer: a published frame is fresh once", "[triple_buffer]")
{
    TripleBuffer<int> buf;
    buf.back() = 42;
    buf.publish();
    CHECK(buf.update());
    CHECK(buf.front() == 42);
    CHECK_FALSE(buf.update());
    CHECK(buf.front() == 42);
}

TEST_CASE("triple buffer: the latest of several publishes wins", "[triple_buffer]")
{
    TripleBuffer<int> buf;
    for (int v : {1, 2, 3}) {
        buf.back() = v;
        buf.publish();
    }
    CHECK(buf.update());
    CHECK(buf.front() == 3);
    CHECK_FALSE(buf.update());

    buf.back() = 4;
    buf.publish();
    buf.back() = 5;
    buf.publish();
    CHECK(buf.update());
    CHECK(buf.front() == 5);
}

TEST_CASE("triple buffer: front is stable across later publishes until update", "[triple_buffer]")
{
    TripleBuffer<int> buf;
    buf.back() = 10;
    buf.publish();
    REQUIRE(buf.update());
    const int* const held = &buf.front();
    for (int v = 11; v < 20; ++v) {
        buf.back() = v;
        buf.publish();
        CHECK(&buf.front() == held);
        CHECK(buf.front() == 10);
    }
    CHECK(buf.update());
    CHECK(buf.front() == 19);
}

// The three slots stay pairwise distinct: front and back never alias, and a write to back never
// reaches a frame the consumer holds or one that is still waiting in the middle.
TEST_CASE("triple buffer: slots stay distinct across publish and update interleavings", "[triple_buffer]")
{
    TripleBuffer<int> buf;
    int published = 0; // newest value handed to publish
    int shown = 0;     // value the consumer holds
    std::uint32_t pattern = 0x9e3779b9u;
    for (int step = 0; step < 5000; ++step) {
        pattern = pattern * 1664525u + 1013904223u;
        const int* const frontAddr = &buf.front();
        if ((pattern >> 16) & 1u) {
            buf.back() = ++published;
            CHECK(&buf.back() != frontAddr);
            buf.publish();
            CHECK(&buf.back() != frontAddr); // the recycled back is not the consumer's slot
        } else if (buf.update()) {
            shown = published;
            CHECK(&buf.front() != frontAddr); // a fresh frame arrives in a different slot
        }
        CHECK(buf.front() == shown);
        CHECK(&buf.back() != &buf.front());
    }
    // Nothing was lost to aliasing: a final update shows the newest publish (or the held one).
    buf.update();
    CHECK(buf.front() == published);
}

// Two threads: the producer publishes 100k self-consistent frames as fast as it can; the consumer
// polls. No sleeps and no timing assertions: both loops are bounded by the producer's frame count.
// Catch2 assertions are not thread-safe, so they all run on the consumer (the test thread).
TEST_CASE("triple buffer: two-thread stress never tears and sequence only increases", "[triple_buffer][stress]")
{
    constexpr std::uint64_t kFrames = 100000;
    TripleBuffer<Frame> buf;
    std::atomic<bool> done{false};

    std::thread producer([&] {
        for (std::uint64_t seq = 1; seq <= kFrames; ++seq) {
            buf.back().fill(seq);
            buf.publish();
        }
        done.store(true, std::memory_order_release);
    });

    std::uint64_t last = 0;
    std::uint64_t observed = 0;
    std::uint64_t torn = 0;
    std::uint64_t regressions = 0;
    const auto poll = [&] {
        if (!buf.update()) {
            std::this_thread::yield();
            return;
        }
        const Frame& f = buf.front();
        if (!f.consistent()) ++torn;
        if (f.w[0] <= last) ++regressions;
        last = f.w[0];
        ++observed;
    };
    while (!done.load(std::memory_order_acquire)) poll();
    poll(); // the producer is finished, so its last frame is now pending (or already shown)
    producer.join();

    CHECK(torn == 0);
    CHECK(regressions == 0);
    CHECK(last == kFrames);
    CHECK(observed >= 1);
    CHECK(observed <= kFrames);
}
