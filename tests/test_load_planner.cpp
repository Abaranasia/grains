// SPDX-License-Identifier: AGPL-3.0-or-later
#include <catch2/catch_test_macros.hpp>

#include <grains/LoadPlanner.h>

#include <atomic>
#include <cstdint>
#include <limits>
#include <string>
#include <thread>

using grains::LoadOrigin;
using grains::LoadOutcome;
using grains::LoadPlanner;
using grains::LoadRequest;
using grains::PublishedSource;

namespace {
LoadRequest makeRequest(const std::string& path, LoadOrigin origin = LoadOrigin::User)
{
    LoadRequest r;
    r.candidates = {path};
    r.original = path;
    r.displayName = "name-" + path;
    r.origin = origin;
    return r;
}

PublishedSource makePublished(double rate)
{
    PublishedSource p;
    p.path = "found.wav";
    p.original = "orig.wav";
    p.displayName = "shown";
    p.label = "Loaded: found.wav";
    p.rate = rate;
    return p;
}

// Runs a request through the whole planner cycle as the loader thread would, publishing at `rate`.
std::uint64_t loadAndPublish(LoadPlanner& p, double rate)
{
    const auto gen = p.request(makeRequest("a.wav"));
    auto taken = p.take();
    REQUIRE(taken.has_value());
    p.published(makePublished(rate));
    p.settle(gen, LoadOutcome::Published);
    return gen;
}
} // namespace

TEST_CASE("loadplan: generations increase and match the gate", "[loadplan]")
{
    LoadPlanner p;
    const auto g1 = p.request(makeRequest("a.wav"));
    CHECK(g1 == 1);
    CHECK(p.gate().current() == g1);
    const auto g2 = p.request(makeRequest("b.wav"));
    CHECK(g2 == g1 + 1);
    CHECK(p.gate().current() == g2);
}

TEST_CASE("loadplan: a request copies the target rate in force", "[loadplan]")
{
    LoadPlanner p;
    p.request(makeRequest("a.wav"));
    auto first = p.take();
    REQUIRE(first.has_value());
    CHECK(first->targetRate == 0.0);

    p.retarget(48000.0);
    p.request(makeRequest("b.wav"));
    auto second = p.take();
    REQUIRE(second.has_value());
    CHECK(second->targetRate == 48000.0);
    CHECK(p.target() == 48000.0);
}

TEST_CASE("loadplan: a newer request replaces the pending one", "[loadplan]")
{
    LoadPlanner p;
    p.request(makeRequest("a.wav"));
    const auto g2 = p.request(makeRequest("b.wav"));
    auto r = p.take();
    REQUIRE(r.has_value());
    CHECK(r->generation == g2);
    CHECK(r->original == "b.wav");
}

TEST_CASE("loadplan: take returns a request once, then nothing", "[loadplan]")
{
    LoadPlanner p;
    CHECK_FALSE(p.take().has_value());
    p.request(makeRequest("a.wav"));
    CHECK(p.take().has_value());
    CHECK_FALSE(p.take().has_value());
}

TEST_CASE("loadplan: retarget ignores invalid and unchanged rates", "[loadplan]")
{
    LoadPlanner p;
    CHECK_FALSE(p.retarget(0.0));
    CHECK_FALSE(p.retarget(-44100.0));
    CHECK_FALSE(p.retarget(std::numeric_limits<double>::quiet_NaN()));
    CHECK_FALSE(p.retarget(std::numeric_limits<double>::infinity()));
    CHECK(p.target() == 0.0);

    p.request(makeRequest("a.wav")); // keep a request in flight so a rate change would issue one
    CHECK(p.retarget(48000.0));
    CHECK_FALSE(p.retarget(48000.0));
}

TEST_CASE("loadplan: retarget replaces a pending request at the new rate", "[loadplan]")
{
    LoadPlanner p;
    const auto g1 = p.request(makeRequest("a.wav"));
    REQUIRE(p.retarget(96000.0));
    auto r = p.take();
    REQUIRE(r.has_value());
    CHECK(r->generation > g1);
    CHECK(r->targetRate == 96000.0);
    CHECK(r->original == "a.wav");
    CHECK(p.gate().current() == r->generation);
    CHECK_FALSE(p.take().has_value());
}

TEST_CASE("loadplan: retarget reissues an in-flight request with the same fields", "[loadplan]")
{
    LoadPlanner p;
    LoadRequest req = makeRequest("a.wav", LoadOrigin::Restore);
    req.label = "keep";
    const auto g1 = p.request(req);
    REQUIRE(p.take().has_value()); // in flight: taken, not settled
    REQUIRE(p.retarget(48000.0));
    auto r = p.take();
    REQUIRE(r.has_value());
    CHECK(r->generation > g1);
    CHECK(r->targetRate == 48000.0);
    CHECK(r->origin == LoadOrigin::Restore);
    CHECK(r->candidates == req.candidates);
    CHECK(r->original == req.original);
    CHECK(r->displayName == req.displayName);
    CHECK(r->label == "keep");
}

