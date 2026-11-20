// SPDX-License-Identifier: AGPL-3.0-or-later
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <grains/Resampler.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <limits>
#include <vector>

using grains::ChannelView;
using grains::ResampleOptions;
using grains::ResampleResult;
using grains::ResampleStatus;

namespace {

const std::function<bool()> kNever = [] { return false; };

std::vector<float> noise(std::size_t n, unsigned seed)
{
    std::vector<float> v(n);
    unsigned s = seed;
    for (auto& x : v) {
        s = s * 1664525u + 1013904223u;
        x = static_cast<float>(static_cast<int>(s >> 8) - (1 << 23)) / static_cast<float>(1 << 23);
    }
    return v;
}

ResampleResult run1(const std::vector<float>& in, double src, double dst, ResampleOptions o = {})
{
    const ChannelView v[] = {{in.data(), in.size()}};
    return grains::resample(v, 1, src, dst, kNever, o);
}

constexpr double kPi = 3.14159265358979323846;
constexpr std::size_t kTrim = 200; // output frames ignored at each end (kernel half-width is below this)

std::vector<float> sine(std::size_t n, double hz, double rate, double amp = 1.0)
{
    std::vector<float> v(n);
    for (std::size_t i = 0; i < n; ++i)
        v[i] = static_cast<float>(amp * std::sin(2.0 * kPi * hz * static_cast<double>(i) / rate));
    return v;
}

struct Fit {
    double amp;      // fitted amplitude
    double phase;    // fitted phase relative to sin(2 pi f m / rate), radians
    double residual; // RMS of what the fitted sine does not explain
};

// Double-precision least-squares fit of a*cos + b*sin over y[begin, end). No FFT involved.
Fit fitSine(const std::vector<float>& y, double hz, double rate, std::size_t begin, std::size_t end)
{
    double scc = 0, sss = 0, scs = 0, syc = 0, sys = 0;
    for (std::size_t m = begin; m < end; ++m) {
        const double w = 2.0 * kPi * hz * static_cast<double>(m) / rate;
        const double c = std::cos(w), s = std::sin(w), v = y[m];
        scc += c * c;
        sss += s * s;
        scs += c * s;
        syc += v * c;
        sys += v * s;
    }
    const double det = scc * sss - scs * scs;
    const double a = (syc * sss - sys * scs) / det;
    const double b = (sys * scc - syc * scs) / det;
    double res = 0;
    for (std::size_t m = begin; m < end; ++m) {
        const double w = 2.0 * kPi * hz * static_cast<double>(m) / rate;
        const double e = static_cast<double>(y[m]) - (a * std::cos(w) + b * std::sin(w));
        res += e * e;
    }
    return {std::hypot(a, b), std::atan2(a, b), std::sqrt(res / static_cast<double>(end - begin))};
}

double rms(const std::vector<float>& y, std::size_t begin, std::size_t end)
{
    double acc = 0;
    for (std::size_t m = begin; m < end; ++m) acc += static_cast<double>(y[m]) * y[m];
    return std::sqrt(acc / static_cast<double>(end - begin));
}

double dbOf(double ratio)
{
    return 20.0 * std::log10(ratio);
}

} // namespace

TEST_CASE("resample: output length is max(1, round(n * dst / src))", "[resampler]")
{
    CHECK(grains::resampledFrames(48000, 96000.0, 48000.0) == 24000);
    CHECK(grains::resampledFrames(44100, 44100.0, 48000.0) == 44100 * 48000 / 44100);
    CHECK(grains::resampledFrames(1, 96000.0, 8000.0) == 1);
    CHECK(grains::resampledFrames(10, 3.0, 1.0) == 3);  // 3.33 rounds down
    CHECK(grains::resampledFrames(11, 3.0, 1.0) == 4);  // 3.67 rounds up

    const auto a = noise(48000, 1);
    const auto d = run1(a, 96000.0, 48000.0);
    REQUIRE(d.status == ResampleStatus::Ok);
    REQUIRE(d.channels.size() == 1);
    CHECK(d.channels[0].size() == 24000);

    const auto b = noise(44100, 2);
    const auto u = run1(b, 44100.0, 48000.0);
    REQUIRE(u.status == ResampleStatus::Ok);
    CHECK(u.channels[0].size() == 48000);

    const auto one = noise(1, 3);
    const auto o = run1(one, 96000.0, 8000.0);
    REQUIRE(o.status == ResampleStatus::Ok);
    CHECK(o.channels[0].size() == 1);

    const ChannelView st[] = {{a.data(), a.size()}, {a.data(), a.size()}};
    const auto s = grains::resample(st, 2, 96000.0, 48000.0, kNever);
    REQUIRE(s.status == ResampleStatus::Ok);
    REQUIRE(s.channels.size() == 2);
    CHECK(s.channels[0].size() == s.channels[1].size());
    CHECK(s.channels[0].size() == 24000);
}

