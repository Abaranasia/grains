#include <grains/GranularEngine.h>

#include <algorithm>
#include <cmath>

namespace grains {

namespace {
constexpr double kPi = 3.14159265358979323846;

float finalize(float y) noexcept
{
    // Written so that NaN also takes the flush branch.
    if (!(std::fabs(y) >= GranularEngine::kFlushThreshold)) return 0.0f;
    return y > GranularEngine::kOutputCeiling ? GranularEngine::kOutputCeiling
                                              : (y < -GranularEngine::kOutputCeiling ? -GranularEngine::kOutputCeiling : y);
}
} // namespace

void GranularEngine::prepare(double sampleRate, std::uint64_t seed) noexcept
{
    sampleRate_ = limits::sanitizeSampleRate(sampleRate);
    rng_.seed(seed);
    pool_.reset();
    scheduler_.reset();
    scheduler_.configure(sampleRate_, params_.density, params_.timingJitter);
}

void GranularEngine::setSource(const SampleBuffer* source) noexcept
{
    source_ = source;
    pool_.reset(); // grain positions refer to the previous source
}

void GranularEngine::setParams(const EngineParams& params) noexcept
{
    params_ = params.clamped();
    scheduler_.configure(sampleRate_, params_.density, params_.timingJitter);
}

void GranularEngine::process(float* left, float* right, int n) noexcept
{
    if (n <= 0) return;
    std::fill(left, left + n, 0.0f);
    std::fill(right, right + n, 0.0f);
    if (source_ == nullptr || source_->numFrames() <= 0 || source_->numChannels() <= 0) return;

    int cursor = 0;
    scheduler_.advance(n, [&](int offset) {
        renderSegment(left, right, cursor, offset - cursor);
        cursor = offset;
        spawn(offset);
    });
    renderSegment(left, right, cursor, n - cursor);

    for (int i = 0; i < n; ++i) {
        left[i] = finalize(left[i]);
        right[i] = finalize(right[i]);
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

void GranularEngine::spawn(int /*offset*/) noexcept
{
    // Fixed draw order and count, before any pool or bounds check, so skipped spawns consume the
    // same random stream as accepted ones (the schedule never depends on pool state).
    const float posDraw = rng_.bipolar();
    const float pitchDraw = rng_.bipolar();
    const float panDraw = rng_.bipolar();

    const SampleBuffer& src = *source_;
    const int frames = src.numFrames();

    const float posNorm = limits::sanitizeUnit(params_.position + params_.positionJitter * posDraw, 0.5f);
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
