// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <grains/WaveformPeaks.h>

#include <juce_audio_utils/juce_audio_utils.h>

#include <memory>

// R-WD-4: paints the sample overview, the `position` marker and the read-only `positionJitter` band, and
// maps the mouse to `position` through a ParameterAttachment. Repaints only on change (peaks, parameter
// callback, resize); it owns no Timer.
class WaveformView final : public juce::Component {
public:
    WaveformView(juce::RangedAudioParameter& position, juce::RangedAudioParameter& positionJitter);
    ~WaveformView() override;

    // Adopts what the processor currently shows. Message thread only.
    void setWaveform(const grains::WaveformState& state);

    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;

private:
    void setPositionFromMouse(const juce::MouseEvent& e);

    std::shared_ptr<const grains::WaveformPeaks> peaks_;
    grains::WaveformDisplay display_ = grains::WaveformDisplay::Empty;
    float position_ = 0.0f;
    float jitter_ = 0.0f;
    bool gesturing_ = false;

    // Declared last so they are destroyed first: their callbacks touch the members above.
    juce::ParameterAttachment positionAttachment_;
    juce::ParameterAttachment jitterAttachment_; // callback only, never writes the parameter

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WaveformView)
};
