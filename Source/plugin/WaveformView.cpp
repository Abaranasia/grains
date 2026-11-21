// SPDX-License-Identifier: AGPL-3.0-or-later
#include "WaveformView.h"

#include <grains/WaveformGeometry.h>

#include <algorithm>

namespace {
const juce::Colour kBackground{0xff1b1e23};
const juce::Colour kWave{0xff6fb3d9};
const juce::Colour kBand{0x40ffc857};
const juce::Colour kMarker{0xffffc857};
const juce::Colour kHint{0xff8a9099};

float clampPeak(float v) { return std::clamp(v, -1.0f, 1.0f); }
} // namespace

WaveformView::WaveformView(juce::RangedAudioParameter& position, juce::RangedAudioParameter& positionJitter)
    : positionAttachment_(position, [this](float v) { position_ = v; repaint(); }),
      jitterAttachment_(positionJitter, [this](float v) { jitter_ = v; repaint(); })
{
    positionAttachment_.sendInitialUpdate();
    jitterAttachment_.sendInitialUpdate();
}

void WaveformView::setWaveform(const grains::WaveformState& state)
{
    peaks_ = state.peaks;
    display_ = state.display();
    repaint();
}

void WaveformView::paint(juce::Graphics& g)
{
    g.fillAll(kBackground);
    const auto w = static_cast<float>(getWidth());
    const auto h = static_cast<float>(getHeight());

    if (display_ == grains::WaveformDisplay::Waveform && peaks_ != nullptr) {
        const float mid = h * 0.5f;
        g.setColour(kWave);
        for (int x = 0; x < getWidth(); ++x) {
            const auto pk = grains::columnPeak(*peaks_, x, getWidth());
            const float top = mid - clampPeak(pk.max) * mid;
            const float bottom = mid - clampPeak(pk.min) * mid;
            g.drawVerticalLine(x, top, std::max(bottom, top + 1.0f)); // at least one pixel so silence shows a line
        }
    } else {
        g.setColour(kHint);
        g.setFont(14.0f);
        g.drawText(display_ == grains::WaveformDisplay::NoWaveform ? "No waveform" : "Drop a sample",
                   getLocalBounds(), juce::Justification::centred);
    }

    const auto band = grains::jitterBand(position_, jitter_);
    const float lo = grains::positionToX(band.lo, w);
    const float hi = grains::positionToX(band.hi, w);
    g.setColour(kBand);
    g.fillRect(lo, 0.0f, hi - lo, h);

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
