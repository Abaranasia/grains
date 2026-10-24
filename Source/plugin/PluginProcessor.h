#pragma once

#include <grains/GranularEngine.h>
#include <grains/ParamSpec.h>
#include <grains/SampleBuffer.h>
#include <grains/SourceExchange.h>

#include "SampleLoader.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <array>
#include <atomic>

// Parameters, state and an editor around GranularEngine. The engine renders exact zeros until a
// sample is loaded (SampleLoader decodes in the background and publishes through exchange_).
// MIDI is accepted and ignored.
class GrainsAudioProcessor final : public juce::AudioProcessor {
public:
    GrainsAudioProcessor();
    ~GrainsAudioProcessor() override { loader_.stop(); }

    void prepareToPlay(double sampleRate, int maxBlockSize) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override;
    using juce::AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    // Message thread.
    void loadFile(const juce::File& file) { loader_.request(file.getFullPathName(), SampleLoader::Origin::User); }
    const juce::String& status() const noexcept { return status_; }
    juce::ChangeBroadcaster& statusBroadcaster() noexcept { return broadcaster_; }

private:
    void onLoadResult(const SampleLoader::Result& r);

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    static constexpr std::uint64_t kSeed = 1;

    juce::AudioProcessorValueTreeState apvts_;
    std::array<std::atomic<float>*, grains::params::kCount> raw_{};
    // Declaration order matters: loader_ and engine_ are destroyed before exchange_.
    grains::SourceExchange<grains::SampleBuffer> exchange_;
    grains::GranularEngine engine_;
    juce::HeapBlock<float> scratch_; // right channel when the host output is mono
    int scratchSize_ = 0;
    juce::String status_{"No sample"};
    juce::ChangeBroadcaster broadcaster_;
    SampleLoader loader_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GrainsAudioProcessor)
};
