// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "PluginProcessor.h"
#include "WaveformView.h"

#include <juce_audio_utils/juce_audio_utils.h>

// Waveform view above the generic parameter editor, plus a Load button, drag-and-drop and a sample
// status label. Fixed size.
class GrainsEditor final : public juce::AudioProcessorEditor,
                           public juce::FileDragAndDropTarget,
                           private juce::ChangeListener {
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
    }

    GrainsAudioProcessor& proc_;
    WaveformView view_;
    juce::GenericAudioProcessorEditor generic_;
    juce::TextButton load_{"Load..."};
    juce::Label status_;
    std::unique_ptr<juce::FileChooser> chooser_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GrainsEditor)
};