TEST_CASE("loadplan: published before settle still counts as in flight", "[loadplan]")
{
    LoadPlanner p;
    const auto g1 = p.request(makeRequest("a.wav"));
    REQUIRE(p.take().has_value());
    p.published(makePublished(44100.0));
    REQUIRE(p.retarget(48000.0)); // not settled yet: the request itself is reissued
    auto r = p.take();
    REQUIRE(r.has_value());
    CHECK(r->generation > g1);
    CHECK(r->origin == LoadOrigin::User);
    CHECK(r->original == "a.wav");
}

TEST_CASE("loadplan: a settled published source at another rate is reloaded", "[loadplan]")
{
    LoadPlanner p;
    loadAndPublish(p, 44100.0);
    REQUIRE(p.retarget(48000.0));
    auto r = p.take();
    REQUIRE(r.has_value());
    CHECK(r->origin == LoadOrigin::Reload);
    CHECK(r->candidates == std::vector<std::string>{"found.wav"});
    CHECK(r->original == "orig.wav");
    CHECK(r->displayName == "shown");
    CHECK(r->label == "Loaded: found.wav");
    CHECK(r->targetRate == 48000.0);
    CHECK(p.gate().current() == r->generation);
}

TEST_CASE("loadplan: nothing is reloaded when the published rate already matches", "[loadplan]")
{
    LoadPlanner p;
    loadAndPublish(p, 44100.0);
    CHECK_FALSE(p.retarget(44100.0));
    CHECK_FALSE(p.take().has_value());
    CHECK(p.target() == 44100.0);
}

TEST_CASE("loadplan: nothing is reloaded after the source was cleared", "[loadplan]")
{
    LoadPlanner p;
    loadAndPublish(p, 44100.0);
    p.cleared();
    CHECK_FALSE(p.retarget(48000.0));
    CHECK_FALSE(p.take().has_value());
}

// ---- R4-001: a failed latest non-Reload request leaves a stale-rate source; reload it once ----

namespace {
// Leaves the planner with a settled 44.1k source and target 48k, with nothing pending.
void settledAtOtherRate(LoadPlanner& p)
{
    loadAndPublish(p, 44100.0);
    REQUIRE(p.retarget(48000.0));
    auto reload = p.take();
    REQUIRE(reload.has_value());
    p.settle(reload->generation, LoadOutcome::Published); // the fake load keeps the 44.1k source
}
} // namespace

TEST_CASE("loadplan: R4-001 a failed user load issues one reload at the target rate", "[loadplan]")
{
    LoadPlanner p;
    loadAndPublish(p, 44100.0);
    REQUIRE(p.retarget(48000.0));
    auto reload = p.take(); // Reload R is issued and taken
    REQUIRE(reload.has_value());
    const auto gu = p.request(makeRequest("u.wav")); // user request U supersedes R
    CHECK_FALSE(p.settle(reload->generation, LoadOutcome::Aborted)); // R aborted: nothing issued
    auto u = p.take();
    REQUIRE(u.has_value());
    CHECK(u->generation == gu);
    CHECK(p.settle(gu, LoadOutcome::Failed)); // U failed: the stale 44.1k source is reloaded
    auto r2 = p.take();
    REQUIRE(r2.has_value());
    CHECK(r2->origin == LoadOrigin::Reload);
    CHECK(r2->targetRate == 48000.0);
    CHECK(r2->candidates == std::vector<std::string>{"found.wav"});
    CHECK(r2->generation > gu);
    CHECK(p.gate().current() == r2->generation);
}

TEST_CASE("loadplan: R4-001 the reissued Reload carries the failure status (R-SS-15)", "[loadplan]")
{
    LoadPlanner p;
    loadAndPublish(p, 44100.0);
    REQUIRE(p.retarget(48000.0));
    p.take();
    const auto gu = p.request(makeRequest("u.wav"));
    p.take();
    REQUIRE(p.settle(gu, LoadOutcome::Failed, "Error: u.wav"));
    auto r2 = p.take();
    REQUIRE(r2.has_value());
    CHECK(r2->label == "Error: u.wav"); // the Reload must not overwrite the error status
}

TEST_CASE("loadplan: R4-001 without a failure status the reissued Reload keeps the published label", "[loadplan]")
{
    LoadPlanner p;
    loadAndPublish(p, 44100.0);
    REQUIRE(p.retarget(48000.0));
    p.take();
    const auto gu = p.request(makeRequest("u.wav"));
    p.take();
    REQUIRE(p.settle(gu, LoadOutcome::Failed));
    auto r2 = p.take();
    REQUIRE(r2.has_value());
    CHECK(r2->label == "Loaded: found.wav");
}

TEST_CASE("loadplan: R4-001 a failed Reload is not retried", "[loadplan]")
{
    LoadPlanner p;
    loadAndPublish(p, 44100.0);
    REQUIRE(p.retarget(48000.0));
    auto reload = p.take();
    REQUIRE(reload.has_value());
    CHECK_FALSE(p.settle(reload->generation, LoadOutcome::Failed));
    CHECK_FALSE(p.take().has_value());
}

