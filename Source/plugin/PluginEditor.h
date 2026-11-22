// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "PluginProcessor.h"
#include "WaveformView.h"

#include <juce_audio_utils/juce_audio_utils.h>

// Waveform view above the generic parameter editor, plus a Load button, drag-and-drop and a sample status label.
class GrainsEditor final : public juce::AudioProcessorEditor,
                           public juce::FileDragAndDropTarget,
                           private juce::ChangeListener,
                           private juce::Timer {
public:
    explicit GrainsEditor(GrainsAudioProcessor& p);
    ~GrainsEditor() override;

    void resized() override;
    bool isInterestedInFileDrag(const juce::StringArray& files) override;
    void filesDropped(const juce::StringArray& files, int x, int y) override;

private:
    static bool isSampleFile(const juce::String& path);
    void changeListenerCallback(juce::ChangeBroadcaster*) override
    {
        status_.setText(proc_.status(), juce::dontSendNotification);
        view_.setWaveform(proc_.waveform());
        syncTransport();
    }
    void timerCallback() override { syncTransport(); }
    // Mirrors the processor into the buttons. Writes only on change and never notifies, so a sync cannot
    // raise a Play/Stop request.
    void syncTransport();

    GrainsAudioProcessor& proc_;
    WaveformView view_;
    juce::GenericAudioProcessorEditor generic_;
    juce::TextButton load_{"Load..."};
    juce::TextButton play_{"Play"};
    juce::TextButton stop_{"Stop"};
    juce::Label status_;
    std::unique_ptr<juce::FileChooser> chooser_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GrainsEditor)
};
