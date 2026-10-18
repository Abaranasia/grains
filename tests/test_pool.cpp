#include <catch2/catch_test_macros.hpp>

#include <grains/GrainPool.h>

#include <vector>

TEST_CASE("pool: capacity is 128 and acquire fails when full", "[pool]")
{
    grains::GrainPool pool;
    CHECK(grains::GrainPool::kCapacity == 128);
    for (int i = 0; i < 128; ++i)
        REQUIRE(pool.acquire() != nullptr);
    CHECK(pool.activeCount() == 128);
    CHECK(pool.acquire() == nullptr);
    CHECK(pool.activeCount() == 128);
}

TEST_CASE("pool: acquired grains start active and cleared", "[pool]")
{
    grains::GrainPool pool;
    grains::Grain* g = pool.acquire();
    REQUIRE(g != nullptr);
    CHECK(g->active);
    CHECK(g->age == 0);
    CHECK(g->length == 0);
}

TEST_CASE("pool: release frees a slot for reuse", "[pool]")
{
    grains::GrainPool pool;
    std::vector<grains::Grain*> grains;
    for (int i = 0; i < 128; ++i) grains.push_back(pool.acquire());
    REQUIRE(pool.acquire() == nullptr);

    grains::Grain* freed = grains[37];
    pool.release(*freed);
    CHECK_FALSE(freed->active);
    CHECK(pool.activeCount() == 127);

    grains::Grain* again = pool.acquire();
    CHECK(again == freed);
    CHECK(again->active);
    CHECK(pool.activeCount() == 128);
}

TEST_CASE("pool: forEachActive visits in index order", "[pool]")
{
    grains::GrainPool pool;
    std::vector<grains::Grain*> g;
    for (int i = 0; i < 10; ++i) {
        g.push_back(pool.acquire());
        g.back()->length = i;
    }
    pool.release(*g[3]);
    pool.release(*g[7]);

    std::vector<int> seen;
    pool.forEachActive([&](grains::Grain& gr) { seen.push_back(gr.length); });
    const std::vector<int> expected{0, 1, 2, 4, 5, 6, 8, 9};
    CHECK(seen == expected);
}

TEST_CASE("pool: release inside forEachActive is safe", "[pool]")
{
    grains::GrainPool pool;
    for (int i = 0; i < 5; ++i) pool.acquire();
    int visited = 0;
    pool.forEachActive([&](grains::Grain& gr) { ++visited; pool.release(gr); });
    CHECK(visited == 5);
    CHECK(pool.activeCount() == 0);
}

TEST_CASE("pool: reset releases everything", "[pool]")
{
    grains::GrainPool pool;
    for (int i = 0; i < 128; ++i) pool.acquire();
    pool.reset();
    CHECK(pool.activeCount() == 0);
    int visited = 0;
    pool.forEachActive([&](grains::Grain&) { ++visited; });
    CHECK(visited == 0);
    CHECK(pool.acquire() != nullptr);
}
