// SPDX-License-Identifier: AGPL-3.0-or-later
#include "WaveformView.h"

#include <grains/WaveformGeometry.h>

WaveformView::WaveformView(juce::RangedAudioParameter& position, juce::RangedAudioParameter& positionJitter)
    : positionAttachment_(position,
                          [this](float v) {
                              position_ = v;
                              repaint();
                          }),
      jitterAttachment_(positionJitter, [this](float v) {
          jitter_ = v;
          repaint();
      })
{
    positionAttachment_.sendInitialUpdate();
    jitterAttachment_.sendInitialUpdate();
}

WaveformView::~WaveformView()
{
    if (gesturing_) positionAttachment_.endGesture();
}

void WaveformView::setWaveform(const grains::WaveformState& state)
{
    peaks_ = state.peaks;
    display_ = state.display();
    repaint();
}

void WaveformView::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds();
    g.fillAll(juce::Colour(0xff1b1b1f));

    const int width = bounds.getWidth();
    const float w = static_cast<float>(width);
    const float h = static_cast<float>(bounds.getHeight());
    const float mid = h * 0.5f;

    if (display_ == grains::WaveformDisplay::Waveform && peaks_ != nullptr) {
        g.setColour(juce::Colour(0xff5aa9e6));
        for (int x = 0; x < width; ++x) {
            const auto p = grains::columnPeak(*peaks_, x, width);
            const float top = mid - juce::jlimit(-1.0f, 1.0f, p.max) * mid;
            const float bottom = mid - juce::jlimit(-1.0f, 1.0f, p.min) * mid;
            // At least one pixel tall so silent columns still draw the centre line.
            g.fillRect(static_cast<float>(x), top, 1.0f, juce::jmax(1.0f, bottom - top));
        }
    } else {
        g.setColour(juce::Colours::grey);
        g.setFont(14.0f);
        const char* hint = display_ == grains::WaveformDisplay::NoWaveform ? "No waveform" : "Drop a sample";
        g.drawText(hint, bounds, juce::Justification::centred);
    }

    const auto band = grains::jitterBand(position_, jitter_);
    const float x0 = grains::positionToX(band.lo, w);
    const float x1 = grains::positionToX(band.hi, w);
    g.setColour(juce::Colour(0x40ffb347));
    g.fillRect(x0, 0.0f, x1 - x0, h);

    const float mx = grains::positionToX(position_, w);
    g.setColour(juce::Colour(0xffffb347));
    g.fillRect(mx - 1.0f, 0.0f, 2.0f, h);
}

void WaveformView::setPositionFromMouse(const juce::MouseEvent& e)
{
    const float p = grains::xToPosition(static_cast<float>(e.x), static_cast<float>(getWidth()));
    positionAttachment_.setValueAsPartOfGesture(p); // skew is 1, so plain == normalised
}

void WaveformView::mouseDown(const juce::MouseEvent& e)
{
    if (gesturing_) return;
    gesturing_ = true;
    positionAttachment_.beginGesture();
    setPositionFromMouse(e);
}

void WaveformView::mouseDrag(const juce::MouseEvent& e)
{
    if (gesturing_) setPositionFromMouse(e);
}

void WaveformView::mouseUp(const juce::MouseEvent&)
{
    if (!gesturing_) return;
    gesturing_ = false;
    positionAttachment_.endGesture();
}
