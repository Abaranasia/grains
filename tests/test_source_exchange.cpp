#include <catch2/catch_test_macros.hpp>

#include <grains/EngineParams.h>
#include <grains/GranularEngine.h>
#include <grains/SampleBuffer.h>
#include <grains/SourceExchange.h>

#include "support/EngineHelpers.h"

#include <atomic>
#include <chrono>
#include <memory>
#include <thread>
#include <vector>

namespace {

// Counts constructions/destructions and records which thread ran the last destructor, so tests can
// prove ownership (freed exactly once) and that the consumer thread never frees.
struct Tracked {
    static inline std::atomic<int> live{0};
    static inline std::atomic<int> destroyed{0};
    static inline std::atomic<std::thread::id> lastDtorThread{};
    static inline std::atomic<int> consumerFrees{0};
    static inline std::atomic<std::thread::id> consumer{};

    explicit Tracked(int i) : id(i) { ++live; }
    Tracked(const Tracked&) = delete;
    Tracked& operator=(const Tracked&) = delete;
    ~Tracked()
    {
        magic = 0;
        --live;
        ++destroyed;
        lastDtorThread = std::this_thread::get_id();
        if (lastDtorThread.load() == consumer.load()) ++consumerFrees;
    }
    static void reset()
    {
        live = 0;
        destroyed = 0;
        consumerFrees = 0;
        consumer = std::thread::id{};
    }

    int id;
    int magic = 0x5eed;
};

using Exchange = grains::SourceExchange<Tracked>;

std::unique_ptr<Tracked> make(int id) { return std::make_unique<Tracked>(id); }

} // namespace

TEST_CASE("exchange: an empty exchange has nothing to acquire", "[exchange]")
{
    Tracked::reset();
    Exchange x;
    const auto r = x.acquire();
    CHECK(r.source == nullptr);
    CHECK_FALSE(r.changed);
}

TEST_CASE("exchange: a published source is delivered once", "[exchange]")
{
    Tracked::reset();
    {
        Exchange x;
        REQUIRE(x.publish(make(1)));
        x.collect();
        const auto a = x.acquire();
        REQUIRE(a.source != nullptr);
        CHECK(a.changed);
        CHECK(a.source->id == 1);

        const auto b = x.acquire();
        CHECK_FALSE(b.changed);
        CHECK(b.source == a.source);
    }
    CHECK(Tracked::live == 0);
}

TEST_CASE("exchange: the latest publish wins and a superseded pending source is freed on publish", "[exchange]")
{
    Tracked::reset();
    {
        Exchange x;
        x.publish(make(1));
        x.publish(make(2)); // 1 was never seen: freed right here
        CHECK(Tracked::live == 1);
        CHECK(Tracked::destroyed == 1);
        const auto r = x.acquire();
        REQUIRE(r.source != nullptr);
        CHECK(r.source->id == 2);
    }
    CHECK(Tracked::live == 0);
}

TEST_CASE("exchange: a swapped-out source stays alive until collect", "[exchange]")
{
    Tracked::reset();
    Exchange x;
    x.publish(make(1));
    x.collect();
    const auto a = x.acquire();
    REQUIRE(a.source != nullptr);
    x.publish(make(2));
    const auto b = x.acquire();
    CHECK(b.changed);
    CHECK(b.source->id == 2);
    CHECK(Tracked::live == 2); // source 1 is retired, not yet freed
    CHECK(a.source->magic == 0x5eed);
    x.collect();
    CHECK(Tracked::live == 1);
    CHECK(Tracked::destroyed == 1);
}

TEST_CASE("exchange: acquire defers adoption while the retire slot is full", "[exchange]")
{
    Tracked::reset();
    Exchange x;
    x.publish(make(1));
    x.acquire();
    x.publish(make(2));
    x.acquire(); // 1 retired
    x.publish(make(3));
    const auto d = x.acquire(); // slot full: keeps 2, 3 stays pending
    CHECK_FALSE(d.changed);
    CHECK(d.source->id == 2);
    x.collect();
    const auto e = x.acquire();
    CHECK(e.changed);
    CHECK(e.source->id == 3);
}

