#include <catch2/catch_test_macros.hpp>

#include <grains/GenerationGate.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <thread>

TEST_CASE("gate: next() counts 1, 2, 3 and current() follows", "[gate]")
{
    grains::GenerationGate g;
    CHECK(g.current() == 0);
    CHECK(g.next() == 1);
    CHECK(g.current() == 1);
    CHECK(g.next() == 2);
    CHECK(g.next() == 3);
    CHECK(g.current() == 3);
}

TEST_CASE("gate: commitIf on the current generation runs the callback once", "[gate]")
{
    grains::GenerationGate g;
    const auto gen = g.next();
    int runs = 0;
    CHECK(g.commitIf(gen, [&]() noexcept { ++runs; }));
    CHECK(runs == 1);
}

TEST_CASE("gate: commitIf on a stale generation does not run the callback", "[gate]")
{
    grains::GenerationGate g;
    const auto old = g.next();
    g.next();
    int runs = 0;
    CHECK_FALSE(g.commitIf(old, [&]() noexcept { ++runs; }));
    CHECK(runs == 0);
}

// R-SS-7: a next() issued while the callback runs must wait for it, so the commit and the currency
// check are one critical section.
TEST_CASE("gate: next() cannot complete while a commit callback is running", "[gate]")
{
    grains::GenerationGate g;
    const auto old = g.next();
    std::thread bumper;
    std::uint64_t seenInside = 0;
    const bool ok = g.commitIf(old, [&]() noexcept {
        bumper = std::thread([&] { g.next(); });
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        seenInside = g.current();
    });
    bumper.join();
    CHECK(ok);
    CHECK(seenInside == old);
    CHECK(g.current() == old + 1);
}

// R-SS-11: bounded stress. The callback must never observe a generation other than the one it was
// admitted under. The number of commits is scheduling dependent and deliberately not asserted.
TEST_CASE("gate: a committed callback always sees its own generation under contention", "[gate][stress]")
{
    grains::GenerationGate g;
    constexpr int kBumps = 100000;
    std::atomic<bool> done{false};
    int mismatches = 0;
    std::thread committer([&] {
        while (!done.load()) {
            const auto gen = g.current();
            g.commitIf(gen, [&]() noexcept {
                if (g.current() != gen) ++mismatches;
            });
        }
    });
    for (int i = 0; i < kBumps; ++i) g.next();
    done = true;
    committer.join();
    CHECK(mismatches == 0);
    CHECK(g.current() == static_cast<std::uint64_t>(kBumps));
    CHECK(g.commitIf(g.current(), []() noexcept {}));
}
