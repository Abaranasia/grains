## Exploration: sample-loader

### Current State
- Engine: SampleBuffer(vector L, vector R, sampleRate) immutable, owns data, mono/stereo, no max length or builder. GranularEngine::setSource(const SampleBuffer*) is NON-owning, clears pool, not thread-safe (call on audio thread). process() zeros when no source. Engine already scales by srcSr/hostSr per grain -> NO loader resampling needed. prepare() keeps source_. process/setSource/setParams noexcept, allocation-free, tested in tests/test_engine_realtime.cpp (AllocationCounter, separate exe grains_realtime_tests). Output clamp +-4 (kOutputCeiling), soft limiter deferred to plugin stage. Engine plays continuously (no MIDI gating).
- Plugin: Source/plugin/PluginProcessor.{h,cpp}; APVTS "GrainsState"; processBlock reads 9 atomics -> toEngineParams -> setParams -> process; no setSource anywhere. State = apvts copyState XML; extra property on apvts.state round-trips (replaceState replaces it). Editor = GenericAudioProcessorEditor (spec R-PS-6 mandates it; R-PS-4 silent, out-of-scope list need MODIFIED deltas). CMake links grains_core + juce_audio_utils (includes audio_formats, gui_basics). Plugin opt-in GRAINS_BUILD_PLUGIN OFF.

### Recommendation
A2 background juce::Thread/ThreadPool loader (latest-wins generation counter, callAsync guarded). B2 JUCE-free core class SourceExchange: pending_ atomic exchange (publisher deletes superseded unseen buffer), audio thread acquire() adopts pending, hands old to retired_ slot (adopt only when retired slot free), calls engine.setSource on change; producer collects retired; dtor frees all. Reject atomic<shared_ptr> (frees on audio thread / not lock-free MSVC) and fixed double buffer. State: absolute path property on apvts.state, async reload in setStateInformation; missing file -> silent, label "Missing: name", path kept; decode failure keeps previous source. UI: custom AudioProcessorEditor with Load button (async FileChooser wav/aiff), label, FileDragAndDropTarget, embedding GenericAudioProcessorEditor as child. Cap length (default 5 min, truncate). Builder in core: planar -> SampleBuffer, >2ch first two, cap, reject empty/bad rate.
TDD JUCE-free: exchange semantics (destruction counter, publish twice, retire, dtor no leaks, 2-thread stress), builder, alloc-free acquire+setSource+process, swap clears pool. Manual: decode, thread lifecycle, FileChooser/drag-drop, state/missing file, replace while playing, rates, VST3-in-DAW stays waived.
Size ~745 lines code+tests (+~120 spec deltas) vs 800 budget: tight. Commits: core, plugin, docs. Defer: waveform, custom sliders UI, embedded audio, relative path, recent files, load-time resample, MIDI, soft limiter, CI, JUCE sourcing, identity codes.
Spec deltas: plugin-shell R-PS-4/6/7 + out-of-scope; new capability sample-source.

### Risks
mailbox memory ordering (no TSan on MSVC); destructor/callAsync races; pool reset dropout on swap; loud +-4 output unlimited; drone starts as soon as file loads; huge files; absolute path leak; stacked on feat/plugin-target.

### Open questions (defaults)
1 max length 5 min truncate; 2 missing file silent+label, path kept; 3 limiter: defer vs cheap +-1 clamp stopgap in plugin.