TEST_CASE("exchange: publishing nullptr clears the source", "[exchange]")
{
    Tracked::reset();
    {
        Exchange x;
        x.publish(make(1));
        x.acquire();
        x.publish(nullptr);
        x.collect();
        const auto r = x.acquire();
        CHECK(r.changed);
        CHECK(r.source == nullptr);
        CHECK_FALSE(x.acquire().changed);
        x.collect();
        CHECK(Tracked::live == 0);
    }
}

TEST_CASE("exchange: the destructor frees retired, current and pending exactly once", "[exchange]")
{
    Tracked::reset();
    {
        Exchange x;
        x.publish(make(1));
        x.acquire();
        x.publish(make(2));
        x.acquire(); // 1 retired, 2 current
        x.publish(make(3)); // 3 pending
        CHECK(Tracked::live == 3);
        CHECK(Tracked::destroyed == 0);
    }
    CHECK(Tracked::live == 0);
    CHECK(Tracked::destroyed == 3);
}

// R-SS-3: concurrent publish/collect against acquire. Ids must only move forward (no duplicates, no
// resurrected source), every read must see a live object, and the consumer thread must never free.
TEST_CASE("exchange: stress with a producer and a consumer thread", "[exchange][stress]")
{
    Tracked::reset();
    constexpr int kLast = 4999;
    std::atomic<bool> done{false};
    int lastSeen = -1;
    bool ordered = true;
    bool alive = true;
    {
        Exchange x;
        std::thread producer([&] {
            for (int id = 0; id <= kLast; ++id) {
                x.publish(id % 7 == 0 ? nullptr : make(id));
                x.collect();
            }
            while (!done.load()) {
                x.collect();
                std::this_thread::yield();
            }
        });
        std::thread consumer([&] {
            Tracked::consumer = std::this_thread::get_id();
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
            while (lastSeen != kLast && std::chrono::steady_clock::now() < deadline) {
                const auto r = x.acquire();
                if (r.changed && r.source != nullptr) {
                    alive = alive && r.source->magic == 0x5eed;
                    ordered = ordered && r.source->id > lastSeen;
                    lastSeen = r.source->id;
                }
            }
            done = true;
        });
        consumer.join();
        producer.join();
    }
    CHECK(ordered);
    CHECK(alive);
    CHECK(lastSeen == kLast);
    CHECK(Tracked::live == 0);
    CHECK(Tracked::consumerFrees == 0);
}

TEST_CASE("exchange: swapping the engine source drops active grains and a clear silences it", "[exchange][engine]")
{
    constexpr double kSr = 48000.0;
    grains::SourceExchange<grains::SampleBuffer> x;
    x.publish(std::make_unique<grains::SampleBuffer>(grains_test::noise(48000, 1), grains_test::noise(48000, 2), kSr));
    grains::GranularEngine e;
    e.prepare(kSr, 1);
    grains::EngineParams p;
    p.density = 100.0f;
    p.grainSizeMs = 200.0f;
    e.setParams(p);
    e.setSource(x.acquire().source);
    grains_test::render(e, 4096, 512);
    REQUIRE(e.activeGrainCount() > 0);

    x.publish(std::make_unique<grains::SampleBuffer>(std::vector<float>(48000, 0.0f), std::vector<float>(48000, 0.0f), kSr));
    x.collect();
    const auto r = x.acquire();
    REQUIRE(r.changed);
    e.setSource(r.source);
    CHECK(e.activeGrainCount() == 0);
    CHECK(grains_test::allZero(grains_test::render(e, 4096, 512)));

    x.publish(nullptr);
    x.collect(); // frees the retired noise buffer so the clear can be adopted
    const auto c = x.acquire();
    REQUIRE(c.changed);
    CHECK(c.source == nullptr);
    e.setSource(c.source);
    CHECK(grains_test::allZero(grains_test::render(e, 4096, 512)));
}
