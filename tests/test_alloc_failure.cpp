// SPDX-License-Identifier: AGPL-3.0-or-later
// R-BH-6 / R-RS-10: fail-nth-allocation injection. Linked only into grains_alloc_failure_tests,
// because AllocationCounter.cpp replaces the global operator new/delete.
#include <catch2/catch_test_macros.hpp>

#include <grains/ChannelView.h>
#include <grains/LoadPlanner.h>
#include <grains/Resampler.h>
#include <grains/SampleBuilder.h>
#include <grains/WaveformPeaks.h>

#include <support/AllocationCounter.h>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <new>
#include <string>
#include <utility>
#include <vector>

using grains::BuildError;
using grains::ChannelView;
using grains::LoadOutcome;
using grains::LoadPlanner;
using grains::LoadRequest;
using grains::PublishedSource;

namespace {
// C++14 lets a compiler elide a new/delete pair even with a replaced operator new (clang and gcc
// do at -O1+), so the self-tests below would see no allocation reach the hook. Publishing the
// pointer through a volatile store makes each allocation observable and keeps it.
void* volatile gSink = nullptr;

void observedNewDelete(int v)
{
    int* p = new int(v);
    gSink = p;
    delete p;
}

void observedNewDeleteArray()
{
    char* p = new char[8];
    gSink = p;
    delete[] p;
}
} // namespace

TEST_CASE("armFailure: only the nth allocation fails, once", "[allocfail]")
{
    int thrown = 0, ok = 0;
    grains_test::armFailure(3);
    for (int i = 0; i < 5; ++i) {
        try {
            observedNewDelete(i);
            ++ok;
        } catch (const std::bad_alloc&) {
            ++thrown;
        }
    }
    const bool fired = grains_test::failureFired();
    grains_test::disarmFailure();
    CHECK(thrown == 1);
    CHECK(ok == 4);
    CHECK(fired);
}

TEST_CASE("armFailure: unarmed allocation counts are unchanged", "[allocfail]")
{
    grains_test::AllocationScope scope;
    observedNewDelete(1);
    observedNewDeleteArray();
    CHECK(scope.count() == 2);
    CHECK(scope.frees() == 2);
}

// Contingency 6.1: under the MSVC debug runtime (_ITERATOR_DEBUG_LEVEL 2) a container construction
// or move can allocate its iterator proxy inside a noexcept function. An injected failure there
// reaches std::terminate, so the sweeps below run in Release-style builds only. The armFailure cases above
// still run everywhere.
#if defined(_MSC_VER) && defined(_DEBUG)
#define GRAINS_ALLOC_SWEEPS 0
#else
#define GRAINS_ALLOC_SWEEPS 1
#endif

#if GRAINS_ALLOC_SWEEPS