TEST_CASE("loadplan: R4-001 a reissued Reload that fails gets only one attempt", "[loadplan]")
{
    LoadPlanner p;
    loadAndPublish(p, 44100.0);
    p.retarget(48000.0);
    p.take();
    const auto gu = p.request(makeRequest("u.wav"));
    p.take();
    REQUIRE(p.settle(gu, LoadOutcome::Failed));
    auto r2 = p.take();
    REQUIRE(r2.has_value());
    CHECK_FALSE(p.settle(r2->generation, LoadOutcome::Failed));
    CHECK_FALSE(p.take().has_value());
}

TEST_CASE("loadplan: R4-001 a published latest user request at a stale rate issues nothing", "[loadplan]")
{
    LoadPlanner p;
    settledAtOtherRate(p); // published 44.1k, target 48k
    const auto gu = p.request(makeRequest("u.wav"));
    p.take();
    CHECK_FALSE(p.settle(gu, LoadOutcome::Published));
    CHECK_FALSE(p.take().has_value());
}

TEST_CASE("loadplan: R4-001 repeating a failed Reload notification issues nothing", "[loadplan]")
{
    LoadPlanner p;
    loadAndPublish(p, 44100.0);
    REQUIRE(p.retarget(48000.0));
    auto reload = p.take();
    REQUIRE(reload.has_value());
    CHECK_FALSE(p.settle(reload->generation, LoadOutcome::Failed));
    CHECK_FALSE(p.settle(reload->generation, LoadOutcome::Failed));
    CHECK_FALSE(p.take().has_value());
}

TEST_CASE("loadplan: R4-001 a superseded failure issues nothing", "[loadplan]")
{
    LoadPlanner p;
    settledAtOtherRate(p);
    const auto ga = p.request(makeRequest("a.wav"));
    p.take();
    p.request(makeRequest("b.wav")); // A is no longer the latest
    CHECK_FALSE(p.settle(ga, LoadOutcome::Failed));
}

TEST_CASE("loadplan: R4-001 issues nothing without a known target rate", "[loadplan]")
{
    LoadPlanner p;
    loadAndPublish(p, 44100.0); // target stays 0
    const auto gu = p.request(makeRequest("u.wav"));
    p.take();
    CHECK_FALSE(p.settle(gu, LoadOutcome::Failed));
    CHECK_FALSE(p.take().has_value());
}

TEST_CASE("loadplan: R4-001 issues nothing when the published rate equals the target", "[loadplan]")
{
    LoadPlanner p;
    loadAndPublish(p, 44100.0);
    CHECK_FALSE(p.retarget(44100.0));
    const auto gu = p.request(makeRequest("u.wav"));
    p.take();
    CHECK_FALSE(p.settle(gu, LoadOutcome::Failed));
    CHECK_FALSE(p.take().has_value());
}

TEST_CASE("loadplan: R4-001 issues nothing when nothing is published", "[loadplan]")
{
    LoadPlanner p;
    p.retarget(48000.0);
    const auto gu = p.request(makeRequest("u.wav"));
    p.take();
    CHECK_FALSE(p.settle(gu, LoadOutcome::Failed));
    CHECK_FALSE(p.take().has_value());
}

TEST_CASE("loadplan: R4-001 an aborted latest request issues nothing", "[loadplan]")
{
    LoadPlanner p;
    settledAtOtherRate(p);
    const auto gu = p.request(makeRequest("u.wav"));
    p.take();
    CHECK_FALSE(p.settle(gu, LoadOutcome::Aborted));
    CHECK_FALSE(p.take().has_value());
}

TEST_CASE("loadplan: R4-001 a failed Restore after the source was cleared issues nothing", "[loadplan]")
{
    LoadPlanner p;
    settledAtOtherRate(p);
    p.cleared();
    const auto gr = p.request(makeRequest("r.wav", LoadOrigin::Restore));
    p.take();
    CHECK_FALSE(p.settle(gr, LoadOutcome::Failed));
    CHECK_FALSE(p.take().has_value());
}

TEST_CASE("loadplan: concurrent requests and loads neither deadlock nor lose the last request", "[loadplan]")
{
    LoadPlanner p;
    constexpr int kIterations = 10000;
    std::atomic<bool> producerDone{false};

    std::thread loader([&] {
        const auto drain = [&] {
            while (auto r = p.take()) {
                const bool accepted = p.gate().commitIf(r->generation, []() noexcept {});
                if (accepted) p.published(makePublished(r->targetRate > 0.0 ? r->targetRate : 44100.0));
                p.settle(r->generation, accepted ? LoadOutcome::Published : LoadOutcome::Aborted);
            }
        };
        while (!producerDone.load()) drain();
        drain();
    });

    std::uint64_t last = 0;
    for (int i = 0; i < kIterations; ++i) {
        if (i % 3 == 0) p.retarget(i % 2 == 0 ? 48000.0 : 44100.0);
        last = p.request(makeRequest("f" + std::to_string(i) + ".wav"));
    }
    const auto lastGen = last;
    producerDone.store(true);
    loader.join();

    // No retarget ran after the final request, so nothing superseded it.
    CHECK(p.gate().current() == lastGen);
    CHECK_FALSE(p.take().has_value());
}
