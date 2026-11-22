// SPDX-License-Identifier: AGPL-3.0-or-later
#include "PluginProcessor.h"

#include "PluginEditor.h"

#include <grains/SamplePathResolver.h>

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

namespace {
namespace pm = grains::params;
const juce::Identifier kSamplePath{"samplePath"};
const juce::Identifier kSampleName{"sampleName"};
const juce::Identifier kSampleAnchor{"sampleAnchor"};
const juce::Identifier kSampleRel{"sampleRel"};

// Well-known user folders, in priority order. Keys are stable across machines; empty ones are skipped.
std::vector<grains::PathAnchor> localAnchors()
{
    using SL = juce::File::SpecialLocationType;
    const std::pair<const char*, SL> locations[] = {{"music", SL::userMusicDirectory},
                                                    {"documents", SL::userDocumentsDirectory},
                                                    {"desktop", SL::userDesktopDirectory},
                                                    {"home", SL::userHomeDirectory}};
    std::vector<grains::PathAnchor> out;
    for (const auto& [key, location] : locations) {
        const auto dir = juce::File::getSpecialLocation(location).getFullPathName();
        if (dir.isNotEmpty()) out.push_back({key, dir.toStdString()});
    }
    return out;
}

juce::String prop(const juce::ValueTree& tree, const juce::Identifier& id)
{
    return tree.getProperty(id).toString();
}

// APVTS::replaceState gives a parameter that has no child in the incoming tree the current live value,
// not its default. State saved before a parameter existed (for example `scan`) would then keep whatever
// the running instance held. Add an explicit default entry for every table id the state lacks.
void addMissingParamDefaults(juce::ValueTree& state)
{
    const juce::Identifier paramType{"PARAM"};
    const juce::Identifier idProp{"id"};
    const juce::Identifier valueProp{"value"};
    for (const auto& s : pm::kSpecs) {
        const juce::String id{s.id};
        if (state.getChildWithProperty(idProp, id).isValid()) continue;
        juce::ValueTree child{paramType};
        child.setProperty(idProp, id, nullptr);
        child.setProperty(valueProp, s.def, nullptr);
        state.appendChild(child, nullptr);
    }
}
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
            juce::ParameterID{s.id, s.versionHint}, s.name,
            juce::NormalisableRange<float>(s.min, s.max, 0.0f, s.skew, s.symmetric), s.def,
            juce::AudioParameterFloatAttributes().withLabel(s.unit)));
    }
    return layout;
}

void GrainsAudioProcessor::prepareToPlay(double sampleRate, int maxBlockSize)
{
    engine_.prepare(sampleRate, kSeed); // Stopped: a host prepare returns the plugin to Stopped
    playing_.store(false, std::memory_order_relaxed);
    loader_.retarget(sampleRate); // reissues or reloads off this thread; a no-op when the rate is unchanged
    scratchSize_ = std::max(1, maxBlockSize);
    scratch_.allocate(static_cast<size_t>(scratchSize_), true);
    publishGrains(); // the pool is empty after prepare: clears any markers the editor still shows
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
    if (const auto r = exchange_.acquire(); r.changed) {
        engine_.setSource(r.source);
        engine_.stopNow(); // a new sample hard-stops with no fade and stays silent until Play
        playing_.store(false, std::memory_order_relaxed);
    }
    engine_.setPlaying(playing_.load(std::memory_order_relaxed));
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
    } else if (scratchSize_ <= 0 || scratch_ == nullptr) {
        // Mono host, unprepared (no scratch yet): output silence instead of looping forever.
        buffer.clear();
    } else {
        // Mono host: render into the preallocated scratch right channel and discard it.
        for (int done = 0; done < numSamples;) {
            const int n = std::min(scratchSize_, numSamples - done);
            engine_.process(left + done, scratch_.get(), n);
            done += n;
        }
    }
    publishGrains(); // one snapshot per block, after the render
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
            auto incoming = juce::ValueTree::fromXml(*xml);
            addMissingParamDefaults(incoming);
            apvts_.replaceState(incoming);
            playing_.store(false, std::memory_order_relaxed); // restore always starts Stopped
            const auto ref = grains::SamplePathResolver::restoreRef(
                prop(apvts_.state, kSamplePath).toStdString(), prop(apvts_.state, kSampleName).toStdString(),
                prop(apvts_.state, kSampleAnchor).toStdString(), prop(apvts_.state, kSampleRel).toStdString());
            const auto anchors = localAnchors();
            std::vector<std::string> searchDirs; // name search: directly inside each anchor, not recursive
            for (const auto& a : anchors) searchDirs.push_back(a.dir);
            juce::StringArray list;
            for (const auto& c : grains::SamplePathResolver::candidates(ref, anchors, searchDirs))
                list.add(juce::String::fromUTF8(c.c_str()));
            loader_.request(list, juce::String::fromUTF8(ref.absolute.c_str()), juce::String::fromUTF8(ref.name.c_str()),
                            SampleLoader::Origin::Restore);
            restoreGeneration_.store(loader_.latestGeneration());
        }
}

void GrainsAudioProcessor::loadFile(const juce::File& file)
{
    const auto path = file.getFullPathName();
    loader_.request(juce::StringArray{path}, path, file.getFileName(), SampleLoader::Origin::User);
}

void GrainsAudioProcessor::onLoadResult(const SampleLoader::Result& r)
{
    // Results arrive in publish order, and a User result may be superseded before it is processed (for
    // example by a rate-change reload of the same file). The file is live either way, so persist it
    // before the generation filter. A result older than the last state restore is skipped: the restore
    // replaced the state it would write into.
    if (r.loaded && r.origin == SampleLoader::Origin::User && r.generation > restoreGeneration_.load()) {
        const auto ref = grains::SamplePathResolver::makeRef(r.path.toStdString(), localAnchors());
        apvts_.state.setProperty(kSamplePath, r.path, nullptr);
        apvts_.state.setProperty(kSampleName, juce::String::fromUTF8(ref.name.c_str()), nullptr);
        if (ref.anchorKey.empty()) {
            apvts_.state.removeProperty(kSampleAnchor, nullptr);
            apvts_.state.removeProperty(kSampleRel, nullptr);
        } else {
            apvts_.state.setProperty(kSampleAnchor, juce::String::fromUTF8(ref.anchorKey.c_str()), nullptr);
            apvts_.state.setProperty(kSampleRel, juce::String::fromUTF8(ref.relative.c_str()), nullptr);
        }
    }
    // Results are posted in commit order, so applying every one here (before the label filter below)
    // keeps the waveform in step with the source that is actually audible.
    if (r.loaded || r.cleared) playing_.store(false, std::memory_order_relaxed); // a failed load keeps the state
    const bool waveformChanged = waveform_.apply(r.loaded, r.cleared, r.peaks);
    if (r.generation < loader_.latestGeneration()) { // superseded: only the label is filtered
        if (waveformChanged) broadcaster_.sendChangeMessage();
        return;
    }
    status_ = r.text;
    broadcaster_.sendChangeMessage();
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new GrainsAudioProcessor();
}
