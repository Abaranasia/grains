// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <grains/GrainDisplayModel.h>
#include <grains/WaveformPeaks.h>

#include <juce_audio_processors/juce_audio_processors.h>

#include <functional>
#include <memory>

// Full-sample waveform with the `position` marker and a read-only `positionJitter` band. Press sets
// `position` to the clicked x and drag keeps updating it, inside one automation gesture. Below the
// waveform a strip of lane dots shows the active grains (R-WD-7). The only polling is a 30 Hz timer that
// runs while the view is showing a waveform and has a poll function; everything static repaints only on
// change (peaks, parameter callbacks, resize). Message thread only.
class WaveformView final : public juce::Component, private juce::Timer {
public:
    static constexpr int kStripHeight = 48; // lane-dot strip under the waveform
    static constexpr int kWaveformHeight = 140;
    static constexpr int kViewHeight = kWaveformHeight + kStripHeight;

    // Both parameters must outlive the view. Only `position` is ever written.
    WaveformView(juce::RangedAudioParameter& position, juce::RangedAudioParameter& positionJitter);
    ~WaveformView() override;

    // Adopts what to show (peaks and empty / no-waveform / waveform) and repaints.
    void setWaveform(const grains::WaveformState& state);

    // Source of fresh grain snapshots (a nullptr result means nothing new). Called from the timer only.
    void setPollGrains(std::function<const grains::GrainSnapshot*()> poll);

    void paint(juce::Graphics& g) override;
    void resized() override;
    void visibilityChanged() override;
    void parentHierarchyChanged() override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;

private:
    void timerCallback() override;
    void updateTimer();
    void rebuildImage(float scale);
    juce::Rectangle<int> stripBounds() const { return getLocalBounds().removeFromBottom(kStripHeight); }
    bool showsWaveform() const { return display_ == grains::WaveformDisplay::Waveform && peaks_ != nullptr; }
    void setPositionFromMouse(const juce::MouseEvent& e);

    std::shared_ptr<const grains::WaveformPeaks> peaks_;
    grains::WaveformDisplay display_ = grains::WaveformDisplay::Empty;
    float position_ = 0.0f; // plain == normalised: both parameters have range [0, 1] and skew 1
    float jitter_ = 0.0f;
    bool dragging_ = false;
    std::function<const grains::GrainSnapshot*()> pollGrains_;
    grains::GrainDisplayModel model_;
    juce::Image image_; // background and waveform (or hint), rebuilt lazily when stale
    bool imageDirty_ = true;
    juce::ParameterAttachment positionAttachment_;
    juce::ParameterAttachment jitterAttachment_; // callback only: never writes

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WaveformView)
};
