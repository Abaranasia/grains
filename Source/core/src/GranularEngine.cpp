// SPDX-License-Identifier: AGPL-3.0-or-later
#include <grains/GranularEngine.h>

#include <algorithm>
#include <cmath>

namespace grains {

namespace {
constexpr double kPi = 3.14159265358979323846;

// Clamped to [0, 1]; NaN fails both comparisons and maps to 0.
float clamp01(double v) noexcept
{
    return v > 0.0 ? (v < 1.0 ? static_cast<float>(v) : 1.0f) : 0.0f;
}

// Fractional part in [0, 1); a non-finite x gives 0. The subtraction can round up to exactly 1.
double wrap01(double x) noexcept
{
    if (!std::isfinite(x)) return 0.0;
    const double r = x - std::floor(x);
    return r >= 1.0 ? 0.0 : r;
}

// Wraps around the source ends only when x lies outside [0, 1]; 1.0 itself (the last frame) stays.
double foldOutside01(double x) noexcept
{
    return (x >= 0.0 && x <= 1.0) ? x : wrap01(x);
}

} // namespace

float GranularEngine::softLimit(float y) noexcept
{
    const float mag = std::fabs(y);
    // Written so that NaN also takes the flush branch. Exact zero is returned as is (R-GR-10:
    // x == 0 is transparent bit-exactly, so -0 keeps its sign); everything else flushes to +0.
    if (!(mag >= kFlushThreshold)) return y == 0.0f ? y : 0.0f;
    const float a = std::min(mag, kLimiterPreClamp); // +-inf, FLT_MAX -> pre-clamp
    if (a <= kLimiterKnee) return y;                 // transparent, bit-exact
    // C - s^2 / ((a - T) + s) is a chain of single monotone IEEE operations (no FMA candidate), so
    // the result is monotone in float; max() removes a possible 1-ulp dip just above the knee.
    const float m = std::max(kLimiterKnee, kOutputCeiling - kLimiterSpanSq / ((a - kLimiterKnee) + kLimiterSpan));
    return y < 0.0f ? -m : m;
}

void GranularEngine::prepare(double sampleRate, std::uint64_t seed, InitialTransport initial) noexcept
{
    sampleRate_ = limits::sanitizeSampleRate(sampleRate);
    seed_ = seed;
    rng_.seed(seed);
    gate_.prepare(sampleRate_);
    target_ = initial == InitialTransport::Playing;
    if (target_) gate_.forcePlaying();
    pool_.reset();
    scheduler_.reset();
    resetScan();
    scheduler_.configure(sampleRate_, params_.density, params_.timingJitter);
}

void GranularEngine::stopNow() noexcept
{
    gate_.forceStopped();
    target_ = false;
    pool_.reset();
    scheduler_.reset();
    resetScan();
}

void GranularEngine::setSource(const SampleBuffer* source) noexcept
{
    source_ = source;
    pool_.reset(); // grain positions refer to the previous source
    resetScan();   // a new load restarts the scan from the anchor
}

void GranularEngine::setParams(const EngineParams& params) noexcept
{
    const EngineParams clamped = params.clamped();
    if (clamped.scan != params_.scan) {
        // Rebase on the old scan so the offset is continuous: only the time after the change is affected.
        scanBase_ = wrap01(offsetAt(elapsed_));
        scanFrom_ = elapsed_;
    }
    params_ = clamped;
    scheduler_.configure(sampleRate_, params_.density, params_.timingJitter);
}

void GranularEngine::process(float* left, float* right, int n) noexcept
{
    if (n <= 0) return;
    // The target is read once per call (block-quantised). Only Stopped -> Starting restarts: the
    // state is then identical to prepare(), so every Play reproduces the same output.
    if (gate_.request(target_)) {
        rng_.seed(seed_);
        pool_.reset();
        scheduler_.reset();
        resetScan();
    }
    std::fill(left, left + n, 0.0f);
    std::fill(right, right + n, 0.0f);
    if (source_ == nullptr || source_->numFrames() <= 0 || source_->numChannels() <= 0) return;
    if (gate_.stopped()) return; // no scheduling, no rng draws, no rendering

    int cursor = 0;
    scheduler_.advance(n, [&](int offset) {
        renderSegment(left, right, cursor, offset - cursor);
        cursor = offset;
        spawn(elapsed_ + offset);
    });
    renderSegment(left, right, cursor, n - cursor);
    elapsed_ += n;

    gate_.apply(left, right, n); // before the limiter, so |y| < 1 holds during ramps
    if (gate_.stopped()) {       // the fade-out completed in this call: drop what is left
        pool_.reset();
        scheduler_.reset();
        resetScan();
    }

    for (int i = 0; i < n; ++i) {
        left[i] = softLimit(left[i]);
        right[i] = softLimit(right[i]);
    }
}

// Renders `count` samples of every active grain at output position `offset`, then releases the
// grains that have played their whole length.
void GranularEngine::renderSegment(float* left, float* right, int offset, int count) noexcept
{
    const SampleBuffer& src = *source_;
    const float* srcL = src.channel(0);
    const float* srcR = src.numChannels() > 1 ? src.channel(1) : srcL;
    const int frames = src.numFrames();
    pool_.forEachActive([&](Grain& g) {
        if (count > 0)
            renderGrain<LinearInterpolator>(g, srcL, srcR, frames, window_, left + offset, right + offset, count);
        if (g.age >= g.length) pool_.release(g);
    });
}

void GranularEngine::snapshotGrains(GrainSnapshot& out) const noexcept
{
    out.version = GrainSnapshot::kVersion;
    out.count = 0;
    if (source_ == nullptr) return; // setSource and prepare reset the pool, so it is empty here too

    const int frames = source_->numFrames();
    const double lastFrame = frames > 1 ? static_cast<double>(frames - 1) : 0.0;
    pool_.forEachActiveSlot([&](int slot, const Grain& g) {
        // spawn keeps pos <= frames - 1; the clamp only absorbs floating-point rounding.
        // progress is the expression renderGrain feeds the window, so level is the next sample's window.
        const float progress = g.length > 0 ? clamp01(static_cast<float>(g.age) / static_cast<float>(g.length)) : 0.0f;
        GrainMark& m = out.marks[static_cast<std::size_t>(out.count++)];
        m.position = lastFrame > 0.0 ? clamp01(g.pos / lastFrame) : 0.0f;
        m.progress = progress;
        m.level = clamp01(static_cast<double>(window_.at(progress)));
        m.slot = static_cast<std::uint16_t>(slot);
        m.reserved = 0;
    });
}

void GranularEngine::resetScan() noexcept
{
    elapsed_ = 0;
    scanFrom_ = 0;
    scanBase_ = 0.0;
}

double GranularEngine::offsetAt(std::int64_t at) const noexcept
{
    if (params_.scan == 0.0f) return scanBase_;
    return scanBase_ + static_cast<double>(params_.scan) * static_cast<double>(at - scanFrom_) / sampleRate_;
}

void GranularEngine::spawn(std::int64_t at) noexcept
{
    // Fixed draw order and count, before any pool or bounds check, so skipped spawns consume the
    // same random stream as accepted ones (the schedule never depends on pool state).
    const float posDraw = rng_.bipolar();
    const float pitchDraw = rng_.bipolar();
    const float panDraw = rng_.bipolar();

    const SampleBuffer& src = *source_;
    const int frames = src.numFrames();

    const double offset = offsetAt(at);
    // Fold before the jitter clamp. At offset 0 the anchor is used untouched (scan 0 is bit-identical).
    const float anchor = offset == 0.0 ? params_.position
                                       : static_cast<float>(foldOutside01(static_cast<double>(params_.position) + offset));
    const float posNorm = limits::sanitizeUnit(anchor + params_.positionJitter * posDraw, 0.5f);
    double start = static_cast<double>(posNorm) * static_cast<double>(frames - 1);

    const float semis = limits::detail::clampTo(params_.pitchSemitones + params_.pitchJitterSemitones * pitchDraw,
                                                -limits::kPitchSemitonesMax, limits::kPitchSemitonesMax);
    const double srcSr = src.sampleRate() > 0.0 ? src.sampleRate() : sampleRate_;
    const double rate = std::exp2(static_cast<double>(semis) / 12.0) * srcSr / sampleRate_;

    const double sizeSec = static_cast<double>(params_.grainSizeMs) * 0.001;
    const double requested = std::max(static_cast<double>(kMinGrainLength), std::floor(sizeSec * sampleRate_ + 0.5));

    // Shrink to the source span that is left; the window then completes over the shrunk length.
    const double last = static_cast<double>(frames - 1);
    double len = std::min(requested, std::floor((last - start) / rate));
    if (len < static_cast<double>(kMinGrainLength)) {
        start = last - static_cast<double>(kMinGrainLength) * rate; // move back to fit 2 samples
        if (start < 0.0) return;                                    // source too short: skip
        len = static_cast<double>(kMinGrainLength);
    }

    Grain* g = pool_.acquire();
    if (g == nullptr) return; // pool full: skip (draws were already consumed)

    const double angle = (static_cast<double>(params_.panSpread) * static_cast<double>(panDraw) + 1.0) * (kPi / 4.0);
    const double norm = 1.0 / std::sqrt(std::max(1.0, static_cast<double>(params_.density) * sizeSec));
    const double gain = static_cast<double>(params_.gain) * norm;

    g->pos = start;
    g->rate = rate;
    g->length = static_cast<int>(len);
    g->age = 0;
    g->gainL = static_cast<float>(gain * std::cos(angle));
    g->gainR = static_cast<float>(gain * std::sin(angle));
}

} // namespace grains
