#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <grains/WindowTable.h>

#include <cmath>
#include <random>

namespace {
double hann(double p)
{
    return 0.5 * (1.0 - std::cos(2.0 * 3.14159265358979323846 * p));
}
} // namespace

TEST_CASE("window: endpoints are zero and centre is one", "[window]")
{
    const grains::WindowTable w;
    CHECK(w.at(0.0f) == 0.0f);
    CHECK(w.at(1.0f) == 0.0f);
    CHECK(w.at(0.5f) == Catch::Approx(1.0).margin(1e-3));
}

TEST_CASE("window: symmetric around the centre", "[window]")
{
    const grains::WindowTable w;
    for (int i = 0; i <= 100; ++i) {
        const float p = static_cast<float>(i) / 100.0f;
        CHECK(w.at(p) == Catch::Approx(w.at(1.0f - p)).margin(1e-5));
    }
}

TEST_CASE("window: matches Hann at 1000 random phases within 1e-3", "[window]")
{
    const grains::WindowTable w;
    std::mt19937 gen(12345);
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    for (int i = 0; i < 1000; ++i) {
        const float p = dist(gen);
        CHECK(static_cast<double>(w.at(p)) == Catch::Approx(hann(p)).margin(1e-3));
    }
}

TEST_CASE("window: out-of-range phase is clamped", "[window]")
{
    const grains::WindowTable w;
    CHECK(w.at(-0.5f) == 0.0f);
    CHECK(w.at(1.5f) == 0.0f);
}

// R-GR-1 range sweep: every table-entry boundary phase (frac == 0, including the last interval
// edge), a dense sweep between them and the largest float below 1 must stay finite and in [0, 1].
TEST_CASE("window: values stay finite and within [0, 1] over boundary and dense phases", "[window]")
{
    const grains::WindowTable w;
    auto inRange = [](float v) { return std::isfinite(v) && v >= 0.0f && v <= 1.0f; };

    int checked = 0;
    int inside = 0;
    float maxValue = 0.0f;
    auto visit = [&](float phase) {
        const float v = w.at(phase);
        ++checked;
        if (inRange(v)) ++inside;
        if (v > maxValue) maxValue = v;
    };
    for (int k = 0; k < grains::WindowTable::kSize; ++k)
        visit(static_cast<float>(k) / static_cast<float>(grains::WindowTable::kSize - 1));
    for (int i = 0; i <= 100000; ++i) visit(static_cast<float>(i) / 100000.0f);
    visit(std::nextafter(1.0f, 0.0f));

    CHECK(checked == grains::WindowTable::kSize + 100001 + 1);
    CHECK(inside == checked); // every sampled value is finite and in [0, 1]
    CHECK(maxValue > 0.99f);  // the sweep really reaches the window peak
}