// R-RS-10. A real std::bad_alloc inside grains::resample, forced by the replaced operator new. (A huge
// ratio cannot do this portably: ASan's allocator aborts on an absurd request even though the call
// is a throwing new, whereas this executable's own operator new overrides it.) Sweeps n = 1, 2, ...
// so every allocation the resampler makes (kernel table, output channel vectors) fails once; each
// fired run must be OutOfMemory with no channels and no leak. Gated like the other sweeps: under the
// MSVC debug runtime even n = 1 (a vector's iterator proxy, allocated in a noexcept constructor)
// reaches std::terminate; verified by running Debug.
TEST_CASE("resample: every nth allocation failure reports OutOfMemory", "[allocfail][resampler]")
{
    const std::vector<float> in(1000, 0.25f);
    const ChannelView mono[] = {{in.data(), in.size()}};
    const ChannelView stereo[] = {{in.data(), in.size()}, {in.data(), in.size()}};
    const std::function<bool()> never = [] { return false; };
    struct Case {
        const char* name;
        const ChannelView* view;
        std::size_t channels;
    };
    const Case cases[] = {{"mono", mono, 1}, {"stereo", stereo, 2}};
    for (const auto& c : cases) {
        bool completed = false;
        for (std::size_t n = 1; n < 1000 && !completed; ++n) {
            grains::ResampleStatus status = grains::ResampleStatus::Ok;
            std::size_t channelCount = 99, allocs = 0, frees = 0;
            bool fired = false;
            {
                grains_test::AllocationScope scope;
                grains_test::armFailure(n);
                {
                    const auto r = grains::resample(c.view, c.channels, 48000.0, 44100.0, never);
                    grains_test::disarmFailure(); // before any assertion, which allocates
                    fired = grains_test::failureFired();
                    status = r.status;
                    channelCount = r.channels.size();
                }
                allocs = scope.count();
                frees = scope.frees();
            }
            grains_test::disarmFailure();
            INFO(c.name << " n=" << n);
            REQUIRE(allocs == frees);
            if (!fired) {
                REQUIRE(status == grains::ResampleStatus::Ok);
                REQUIRE(channelCount == c.channels);
                completed = true;
                continue;
            }
            REQUIRE(status == grains::ResampleStatus::OutOfMemory);
            REQUIRE(channelCount == 0);
        }
        REQUIRE(completed);
    }

    // OutOfMemory is its own status, distinct from the invalid-argument and aborted outcomes.
    const auto invalid = grains::resample(mono, 1, 0.0, 48000.0, never);
    const auto aborted = grains::resample(mono, 1, 48000.0, 44100.0, [] { return true; });
    grains_test::armFailure(1);
    const auto oom = grains::resample(mono, 1, 48000.0, 44100.0, never);
    grains_test::disarmFailure();
    CHECK(oom.status == grains::ResampleStatus::OutOfMemory);
    CHECK(oom.status != invalid.status);
    CHECK(oom.status != aborted.status);
    CHECK(invalid.status != aborted.status);
}

namespace {

const std::function<bool()> kNever = [] { return false; };

struct SweepCase {
    const char* name;
    std::size_t channels;
    double fileRate, targetRate;
};

// Arms the n-th allocation for each n = 1, 2, ... until a build completes without the failure
// firing. Every fired run must report OutOfMemory, return no buffer, and leave the allocation
// balance at zero. Returns the n of the first run that was not hit by the failure.
std::size_t sweep(const SweepCase& c)
{
    const std::vector<float> left(1000, 0.25f), right(1000, -0.25f);
    const ChannelView view[] = {{left.data(), left.size()}, {right.data(), right.size()}};
    for (std::size_t n = 1; n < 1000; ++n) {
        BuildError error = BuildError::None;
        bool hadBuffer = false, fired = false;
        std::size_t allocs = 0, frees = 0;
        {
            grains_test::AllocationScope scope;
            grains_test::armFailure(n);
            {
                const auto r = grains::buildSampleBuffer(view, c.channels, c.fileRate, c.targetRate, kNever);
                grains_test::disarmFailure();
                error = r.error;
                hadBuffer = r.buffer != nullptr;
                fired = grains_test::failureFired();
            }
            allocs = scope.count();
            frees = scope.frees();
        }
        grains_test::disarmFailure();
        INFO(c.name << " n=" << n);
        REQUIRE(allocs == frees); // nothing leaked, on success or failure
        if (!fired) {
            REQUIRE(error == BuildError::None);
            REQUIRE(hadBuffer);
            return n;
        }
        REQUIRE(error == BuildError::OutOfMemory);
        REQUIRE_FALSE(hadBuffer);
    }
    FAIL("the sweep never reached a run without the failure firing");
    return 0;
}

LoadRequest longRequest()
{
    LoadRequest r;
    r.candidates = {std::string(64, 'a') + ".wav", std::string(64, 'b') + ".wav"};
    r.original = std::string(64, 'o') + ".wav";
    r.displayName = std::string(64, 'd') + ".wav";
    r.label = std::string(64, 'l');
    return r;
}

PublishedSource longPublished(double rate)
{
    PublishedSource p;
    p.path = std::string(64, 'p') + ".wav";
    p.original = std::string(64, 'o') + ".wav";
    p.displayName = std::string(64, 'd') + ".wav";
    p.label = std::string(64, 'l');
    p.rate = rate;
    return p;
}

} // namespace

TEST_CASE("builder: every nth allocation failure gives OutOfMemory or success", "[allocfail]")
{
    const SweepCase cases[] = {{"passthrough mono", 1, 48000.0, 48000.0},
                               {"passthrough stereo", 2, 48000.0, 48000.0},
                               {"48k to 44.1k mono", 1, 48000.0, 44100.0},
                               {"48k to 44.1k stereo", 2, 48000.0, 44100.0}};
    for (const auto& c : cases) CHECK(sweep(c) > 1);
}

