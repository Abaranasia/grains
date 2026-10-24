#include "PluginEditor.h"

#include <algorithm>

namespace {
constexpr int kBarHeight = 40;
constexpr const char* kPatterns = "*.wav;*.aiff;*.aif";
} // namespace

GrainsEditor::GrainsEditor(GrainsAudioProcessor& p)
    : juce::AudioProcessorEditor(p), proc_(p), generic_(p)
{
    addAndMakeVisible(generic_);
    addAndMakeVisible(load_);
    addAndMakeVisible(status_);
    status_.setText(proc_.status(), juce::dontSendNotification);

    load_.onClick = [this] {
        const juce::Component::SafePointer<GrainsEditor> safe(this);
        chooser_ = std::make_unique<juce::FileChooser>("Load sample", juce::File{}, kPatterns);
        chooser_->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [safe](const juce::FileChooser& fc) {
                                  const auto f = fc.getResult();
                                  if (safe != nullptr && f != juce::File{}) safe->proc_.loadFile(f);
                              });
    };

    proc_.statusBroadcaster().addChangeListener(this);
    setSize(generic_.getWidth(), generic_.getHeight() + kBarHeight);
}

GrainsEditor::~GrainsEditor()
{
    proc_.statusBroadcaster().removeChangeListener(this);
}

void GrainsEditor::resized()
{
    auto area = getLocalBounds();
    auto bar = area.removeFromTop(kBarHeight).reduced(4);
    load_.setBounds(bar.removeFromLeft(100));
    status_.setBounds(bar.reduced(8, 0));
    generic_.setBounds(area);
}

bool GrainsEditor::isSampleFile(const juce::String& path)
{
    return juce::File(path).hasFileExtension("wav;aiff;aif");
}

bool GrainsEditor::isInterestedInFileDrag(const juce::StringArray& files)
{
    return std::any_of(files.begin(), files.end(), [](const auto& f) { return isSampleFile(f); });
}

void GrainsEditor::filesDropped(const juce::StringArray& files, int, int)
{
    for (const auto& f : files)
        if (isSampleFile(f)) {
            proc_.loadFile(juce::File(f)); // the first matching file wins, others are ignored
            return;
        }
}