TEST_CASE("resample: invalid arguments are rejected without output", "[resampler]")
{
    const auto a = noise(100, 4), b = noise(70, 5);
    const ChannelView good[] = {{a.data(), a.size()}};
    const ChannelView empty[] = {{a.data(), 0}};
    const ChannelView nullData[] = {{nullptr, 100}};
    const ChannelView mismatch[] = {{a.data(), a.size()}, {b.data(), b.size()}};
    const double inf = std::numeric_limits<double>::infinity();
    const double nan = std::numeric_limits<double>::quiet_NaN();

    struct Case {
        const char* name;
        const ChannelView* ch;
        std::size_t n;
        double src, dst;
    };
    const Case cases[] = {
        {"zero channels", good, 0, 96000.0, 48000.0},
        {"null array", nullptr, 1, 96000.0, 48000.0},
        {"null data", nullData, 1, 96000.0, 48000.0},
        {"zero frames", empty, 1, 96000.0, 48000.0},
        {"three channels", good, 3, 96000.0, 48000.0},
        {"mismatched lengths", mismatch, 2, 96000.0, 48000.0},
        {"src zero", good, 1, 0.0, 48000.0},
        {"src negative", good, 1, -1.0, 48000.0},
        {"src NaN", good, 1, nan, 48000.0},
        {"src Inf", good, 1, inf, 48000.0},
        {"dst zero", good, 1, 96000.0, 0.0},
        {"dst negative", good, 1, 96000.0, -1.0},
        {"dst NaN", good, 1, 96000.0, nan},
        {"dst Inf", good, 1, 96000.0, inf},
    };
    for (const Case& c : cases) {
        INFO(c.name);
        const auto r = grains::resample(c.ch, c.n, c.src, c.dst, kNever);
        CHECK(r.status == ResampleStatus::InvalidArgument);
        CHECK(r.channels.empty());
        CHECK_FALSE(r.capped);
    }
    // Control: the same data with valid arguments succeeds, so the rejections above are real.
    const auto ok = grains::resample(good, 1, 96000.0, 48000.0, kNever);
    CHECK(ok.status == ResampleStatus::Ok);
    CHECK(grains::resampledFrames(100, 0.0, 48000.0) == 0);
    CHECK(grains::resampledFrames(0, 96000.0, 48000.0) == 0);
    CHECK(grains::resampledFrames(100, nan, 48000.0) == 0);
}

TEST_CASE("resample: equal rates copy bytes exactly", "[resampler]")
{
    const auto l = noise(5000, 6), r = noise(5000, 7);
    const ChannelView mono[] = {{l.data(), l.size()}};
    const auto m = grains::resample(mono, 1, 48000.0, 48000.0, kNever);
    REQUIRE(m.status == ResampleStatus::Ok);
    REQUIRE(m.channels.size() == 1);
    REQUIRE(m.channels[0].size() == l.size());
    CHECK(std::memcmp(m.channels[0].data(), l.data(), l.size() * sizeof(float)) == 0);

    const ChannelView st[] = {{l.data(), l.size()}, {r.data(), r.size()}};
    const auto s = grains::resample(st, 2, 44100.0, 44100.0, kNever);
    REQUIRE(s.status == ResampleStatus::Ok);
    REQUIRE(s.channels.size() == 2);
    CHECK(std::memcmp(s.channels[0].data(), l.data(), l.size() * sizeof(float)) == 0);
    CHECK(std::memcmp(s.channels[1].data(), r.data(), r.size() * sizeof(float)) == 0);
    CHECK(s.channels[0] != s.channels[1]); // the channels really are distinct inputs
}

