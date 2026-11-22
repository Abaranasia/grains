// SPDX-License-Identifier: AGPL-3.0-or-later
#include "WaveformView.h"

#include <grains/WaveformGeometry.h>

#include <algorithm>
#include <utility>

namespace {
const juce::Colour kBackground{0xff1b1e23};
const juce::Colour kWave{0xff6fb3d9};
const juce::Colour kBand{0x40ffc857};
const juce::Colour kMarker{0xffffc857};
const juce::Colour kHint{0xff8a9099};
const juce::Colour kDot{0xff7ee0a1};
constexpr int kLaneRows = 8;
constexpr float kRowPitch = 6.0f; // kLaneRows * kRowPitch == WaveformView::kStripHeight
constexpr float kDotSize = 4.0f;
static_assert(kLaneRows * static_cast<int>(kRowPitch) == WaveformView::kStripHeight);

float clampPeak(float v) { return std::clamp(v, -1.0f, 1.0f); }
} // namespace

WaveformView::WaveformView(juce::RangedAudioParameter& position, juce::RangedAudioParameter& positionJitter)
    : positionAttachment_(position, [this](float v) { position_ = v; repaint(); }),
      jitterAttachment_(positionJitter, [this](float v) { jitter_ = v; repaint(); })
{
    positionAttachment_.sendInitialUpdate();
    jitterAttachment_.sendInitialUpdate();
}

WaveformView::~WaveformView()
{
    stopTimer();
}

void WaveformView::setWaveform(const grains::WaveformState& state)
{
    peaks_ = state.peaks;
    display_ = state.display();
    imageDirty_ = true;
    model_.reset(); // marks of the previous source must not outlive it
    updateTimer();
    repaint();
}

void WaveformView::setPollGrains(std::function<const grains::GrainSnapshot*()> poll)
{
    pollGrains_ = std::move(poll);
    updateTimer();
}

void WaveformView::resized()
{
    imageDirty_ = true;
}

void WaveformView::visibilityChanged()
{
    updateTimer();
}

void WaveformView::parentHierarchyChanged()
{
    updateTimer();
}

// The timer runs only while the view is visible, shows a waveform and can poll. Stopping drops the marks.
void WaveformView::updateTimer()
{
    const bool wanted = isShowing() && showsWaveform() && static_cast<bool>(pollGrains_);
    if (wanted) {
        if (!isTimerRunning()) startTimerHz(grains::GrainDisplayModel::kPollHz);
    } else if (isTimerRunning()) {
        stopTimer();
        const bool hadMarks = model_.shown().count > 0;
        model_.reset();
        if (hadMarks) repaint(stripBounds());
    }
}

void WaveformView::timerCallback()
{
    if (model_.tick(pollGrains_ ? pollGrains_() : nullptr)) repaint(stripBounds());
}

void WaveformView::rebuildImage(float scale)
{
    const int w = getWidth();
    const int h = getHeight();
    const int waveH = h - kStripHeight;
    image_ = juce::Image(juce::Image::ARGB, std::max(1, juce::roundToInt(static_cast<float>(w) * scale)),
                         std::max(1, juce::roundToInt(static_cast<float>(h) * scale)), true);
    juce::Graphics g(image_);
    g.addTransform(juce::AffineTransform::scale(scale));
    g.fillAll(kBackground);

    if (showsWaveform()) {
        const float mid = static_cast<float>(waveH) * 0.5f;
        g.setColour(kWave);
        for (int x = 0; x < w; ++x) {
            const auto pk = grains::columnPeak(*peaks_, x, w);
            const float top = mid - clampPeak(pk.max) * mid;
            const float bottom = mid - clampPeak(pk.min) * mid;
            g.drawVerticalLine(x, top, std::max(bottom, top + 1.0f)); // at least one pixel so silence shows a line
        }
    } else {
        g.setColour(kHint);
        g.setFont(14.0f);
        g.drawText(display_ == grains::WaveformDisplay::NoWaveform ? "No waveform" : "Drop a sample",
                   juce::Rectangle<int>(0, 0, w, waveH), juce::Justification::centred);
    }
    imageDirty_ = false;
}

void WaveformView::paint(juce::Graphics& g)
{
    const auto w = static_cast<float>(getWidth());
    const auto h = static_cast<float>(getHeight());

    // Rebuilt after setWaveform / resized, or when the physical pixel scale changed (display move).
    const float scale = static_cast<float>(g.getInternalContext().getPhysicalPixelScaleFactor());
    if (imageDirty_ || !image_.isValid() || image_.getWidth() != std::max(1, juce::roundToInt(w * scale)))
        rebuildImage(scale);
    g.drawImage(image_, getLocalBounds().toFloat());

    const auto band = grains::jitterBand(position_, jitter_);
    const float lo = grains::positionToX(band.lo, w);
    const float hi = grains::positionToX(band.hi, w);
    g.setColour(kBand);
    g.fillRect(lo, 0.0f, hi - lo, h);

    if (showsWaveform()) { // no dots in the Empty and NoWaveform states
        const auto& shown = model_.shown();
        const float stripTop = h - static_cast<float>(kStripHeight);
        for (int i = 0; i < shown.count; ++i) {
            const auto& m = shown.marks[static_cast<std::size_t>(i)];
            const float cx = grains::positionToX(m.position, w);
            const float cy = stripTop + static_cast<float>(m.slot % kLaneRows) * kRowPitch + kRowPitch * 0.5f;
            g.setColour(kDot.withAlpha(0.25f + 0.75f * m.level));
            g.fillEllipse(cx - kDotSize * 0.5f, cy - kDotSize * 0.5f, kDotSize, kDotSize);
        }
    }

    const float x = grains::positionToX(position_, w);
    g.setColour(kMarker);
    g.drawLine(x, 0.0f, x, h, 2.0f);
}

void WaveformView::setPositionFromMouse(const juce::MouseEvent& e)
{
    positionAttachment_.setValueAsPartOfGesture(
        grains::xToPosition(static_cast<float>(e.x), static_cast<float>(getWidth())));
}

void WaveformView::mouseDown(const juce::MouseEvent& e)
{
    dragging_ = true;
    positionAttachment_.beginGesture();
    setPositionFromMouse(e); // absolute jump, then the drag continues from here
}

void WaveformView::mouseDrag(const juce::MouseEvent& e)
{
    if (dragging_) setPositionFromMouse(e);
}

void WaveformView::mouseUp(const juce::MouseEvent&)
{
    if (!dragging_) return;
    dragging_ = false;
    positionAttachment_.endGesture();
}
