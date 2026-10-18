#pragma once

// Shared scheduler test helpers.

#include <catch2/catch_test_macros.hpp>

#include <grains/GrainScheduler.h>
#include <grains/Rng.h>

#include <algorithm>
#include <cstdint>
#include <vector>

namespace grains_test {

struct Spawn {
    std::int64_t time;  // absolute sample index
    float draw;         // a per-grain random draw taken in the callback
    bool operator==(const Spawn& o) const { return time == o.time && draw == o.draw; }
};

// Runs `total` samples through the scheduler in blocks of the given sizes (cycled).
inline std::vector<Spawn> run(std::uint64_t seed, double sr, float density, float jitter, std::int64_t total,
                       const std::vector<int>& blocks)
{
    grains::Rng rng(seed);
    grains::GrainScheduler s(rng);
    s.configure(sr, density, jitter);
    s.reset();
    std::vector<Spawn> out;
    std::int64_t done = 0;
    std::size_t bi = 0;
    while (done < total) {
        const int b = static_cast<int>(std::min<std::int64_t>(blocks[bi++ % blocks.size()], total - done));
        s.advance(b, [&](int offset) {
            REQUIRE(offset >= 0);
            REQUIRE(offset < b);
            out.push_back({done + offset, rng.uniform01()});
        });
        done += b;
    }
    return out;
}

} // namespace grains_test
