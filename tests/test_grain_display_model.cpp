// SPDX-License-Identifier: AGPL-3.0-or-later
#include <catch2/catch_test_macros.hpp>

#include <grains/GrainDisplayModel.h>
#include <grains/GrainSnapshot.h>

#include <cstddef>

using grains::GrainDisplayModel;
using grains::GrainSnapshot;

namespace {

// A frame with `n` marks whose positions are 0.1, 0.2, ... so copies can be told apart.
GrainSnapshot frame(int n)
{
    GrainSnapshot s;
    s.count = n;
    for (int i = 0; i < n; ++i) {
        s.marks[static_cast<std::size_t>(i)].position = 0.1f * static_cast<float>(i + 1);
        s.marks[static_cast<std::size_t>(i)].slot = static_cast<std::uint16_t>(i);
    }
    return s;
}

void idle(GrainDisplayModel& m, int ticks)
{
    for (int i = 0; i < ticks; ++i) m.tick(nullptr);
}

} // namespace

TEST_CASE("display model: constants are named, distinct and consistent", "[grain_display_model]")
{
    static_assert(GrainDisplayModel::kPollHz == 30, "poll rate");
    static_assert(GrainDisplayModel::kStaleMs == 250, "stale timeout");
    static_assert(GrainDisplayModel::kStaleTicks == 8, "ceil(250 ms * 30 Hz / 1000)");
    static_assert(GrainDisplayModel::kPollHz != GrainDisplayModel::kStaleMs &&
                      GrainDisplayModel::kStaleMs != GrainDisplayModel::kStaleTicks &&
                      GrainDisplayModel::kPollHz != GrainDisplayModel::kStaleTicks,
                  "distinct constants");
    CHECK(GrainDisplayModel::kStaleTicks * 1000 >= GrainDisplayModel::kStaleMs * GrainDisplayModel::kPollHz);
}

TEST_CASE("display model: a fresh frame is shown and requests a repaint", "[grain_display_model]")
{
    GrainDisplayModel m;
    CHECK(m.shown().count == 0);
    const GrainSnapshot f = frame(5);
    CHECK(m.tick(&f));
    REQUIRE(m.shown().count == 5);
    CHECK(m.shown().marks[4].position == f.marks[4].position);
    CHECK(m.shown().marks[4].slot == 4);

    const GrainSnapshot g = frame(2); // fewer marks: the old count was above 0, so repaint
    CHECK(m.tick(&g));
    CHECK(m.shown().count == 2);
}

TEST_CASE("display model: an empty frame after an empty frame does not repaint", "[grain_display_model]")
{
    GrainDisplayModel m;
    const GrainSnapshot none = frame(0);
    CHECK_FALSE(m.tick(&none));
    CHECK_FALSE(m.tick(&none));

    const GrainSnapshot some = frame(3);
    CHECK(m.tick(&some));
    CHECK(m.tick(&none)); // marks to no marks must repaint once to clear
    CHECK(m.shown().count == 0);
    CHECK_FALSE(m.tick(&none));
}

TEST_CASE("display model: marks are held until the stale tick, which clears once", "[grain_display_model]")
{
    GrainDisplayModel m;
    const GrainSnapshot f = frame(4);
    REQUIRE(m.tick(&f));
    for (int i = 1; i < GrainDisplayModel::kStaleTicks; ++i) { // 7 idle ticks: held, no repaint
        INFO("idle tick " << i);
        CHECK_FALSE(m.tick(nullptr));
        CHECK(m.shown().count == 4);
    }
    CHECK(m.tick(nullptr)); // the 8th idle tick clears and requests the repaint
    CHECK(m.shown().count == 0);
    for (int i = 0; i < 20; ++i) CHECK_FALSE(m.tick(nullptr)); // later idle ticks stay quiet
}

TEST_CASE("display model: a fresh frame restarts the idle count", "[grain_display_model]")
{
    GrainDisplayModel m;
    const GrainSnapshot f = frame(4);
    m.tick(&f);
    idle(m, GrainDisplayModel::kStaleTicks - 1);
    CHECK(m.tick(&f)); // a frame just before the timeout keeps the marks alive
    idle(m, GrainDisplayModel::kStaleTicks - 1);
    CHECK(m.shown().count == 4);
    CHECK(m.tick(nullptr));
    CHECK(m.shown().count == 0);
}

TEST_CASE("display model: a fresh frame resumes after stale", "[grain_display_model]")
{
    GrainDisplayModel m;
    const GrainSnapshot f = frame(4);
    m.tick(&f);
    idle(m, GrainDisplayModel::kStaleTicks + 5);
    REQUIRE(m.shown().count == 0);
    const GrainSnapshot g = frame(6);
    CHECK(m.tick(&g));
    CHECK(m.shown().count == 6);
    CHECK(m.shown().marks[5].position == g.marks[5].position);
}

TEST_CASE("display model: reset clears the marks without a repaint request", "[grain_display_model]")
{
    GrainDisplayModel m;
    const GrainSnapshot f = frame(4);
    m.tick(&f);
    m.reset();
    CHECK(m.shown().count == 0);
    CHECK_FALSE(m.tick(nullptr)); // nothing left to clear
    CHECK(m.tick(&f));            // and the model is usable again
    CHECK(m.shown().count == 4);
}