TEST_CASE("resample: passband is flat within 0.1 dB with zero phase error", "[resampler]")
{
    struct Pair {
        double src, dst;
        std::size_t n;
    };
    const Pair pairs[] = {{96000.0, 44100.0, 24000}, {44100.0, 96000.0, 12000}};
    const double tones[] = {100.0, 1000.0, 5000.0, 10000.0, 15000.0, 19000.0};
    for (const Pair& p : pairs) {
        const double edge = 0.9 * 0.5 * std::min(p.src, p.dst); // 0.9 x cutoff in Hz
        std::vector<double> freqs(std::begin(tones), std::end(tones));
        freqs.push_back(edge);
        for (const double hz : freqs) {
            INFO("src " << p.src << " dst " << p.dst << " tone " << hz);
            const auto in = sine(p.n, hz, p.src);
            const auto out = run1(in, p.src, p.dst);
            REQUIRE(out.status == ResampleStatus::Ok);
            const auto& y = out.channels[0];
            REQUIRE(y.size() == grains::resampledFrames(p.n, p.src, p.dst));
            const Fit f = fitSine(y, hz, p.dst, kTrim, y.size() - kTrim);
            CHECK(std::fabs(dbOf(f.amp)) <= 0.1);
            CHECK(std::fabs(f.phase) <= 1e-4);
        }
    }
}

TEST_CASE("resample: an impulse lands at i * dst / src", "[resampler]")
{
    std::vector<float> in(4000, 0.0f);
    in[2000] = 1.0f;
    const auto down = run1(in, 96000.0, 48000.0); // ratio 2:1, cutoff 0.5 -> centre tap 0.5
    REQUIRE(down.status == ResampleStatus::Ok);
    const auto& d = down.channels[0];
    REQUIRE(d.size() == 2000);
    CHECK(d[1000] == Catch::Approx(0.5f).margin(1e-5));
    CHECK(std::fabs(d[999]) < 1e-5f);
    CHECK(std::fabs(d[1001]) < 1e-5f);
    CHECK(std::max_element(d.begin(), d.end()) - d.begin() == 1000);

    const auto up = run1(in, 44100.0, 88200.0); // ratio 1:2, cutoff 1 -> unit centre tap
    REQUIRE(up.status == ResampleStatus::Ok);
    const auto& u = up.channels[0];
    REQUIRE(u.size() == 8000);
    CHECK(u[4000] == Catch::Approx(1.0f).margin(1e-5));
    CHECK(std::fabs(u[3998]) < 1e-5f); // sinc zero crossings on the input grid
    CHECK(std::fabs(u[4002]) < 1e-5f);
    CHECK(std::max_element(u.begin(), u.end()) - u.begin() == 4000);
    CHECK(u[4001] > 0.4f); // the half-sample point of the interpolated impulse is non-trivial
}

TEST_CASE("resample: out-of-band tones are rejected by at least 100 dB", "[resampler]")
{
    struct Pair {
        double src, dst;
    };
    const Pair pairs[] = {{96000.0, 44100.0}, {96000.0, 48000.0}};
    const double mults[] = {1.1, 1.3, 1.6, 1.9};
    const std::size_t n = 24000;
    for (const Pair& p : pairs) {
        // Control: a passband tone survives at unity, so the rejection below is not a dead output.
        const auto pass = run1(sine(n, 5000.0, p.src), p.src, p.dst);
        REQUIRE(pass.status == ResampleStatus::Ok);
        const auto& yp = pass.channels[0];
        CHECK(std::fabs(dbOf(rms(yp, kTrim, yp.size() - kTrim) * std::sqrt(2.0))) <= 0.1);

        for (const double mult : mults) {
            const double hz = mult * 0.5 * p.dst;
            INFO("src " << p.src << " dst " << p.dst << " tone " << hz);
            const auto out = run1(sine(n, hz, p.src), p.src, p.dst);
            REQUIRE(out.status == ResampleStatus::Ok);
            const auto& y = out.channels[0];
            const double level = dbOf(rms(y, kTrim, y.size() - kTrim) * std::sqrt(2.0)); // vs unit amplitude
            CHECK(level <= -100.0);
        }
    }
}

TEST_CASE("resample: upsampling leaves no image above -100 dB", "[resampler]")
{
    struct Case {
        double src, dst, hz;
        std::size_t n;
    };
    const Case cases[] = {{44100.0, 96000.0, 10000.0, 12000}, {22050.0, 48000.0, 5000.0, 6000}};
    for (const Case& c : cases) {
        INFO("src " << c.src << " dst " << c.dst << " tone " << c.hz);
        const auto out = run1(sine(c.n, c.hz, c.src), c.src, c.dst);
        REQUIRE(out.status == ResampleStatus::Ok);
        const auto& y = out.channels[0];
        const Fit f = fitSine(y, c.hz, c.dst, kTrim, y.size() - kTrim);
        CHECK(std::fabs(dbOf(f.amp)) <= 0.1); // the tone itself is present
        const double residualDb = dbOf(f.residual / (f.amp / std::sqrt(2.0)));
        CHECK(residualDb <= -100.0);
    }
}

