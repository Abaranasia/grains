#pragma once

#include <grains/GranularEngine.h>
#include <grains/ParamSpec.h>

#include <juce_audio_processors/juce_audio_processors.h>

#include <array>
#include <atomic>

// Silent shell: parameters, state and a generic editor around GranularEngine. No sample source is
// set, so the engine renders exact zeros. MIDI is accepted and ignored.
class GrainsAudioProcessor final : public juce::AudioProcessor {
public:
    GrainsAudioProcessor();
    ~GrainsAudioProcessor() override = default;

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

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    static constexpr std::uint64_t kSeed = 1;

    juce::AudioProcessorValueTreeState apvts_;
    std::array<std::atomic<float>*, grains::params::kCount> raw_{};
    grains::GranularEngine engine_;
    juce::HeapBlock<float> scratch_; // right channel when the host output is mono
    int scratchSize_ = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GrainsAudioProcessor)
};
