// SPDX-License-Identifier: AGPL-3.0-or-later
#include <grains/Resampler.h>

#include <algorithm>
#include <cstddef>
#include <cmath>
#include <new>

namespace grains {

namespace {

bool validRate(double r) noexcept
{
    return std::isfinite(r) && r > 0.0;
}

bool validInput(const ChannelView* channels, std::size_t numChannels, double src, double dst) noexcept
{
    if (channels == nullptr || numChannels < 1 || numChannels > 2) return false;
    if (!validRate(src) || !validRate(dst)) return false;
    for (std::size_t c = 0; c < numChannels; ++c) {
        if (channels[c].data == nullptr || channels[c].frames == 0) return false;
        if (channels[c].frames != channels[0].frames) return false;
    }
    return true;
}

// Design A (see openspec design): Kaiser beta for 110 dB, half-width in periods of the lower rate, and
// the number of table rows per input-sample zero crossing.
constexpr double kPi = 3.14159265358979323846;
constexpr double kKaiserBeta = 11.16;      // 0.1102 * (110 - 8.7)
constexpr double kHalfWidthPeriods = 36.0; // kernel half-width in lower-rate sample periods
constexpr double kRowsPerCrossing = 1024.0;
constexpr double kMaxDownRatio = 1000.0;   // longest supported decimation (kernel stays below ~72k taps)

// Modified Bessel function of the first kind, order 0 (power series, converges fast for x <= ~12).
double besselI0(double x) noexcept
{
    const double q = 0.25 * x * x;
    double term = 1.0, sum = 1.0;
    for (int k = 1; k < 200; ++k) {
        term *= q / (static_cast<double>(k) * static_cast<double>(k));
        sum += term;
        if (term < sum * 1e-17) break;
    }
    return sum;
}

double sinc(double x) noexcept
{
    if (x == 0.0) return 1.0;
    const double px = kPi * x;
    return std::sin(px) / px;
}

// Polyphase table: rows[r] holds the 2K+1 taps for an output point at fractional input offset r / P.
// Row r, tap j sits at distance (j - r/P) input samples from the output point.
struct KernelTable {
    std::ptrdiff_t half = 0;       // K: taps cover input offsets -K..K
    std::size_t rows = 0; // P: rows per input sample (the table holds P + 1 rows, the last is frac = 1)
    std::size_t width = 0; // 2K + 1
    std::vector<float> taps;
};

// False when the kernel would be unreasonably long (a ratio beyond kMaxDownRatio : 1).
bool buildTable(double fc, KernelTable& t)
{
    if (fc < 1.0 / kMaxDownRatio) return false;
    t.half = static_cast<std::ptrdiff_t>(std::ceil(kHalfWidthPeriods / fc));
    t.rows = static_cast<std::size_t>(std::ceil(kRowsPerCrossing * fc));
    t.width = static_cast<std::size_t>(2 * t.half + 1);
    t.taps.assign((t.rows + 1) * t.width, 0.0f);
    const double invI0 = 1.0 / besselI0(kKaiserBeta);
    const double halfD = static_cast<double>(t.half);
    std::vector<double> row(t.width);
    for (std::size_t r = 0; r <= t.rows; ++r) {
        const double frac = static_cast<double>(r) / static_cast<double>(t.rows);
        double sum = 0.0;
        for (std::size_t k = 0; k < t.width; ++k) {
            const double x = static_cast<double>(static_cast<std::ptrdiff_t>(k) - t.half) - frac;
            const double u = x / halfD;
            double v = 0.0;
            if (u > -1.0 && u < 1.0) v = fc * sinc(fc * x) * besselI0(kKaiserBeta * std::sqrt(1.0 - u * u)) * invI0;
            row[k] = v;
            sum += v;
        }
        for (std::size_t k = 0; k < t.width; ++k) t.taps[r * t.width + k] = static_cast<float>(row[k] / sum);
    }
    return true;
}

// Dot product of one table row with the input around index i0, in double with four fixed partial sums.
// Taps that fall outside [0, n) are skipped (zero padding), so the edges need no extra buffer.
double dotRow(const float* row, std::ptrdiff_t half, const float* in, std::ptrdiff_t n, std::ptrdiff_t i0) noexcept
{
    const std::ptrdiff_t jlo = std::max(-half, -i0);
    const std::ptrdiff_t jhi = std::min(half, n - 1 - i0);
    const float* w = row + (jlo + half);
    const float* x = in + (i0 + jlo);
    const std::ptrdiff_t count = jhi - jlo + 1;
    double a0 = 0.0, a1 = 0.0, a2 = 0.0, a3 = 0.0;
    std::ptrdiff_t k = 0;
    for (; k + 4 <= count; k += 4) {
        a0 += static_cast<double>(w[k]) * static_cast<double>(x[k]);
        a1 += static_cast<double>(w[k + 1]) * static_cast<double>(x[k + 1]);
        a2 += static_cast<double>(w[k + 2]) * static_cast<double>(x[k + 2]);
        a3 += static_cast<double>(w[k + 3]) * static_cast<double>(x[k + 3]);
    }
    for (; k < count; ++k) a0 += static_cast<double>(w[k]) * static_cast<double>(x[k]);
    return (a0 + a1) + (a2 + a3);
}

// Output frame m, computed on its own from its exact input time t = m * ratio.
float renderFrame(const KernelTable& t, const float* in, std::ptrdiff_t n, std::size_t m, double ratio) noexcept
{
    const double pos = static_cast<double>(m) * ratio;
    const double fl = std::floor(pos);
    const std::ptrdiff_t i0 = static_cast<std::ptrdiff_t>(fl);
    const double u = (pos - fl) * static_cast<double>(t.rows);
    const auto r = static_cast<std::size_t>(u);
    const double alpha = u - static_cast<double>(r);
    double v = dotRow(&t.taps[r * t.width], t.half, in, n, i0);
    if (alpha > 0.0) {
        const double v1 = dotRow(&t.taps[(r + 1) * t.width], t.half, in, n, i0);
        v += alpha * (v1 - v);
    }
    return static_cast<float>(v);
}

} // namespace

std::size_t resampledFrames(std::size_t n, double src, double dst) noexcept
{
    if (n == 0 || !validRate(src) || !validRate(dst)) return 0;
    const double frames = std::round(static_cast<double>(n) * dst / src);
    if (!(frames < static_cast<double>(SIZE_MAX / 2))) return SIZE_MAX / 2; // unreachable for real inputs
    return std::max<std::size_t>(1, static_cast<std::size_t>(frames));
}

ResampleResult resample(const ChannelView* channels, std::size_t numChannels, double src, double dst,
                        const std::function<bool()>& shouldAbort, ResampleOptions options) noexcept
{
    ResampleResult result;
    if (!validInput(channels, numChannels, src, dst)) return result;
    const std::size_t n = channels[0].frames;
    try {
        if (src == dst) {
            const std::size_t keep = std::min(n, options.maxFrames);
            for (std::size_t c = 0; c < numChannels; ++c)
                result.channels.emplace_back(channels[c].data, channels[c].data + keep);
            result.capped = keep < n;
        } else {
            const std::size_t full = resampledFrames(n, src, dst);
            const std::size_t out = std::min(full, options.maxFrames);
            KernelTable table;
            if (!buildTable(std::min(1.0, dst / src), table)) return result; // InvalidArgument
            result.capped = out < full;
            const double ratio = src / dst;
            const std::size_t chunk = std::max<std::size_t>(1, options.chunkFrames);
            for (std::size_t c = 0; c < numChannels; ++c) result.channels.emplace_back(out);
            for (std::size_t begin = 0; begin < out; begin += chunk) {
                if (shouldAbort && shouldAbort()) {
                    result.channels.clear();
                    result.capped = false;
                    result.status = ResampleStatus::Aborted;
                    return result;
                }
                const std::size_t end = std::min(out, begin + chunk);
                for (std::size_t c = 0; c < numChannels; ++c) {
                    const float* in = channels[c].data;
                    float* dstp = result.channels[c].data();
                    for (std::size_t m = begin; m < end; ++m)
                        dstp[m] = renderFrame(table, in, static_cast<std::ptrdiff_t>(n), m, ratio);
                }
            }
        }
        result.status = ResampleStatus::Ok;
    } catch (const std::bad_alloc&) {
        result.channels.clear();
        result.status = ResampleStatus::OutOfMemory;
    }
    return result;
}

} // namespace grains