TEST_CASE("resample: finite input gives finite output at extreme ratios", "[resampler]")
{
    struct Pair {
        double src, dst;
    };
    const Pair pairs[] = {{22050.0, 192000.0}, {192000.0, 22050.0}};
    for (const Pair& p : pairs) {
        std::vector<float> zeros(3000, 0.0f), square(3000), single(1, 1.0f);
        for (std::size_t i = 0; i < square.size(); ++i) square[i] = (i / 7) % 2 == 0 ? 1.0f : -1.0f;
        const std::vector<float>* inputs[] = {&zeros, &square, &single};
        for (const auto* in : inputs) {
            INFO("src " << p.src << " dst " << p.dst << " frames " << in->size());
            const auto out = run1(*in, p.src, p.dst);
            REQUIRE(out.status == ResampleStatus::Ok);
            const auto& y = out.channels[0];
            REQUIRE(y.size() == grains::resampledFrames(in->size(), p.src, p.dst));
            bool allFinite = true;
            float peak = 0.0f;
            for (const float v : y) {
                allFinite = allFinite && std::isfinite(v);
                peak = std::max(peak, std::fabs(v));
            }
            CHECK(allFinite);
            if (in == &zeros) CHECK(peak == 0.0f);
            else CHECK(peak > 0.0f); // non-silent input yields non-silent output
            CHECK(peak < 1.5f);      // a unit square overshoots (Gibbs) but stays bounded
        }
    }
}

TEST_CASE("resample: output is deterministic and independent of the chunk size", "[resampler]")
{
    const auto l = noise(9000, 8), r = noise(9000, 9);
    const ChannelView st[] = {{l.data(), l.size()}, {r.data(), r.size()}};
    ResampleOptions small;
    small.chunkFrames = 777;
    const auto a = grains::resample(st, 2, 96000.0, 44100.0, kNever);
    const auto b = grains::resample(st, 2, 96000.0, 44100.0, kNever);
    const auto c = grains::resample(st, 2, 96000.0, 44100.0, kNever, small);
    REQUIRE(a.status == ResampleStatus::Ok);
    REQUIRE(c.status == ResampleStatus::Ok);
    REQUIRE(a.channels.size() == 2);
    CHECK(a.channels[0].size() == 4134); // round(9000 * 44100 / 96000)
    CHECK(a.channels[0] == b.channels[0]);
    CHECK(a.channels[1] == b.channels[1]);
    CHECK(a.channels[0] == c.channels[0]);
    CHECK(a.channels[1] == c.channels[1]);
    CHECK(a.channels[0] != a.channels[1]); // not a vacuous comparison of identical data
}

TEST_CASE("resample: identical stereo channels stay bit-identical", "[resampler]")
{
    const auto l = noise(6000, 10);
    const ChannelView st[] = {{l.data(), l.size()}, {l.data(), l.size()}};
    const auto out = grains::resample(st, 2, 44100.0, 48000.0, kNever);
    REQUIRE(out.status == ResampleStatus::Ok);
    REQUIRE(out.channels.size() == 2);
    REQUIRE(out.channels[0].size() == 6531);
    CHECK(out.channels[0] == out.channels[1]);
    CHECK(rms(out.channels[0], 0, out.channels[0].size()) > 0.1); // real signal, not two empty channels
}

TEST_CASE("resample: abort is honoured and leaves no output", "[resampler]")
{
    const auto in = noise(60 * 96000 / 10, 11); // 6 s at 96 kHz -> 288000 output frames at 48 kHz
    int polls = 0;
    const std::function<bool()> third = [&] { return ++polls >= 3; };
    const ChannelView v[] = {{in.data(), in.size()}};
    const auto r = grains::resample(v, 1, 96000.0, 48000.0, third);
    CHECK(r.status == ResampleStatus::Aborted);
    CHECK(r.channels.empty());
    CHECK(polls == 3); // polled before chunk 1, 2 and 3; chunks 1 and 2 were rendered, 3 was not

    int polls2 = 0;
    const std::function<bool()> first = [&] { ++polls2; return true; };
    const auto r0 = grains::resample(v, 1, 96000.0, 48000.0, first);
    CHECK(r0.status == ResampleStatus::Aborted);
    CHECK(polls2 == 1);

    // Never-abort control: the same input completes with the full length.
    int polls3 = 0;
    const std::function<bool()> never = [&] { ++polls3; return false; };
    const auto ok = grains::resample(v, 1, 96000.0, 48000.0, never);
    REQUIRE(ok.status == ResampleStatus::Ok);
    CHECK(ok.channels[0].size() == 288000);
    CHECK(polls3 == 5); // ceil(288000 / 65536) chunks, one poll each
}