// R-WD-3 / D3: makeWaveformPeaks is noexcept and returns nullptr when an allocation fails. Every
// allocation it makes (the result object, the bucket vector) fails once; a fired run must return
// null with no leak, and the run the failure misses must return all N buckets.
TEST_CASE("peaks: every nth allocation failure returns null, never throws", "[allocfail][waveform]")
{
    const std::vector<float> left(1000, 0.25f), right(1000, -0.5f);
    const ChannelView view[] = {{left.data(), left.size()}, {right.data(), right.size()}};
    constexpr std::size_t kBuckets = 64;
    bool completed = false;
    for (std::size_t n = 1; n < 100 && !completed; ++n) {
        bool fired = false, hadResult = false;
        std::size_t bucketCount = 0, allocs = 0, frees = 0;
        {
            grains_test::AllocationScope scope;
            grains_test::armFailure(n);
            {
                const auto r = grains::makeWaveformPeaks(view, 2, kBuckets);
                grains_test::disarmFailure(); // before any assertion, which allocates
                fired = grains_test::failureFired();
                hadResult = r != nullptr;
                bucketCount = r ? r->buckets.size() : 0;
            }
            allocs = scope.count();
            frees = scope.frees();
        }
        grains_test::disarmFailure();
        INFO("n=" << n);
        REQUIRE(allocs == frees);
        if (!fired) {
            REQUIRE(hadResult);
            REQUIRE(bucketCount == kBuckets);
            completed = true;
            continue;
        }
        REQUIRE_FALSE(hadResult);
    }
    REQUIRE(completed);
}

TEST_CASE("planner: retarget under allocation failure issues nothing", "[allocfail]")
{
    for (const bool inFlight : {true, false}) {
        LoadPlanner planner;
        planner.retarget(48000.0);
        const std::uint64_t gen = planner.request(longRequest());
        if (!inFlight) {
            (void)planner.take();
            planner.published(longPublished(48000.0));
            planner.settle(gen, LoadOutcome::Published);
        }
        const auto genBefore = planner.gate().current();
        bool completed = false;
        for (std::size_t n = 1; n < 100 && !completed; ++n) {
            grains_test::armFailure(n);
            const bool result = planner.retarget(44100.0);
            grains_test::disarmFailure();
            const bool fired = grains_test::failureFired();
            INFO("inFlight=" << inFlight << " n=" << n);
            if (!fired) {
                REQUIRE(result);
                REQUIRE(planner.target() == 44100.0);
                completed = true;
                continue;
            }
            REQUIRE_FALSE(result);
            REQUIRE(planner.target() == 48000.0);
            REQUIRE(planner.gate().current() == genBefore);
        }
        REQUIRE(completed);
    }
}

TEST_CASE("planner: settle under allocation failure issues nothing and does not throw", "[allocfail]")
{
    LoadPlanner planner;
    planner.retarget(44100.0);
    const std::uint64_t gen = planner.request(longRequest());
    (void)planner.take();
    planner.published(longPublished(48000.0)); // published at another rate than the target
    bool completed = false;
    for (std::size_t n = 1; n < 100 && !completed; ++n) {
        const auto genBefore = planner.gate().current();
        std::string status(64, 's'); // built before arming so only settle's own copies can fail
        bool issued = true;
        grains_test::armFailure(n);
        try {
            issued = planner.settle(gen, LoadOutcome::Failed, std::move(status));
        } catch (...) {
            grains_test::disarmFailure();
            FAIL("settle threw under allocation failure");
        }
        grains_test::disarmFailure();
        INFO("n=" << n);
        if (!grains_test::failureFired()) {
            REQUIRE(issued); // the run the failure missed issues the R4-001 Reload
            completed = true;
            continue;
        }
        REQUIRE_FALSE(issued);
        REQUIRE(planner.gate().current() == genBefore);
        REQUIRE_FALSE(planner.take().has_value());
    }
    REQUIRE(completed);
}

#endif // GRAINS_ALLOC_SWEEPS
