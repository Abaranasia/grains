// SPDX-License-Identifier: AGPL-3.0-or-later
#include "PluginProcessor.h"

#include "PluginEditor.h"

#include <algorithm>

namespace {
namespace pm = grains::params;
const juce::Identifier kSamplePath{"samplePath"};
} // namespace

GrainsAudioProcessor::GrainsAudioProcessor()
    : juce::AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts_(*this, nullptr, "GrainsState", createLayout()),
      loader_(exchange_, [this](const SampleLoader::Result& r) { onLoadResult(r); })
{
    for (int i = 0; i < pm::kCount; ++i)
        raw_[static_cast<std::size_t>(i)] = apvts_.getRawParameterValue(pm::kSpecs[static_cast<std::size_t>(i)].id);
}

juce::AudioProcessorValueTreeState::ParameterLayout GrainsAudioProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    for (const auto& s : pm::kSpecs) {
        layout.add(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{s.id, 1}, s.name,
            juce::NormalisableRange<float>(s.min, s.max, 0.0f, s.skew), s.def,
            juce::AudioParameterFloatAttributes().withLabel(s.unit)));
    }
    return layout;
}

void GrainsAudioProcessor::prepareToPlay(double sampleRate, int maxBlockSize)
{
    engine_.prepare(sampleRate, kSeed);
    scratchSize_ = std::max(1, maxBlockSize);
    scratch_.allocate(static_cast<size_t>(scratchSize_), true);
}

bool GrainsAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return layouts.getMainInputChannelSet().isDisabled()
           && (out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo());
}

void GrainsAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    // Adopt a newly published source first so a swap is never stuck behind an early return.
    if (const auto r = exchange_.acquire(); r.changed) engine_.setSource(r.source);
    midi.clear(); // MIDI is accepted and ignored in this version

    std::array<float, pm::kCount> plain{};
    for (std::size_t i = 0; i < plain.size(); ++i)
        plain[i] = raw_[i]->load(std::memory_order_relaxed);
    engine_.setParams(pm::toEngineParams(plain));

    const int numSamples = buffer.getNumSamples();
    if (buffer.getNumChannels() <= 0) return;

    float* left = buffer.getWritePointer(0);
    const bool stereo = buffer.getNumChannels() > 1;
    float* right = stereo ? buffer.getWritePointer(1) : nullptr;

    if (stereo) {
        engine_.process(left, right, numSamples);
        return;
    }
    // Mono host: render into the preallocated scratch right channel and discard it.
    // Unprepared (no scratch yet): output silence instead of looping forever.
    if (scratchSize_ <= 0 || scratch_ == nullptr) {
        buffer.clear();
        return;
    }
    for (int done = 0; done < numSamples;) {
        const int n = std::min(scratchSize_, numSamples - done);
        engine_.process(left + done, scratch_.get(), n);
        done += n;
    }
}

juce::AudioProcessorEditor* GrainsAudioProcessor::createEditor()
{
    return new GrainsEditor(*this);
}

void GrainsAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    if (const auto xml = apvts_.copyState().createXml())
        copyXmlToBinary(*xml, destData);
}

void GrainsAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (data == nullptr || sizeInBytes <= 0) return;
    if (const auto xml = getXmlFromBinary(data, sizeInBytes))
        if (xml->hasTagName(apvts_.state.getType()))
        {
            apvts_.replaceState(juce::ValueTree::fromXml(*xml));
            loader_.request(apvts_.state.getProperty(kSamplePath).toString(), SampleLoader::Origin::Restore);
        }
}

void GrainsAudioProcessor::onLoadResult(const SampleLoader::Result& r)
{
    if (r.generation < loader_.latestGeneration()) return; // superseded by a newer request
    status_ = r.text;
    if (r.loaded && r.origin == SampleLoader::Origin::User)
        apvts_.state.setProperty(kSamplePath, r.path, nullptr);
    broadcaster_.sendChangeMessage();
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new GrainsAudioProcessor();
}