TEST_CASE("resample: maxFrames cuts the output and reports it", "[resampler]")
{
    const auto in = noise(10000, 12);
    ResampleOptions cap;
    cap.maxFrames = 3000;
    const auto cut = run1(in, 96000.0, 48000.0, cap); // full length would be 5000
    REQUIRE(cut.status == ResampleStatus::Ok);
    CHECK(cut.channels[0].size() == 3000);
    CHECK(cut.capped);

    const auto full = run1(in, 96000.0, 48000.0);
    REQUIRE(full.status == ResampleStatus::Ok);
    CHECK_FALSE(full.capped);
    CHECK(std::equal(cut.channels[0].begin(), cut.channels[0].end(), full.channels[0].begin())); // a prefix

    cap.maxFrames = 5000; // exactly the full length: nothing is cut
    const auto exact = run1(in, 96000.0, 48000.0, cap);
    CHECK(exact.channels[0].size() == 5000);
    CHECK_FALSE(exact.capped);

    cap.maxFrames = 4000; // equal rates cap too
    const auto same = run1(in, 48000.0, 48000.0, cap);
    CHECK(same.channels[0].size() == 4000);
    CHECK(same.capped);
}

TEST_CASE("resample: a ratio beyond the kernel limit is rejected", "[resampler]")
{
    const auto in = noise(1000, 13);
    CHECK(run1(in, 1.0e6, 100.0).status == ResampleStatus::InvalidArgument); // 10000:1
    CHECK(run1(in, 96000.0, 96.0).status == ResampleStatus::Ok);             // 1000:1 is the limit
}

// R-RS-9. Hidden (the "." tag) so catch_discover_tests and CI skip it. Run by hand in Release:
//   grains_tests "resample: 300 s stereo 96k to 48k within 3 s"
TEST_CASE("resample: 300 s stereo 96k to 48k within 3 s", "[.][perf]")
{
    const std::size_t n = 300u * 96000u;
    const auto l = noise(n, 14), r = noise(n, 15);
    const ChannelView st[] = {{l.data(), l.size()}, {r.data(), r.size()}};
    const auto t0 = std::chrono::steady_clock::now();
    const auto out = grains::resample(st, 2, 96000.0, 48000.0, kNever);
    const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    REQUIRE(out.status == ResampleStatus::Ok);
    CHECK(out.channels[0].size() == 300u * 48000u);
    WARN("resample wall time: " << seconds << " s");
    CHECK(seconds <= 3.0);
}

// R-RS-9. Run by hand in Release; the times are recorded in the apply-progress artifact.
TEST_CASE("resample: 300 s stereo 96k to 44.1k within 6 s", "[.][perf]")
{
    const std::size_t n = 300u * 96000u;
    const auto l = noise(n, 17), r = noise(n, 18);
    const ChannelView st[] = {{l.data(), l.size()}, {r.data(), r.size()}};
    const auto t0 = std::chrono::steady_clock::now();
    const auto out = grains::resample(st, 2, 96000.0, 44100.0, kNever);
    const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    REQUIRE(out.status == ResampleStatus::Ok);
    WARN("resample wall time: " << seconds << " s");
    REQUIRE(seconds <= 6.0);
}

// R-RS-9 informational: no budget, the time is only reported.
TEST_CASE("resample: 300 s stereo 384k to 44.1k reports its time", "[.][perf]")
{
    const std::size_t n = 300u * 384000u;
    const auto l = noise(n, 19), r = noise(n, 20);
    const ChannelView st[] = {{l.data(), l.size()}, {r.data(), r.size()}};
    const auto t0 = std::chrono::steady_clock::now();
    const auto out = grains::resample(st, 2, 384000.0, 44100.0, kNever);
    const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    REQUIRE(out.status == ResampleStatus::Ok);
    WARN("resample wall time: " << seconds << " s");
}
