// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <grains/WaveformPeaks.h>

#include <juce_audio_processors/juce_audio_processors.h>

#include <memory>

// Full-sample waveform with the `position` marker and a read-only `positionJitter` band. Press sets
// `position` to the clicked x and drag keeps updating it, inside one automation gesture. Repaints only
// on change (peaks, parameter callbacks, resize); nothing polls. Message thread only.
class WaveformView final : public juce::Component {
public:
    // Both parameters must outlive the view. Only `position` is ever written.
    WaveformView(juce::RangedAudioParameter& position, juce::RangedAudioParameter& positionJitter);

    // Adopts what to show (peaks and empty / no-waveform / waveform) and repaints.
    void setWaveform(const grains::WaveformState& state);

    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;

private:
    void setPositionFromMouse(const juce::MouseEvent& e);

    std::shared_ptr<const grains::WaveformPeaks> peaks_;
    grains::WaveformDisplay display_ = grains::WaveformDisplay::Empty;
    float position_ = 0.0f; // plain == normalised: both parameters have range [0, 1] and skew 1
    float jitter_ = 0.0f;
    bool dragging_ = false;
    juce::ParameterAttachment positionAttachment_;
    juce::ParameterAttachment jitterAttachment_; // callback only: never writes

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WaveformView)
};
