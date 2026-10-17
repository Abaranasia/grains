# Granular Synthesizer — Product and Engineering Proposal

**Purpose:** A planning and implementation brief for an AI coding agent or development team.

**Reference direction:** A granular synthesizer inspired by the general capabilities and workflow of the Tasty Chips GR-1, while developing an original product identity, interface, and feature set.

**Proposed technology:** C++ and JUCE for the audio engine and application/plugin infrastructure; React and TypeScript hosted in a JUCE WebView for the user interface.

---

## 1. Executive summary

Build a cross-platform granular synthesizer that can run as a standalone desktop application and, where practical, as a plugin. The proposed architecture separates:

1. **Real-time audio engine:** C++ granular synthesis, voice and grain scheduling, sample playback, modulation, mixing, and effects.
2. **JUCE application/plugin layer:** audio I/O, MIDI, host integration, parameter automation, state/preset handling, sample loading, and native/WebView hosting.
3. **React UI:** controls, waveform editing, grain-cloud visualization, modulation UI, preset browser, and other interactive views.

This is technically achievable. The main risks are real-time thread safety, CPU scaling at high grain counts, sample ownership and loading, plugin state management, and WebView behavior across operating systems and DAW hosts.

**Recommended development strategy:** implement and test a standalone-capable core engine first; integrate it with JUCE audio processing; establish a minimal WebView bridge early; then expand features and validate a plugin target in real hosts before investing heavily in the finished UI.

## 2. Product goals and scope

### 2.1 Product goals

- Create an expressive, sample-based granular instrument.
- Support overlapping grains with controllable duration, density, position, pitch, envelopes, randomization, and pan.
- Support MIDI performance and eventually polyphony.
- Provide an original, polished UI using React and TypeScript.
- Offer standalone use and a plugin build from a shared DSP engine.
- Support presets, robust sample references, and project/state restoration.
- Keep the audio engine independent from the GUI and WebView.

### 2.2 Suggested initial scope

The minimum viable product (MVP) should include:

- Load one WAV file.
- Play and mix multiple overlapping grains.
- Grain size, density, source position, pitch, position jitter, and pitch jitter.
- A grain window/envelope, initially Hann.
- Basic looping and sample-boundary handling.
- MIDI-triggered playback or one clearly defined global-grain mode.
- A basic master output stage and gain control.
- Parameter persistence.
- Basic waveform display and UI-to-engine parameter control.
- Offline rendering tests and real-time audio tests.

Defer complex modulation matrices, extensive effects, multi-sample layering, live recording, advanced spectral features, and elaborate visualizations until the core engine is stable.

## 3. Reference instrument and product differentiation

The Tasty Chips GR-1 is a useful high-level reference for the category: a granular instrument combining sample playback, grain-cloud control, performance, modulation, effects, presets, and a visual workflow. Treat it as a reference for product expectations, not as a blueprint to copy.

Potential areas for original differentiation:

- Distinctive grain scheduling and random distributions.
- An unusually intuitive sample-region and grain-cloud editor.
- Expressive modulation and macro controls.
- Advanced multichannel or stereo granulation.
- Useful visual feedback for grain position, duration, and pitch.
- A focused workflow for ambient textures, rhythmic resynthesis, or experimental sound design.

Do not copy protected branding, proprietary assets, or a distinctive interface design.

## 4. Granular synthesis fundamentals

Granular synthesis divides a source signal into short segments called *grains*. Each grain has its own source position, duration, playback rate, envelope, amplitude, and potentially stereo position. Many grains overlap to form the output.

A basic grain model is:

\[
g_i[n] = A_i w_i[n] x[p_i + n r_i]
\]

Where:

- \(g_i[n]\): output of grain \(i\)
- \(A_i\): grain amplitude
- \(w_i[n]\): grain window/envelope
- \(x\): source sample
- \(p_i\): source start position
- \(r_i\): playback rate (pitch and sample-rate dependent)
- \(n\): sample index within the grain

The combined output is:

\[
y[n] = \sum_{i=1}^{N} g_i[n]
\]

where \(N\) is the number of active grains.

### 4.1 Primary parameters

| Parameter | Meaning | Implementation notes |
|---|---|---|
| Grain size | Duration of each grain | Convert milliseconds to samples; clamp to supported range |
| Density | Average grain spawn rate | Grains/second or interval; support fractional scheduling |
| Position | Source location where a grain starts | Normalized position or source sample index |
| Position jitter | Random variation in source position | Use a defined distribution and bounded range |
| Pitch | Grain playback speed/pitch | Convert semitones to rate ratio |
| Pitch jitter | Random pitch variation | Define units, e.g. semitones |
| Window | Grain amplitude envelope | Hann initially; optionally Gaussian, triangular, custom |
| Pan | Stereo placement | Define mono-to-stereo and stereo-source behavior |
| Direction | Forward/reverse playback | Handle bounds, looping, and negative playback rates |
| Spread | Variation among grains | Define whether it affects position, pitch, pan, timing, or several |

### 4.2 Grain scheduling

For fixed density \(D\) grains per second and sample rate \(f_s\), the average spawn interval is:

\[
T = \frac{f_s}{D}
\]

At 48 kHz and 20 grains/second, the average interval is 2,400 samples.

Use a phase accumulator or countdown with fractional precision to avoid timing drift. The scheduler should account for audio-block boundaries, modulation of density, tempo synchronization (later), and the chosen global or per-voice model.

### 4.3 Grain data structure (illustrative)

```cpp
struct Grain
{
    double position = 0.0;
    double playbackRate = 1.0;

    int length = 0;
    int age = 0;

    float amplitude = 1.0f;
    float pan = 0.0f;

    bool active = false;
};
```

This is a starting point, not a complete production implementation. Consider using fixed-capacity pools, explicit source-buffer ownership, window tables, robust boundary handling, and well-defined grain-stealing behavior.

### 4.4 Simplified grain renderer (illustrative only)

```cpp
float renderGrain(Grain& grain,
                  const float* sample,
                  int sampleLength)
{
    if (!grain.active)
        return 0.0f;

    const double pos = grain.position;
    const int index = static_cast<int>(pos);
    const float frac = static_cast<float>(pos - index);

    if (index < 0 || index + 1 >= sampleLength)
    {
        grain.active = false;
        return 0.0f;
    }

    const float a = sample[index];
    const float b = sample[index + 1];
    const float source = a + frac * (b - a);

    const float phase = static_cast<float>(grain.age)
                      / static_cast<float>(grain.length);

    const float window =
        0.5f - 0.5f * std::cos(
            juce::MathConstants<float>::twoPi * phase);

    const float output = source * window * grain.amplitude;

    grain.position += grain.playbackRate;
    grain.age++;

    if (grain.age >= grain.length)
        grain.active = false;

    return output;
}
```

Important limitations of this snippet:
- It assumes a mono source pointer and does not handle stereo.
- It uses linear interpolation and a per-sample cosine, neither of which should be assumed optimal for production.
- It does not implement looping, reverse playback, pitch conversion, source-region constraints, window-table lookup, or normalization.
- It needs careful handling of `length <= 0`, sample boundaries, and envelope endpoints.
- It is an explanatory sketch, not code to copy directly into a production engine.

### 4.5 Voice architecture

Distinguish **musical polyphony** (simultaneous notes/voices) from **grain density** (simultaneous grains inside a voice). A conceptual architecture:

```text
Synthesizer
├── Voice 1
│   ├── Grain scheduler
│   └── Grain pool
├── Voice 2
│   ├── Grain scheduler
│   └── Grain pool
├── ...
└── Master processor
```

JUCE classes such as `juce::Synthesiser`, `juce::SynthesiserVoice`, and `juce::SynthesiserSound` may help with conventional MIDI voice allocation. A custom voice manager may be more suitable if the design requires global grain scheduling or unusual note semantics.

**Decision to make early:** choose either a global grain cloud controlled by MIDI or per-note grain clouds as the first implementation. A later design may support both, but the MVP should have one clearly defined behavior.

## 5. Real-time audio requirements

At 48 kHz with 128 samples per block, the nominal time budget per audio block is about 2.67 ms. The callback must reliably finish within its deadline.

### 5.1 Do not do these things in the audio callback

- Allocate or free heap memory.
- Read or write files.
- Perform network operations.
- Wait on mutexes or other blocking synchronization.
- Call into the UI or wait for WebView/React.
- Perform unbounded work.
- Trigger expensive operations whose duration is unpredictable.

### 5.2 Recommended practices

- Load and decode samples on a background thread.
- Publish completed sample buffers to the audio thread using a safe ownership/handoff design.
- Preallocate grain pools and scratch buffers.
- Use atomics, lock-free queues where appropriate, or carefully designed immutable snapshots.
- Keep parameter updates bounded and thread-safe.
- Precompute window tables and other reusable data.
- Profile worst-case processing, not just average CPU.
- Consider denormal handling and SIMD only where measurement supports it.
- Ensure no grain can read outside a valid source region.
- Define behavior when grain capacity is reached (skip, steal, or prioritize grains).

Memory ownership is particularly important: the audio thread must never hold a pointer to a sample buffer that is freed or replaced while it is rendering.

## 6. Recommended system architecture

Use three layers with clear responsibilities.

```text
┌─────────────────────────────────────────────┐
│ React + TypeScript UI                       │
│ Knobs, waveform, grain cloud, modulation,   │
│ preset browser, themes, interactive views   │
└──────────────────────┬──────────────────────┘
                       │ Typed WebView bridge
┌──────────────────────▼──────────────────────┐
│ JUCE application/plugin layer                │
│ AudioProcessor, APVTS, MIDI, state, samples, │
│ host integration, WebView host, serialization│
└──────────────────────┬──────────────────────┘
                       │ Safe parameter/state APIs
┌──────────────────────▼──────────────────────┐
│ Independent C++ DSP core                    │
│ Grain scheduler, sample reads, voices,      │
│ envelopes, mixing, modulation, effects      │
└─────────────────────────────────────────────┘
```

### 6.1 JUCE responsibilities

Relevant JUCE components and classes include:

- `juce_audio_processors`: plugin processor and host integration.
- `juce_audio_basics`: audio buffers, MIDI and basic audio types.
- `juce_audio_devices`: standalone device management.
- `juce_dsp`: filters and common DSP components.
- `juce_gui_extra`: `juce::WebBrowserComponent` and related WebView functionality.
- `juce_audio_formats`: audio file reading and supported format handling.
- `juce::AudioProcessorValueTreeState` (APVTS): host-visible parameter management and state serialization.
- `juce::AudioProcessorEditor`: plugin editor container.
- `juce::Synthesiser`, `juce::SynthesiserVoice`, `juce::SynthesiserSound`: possible building blocks for polyphony.

Use one shared DSP core for standalone and plugin targets; do not fork the engine into separate implementations.

### 6.2 React responsibilities

Use TypeScript and React for:
- Custom knobs and sliders.
- Waveform display and draggable regions.
- Grain-cloud visualization.
- Modulation matrix and envelope editors.
- Sample and preset browsers.
- Tooltips, menus, themes, and layout.
- Animations and visual feedback.

Suggested rendering choices:
- SVG for knobs, envelopes, and vector UI.
- Canvas 2D for waveforms and many frequently updated visual elements.
- WebGL only if profiling shows a real need for more complex GPU-rendered visuals.

### 6.3 JUCE WebView integration

JUCE's `juce::WebBrowserComponent` provides WebView functionality and mechanisms for communication between native code and web content. Verify the API against the exact JUCE version chosen; WebView options and integration details can change between versions.

The WebView should be a UI only. Never route audio samples or per-sample processing through JavaScript.

### 6.4 Typed bridge message examples

UI-to-native parameter request:

```json
{
  "type": "parameter",
  "id": "grainSize",
  "value": 120.0
}
```

Native-to-UI waveform snapshot:

```json
{
  "type": "waveform",
  "sampleId": 1,
  "peaks": [0.12, 0.25, 0.18, 0.42]
}
```

These are illustrative message formats. Define and validate a versioned protocol with explicit schemas, permitted message types, bounds checking, and error handling. Avoid accepting arbitrary commands from the UI.

### 6.5 Communication design

| Message | Direction | Expected frequency | Notes |
|---|---|---|---|
| UI parameter request | UI → native | Interactive | Validate ID and value; route through parameter system |
| Parameter/state update | Native → UI | Moderate | UI must reflect host automation and preset changes |
| Sample load request | UI → native | Occasional | Perform file I/O off audio thread |
| Waveform data | Native → UI | On demand or occasional | Send downsampled peaks, not full audio at high rates |
| Grain visualization | Native → UI | 30–60 FPS target if needed | Use snapshots or bounded telemetry, never block audio |
| Preset operation | Both | Occasional | Keep persistence off audio thread |
| MIDI activity | Native → UI | Event-driven | Coalesce or throttle if needed |

For plugin automation, host-visible parameters should be represented in JUCE's parameter system. React should not be the authoritative store for DSP parameter values. UI values should be synchronized from native state so automation, MIDI mapping, and preset recall are represented correctly.

### 6.6 WebView risks

Test these early:
- Windows WebView2 runtime availability and packaging.
- macOS WebKit behavior.
- Plugin host sandbox restrictions.
- UI startup latency and memory footprint.
- Focus and keyboard behavior.
- High-DPI and resizing behavior.
- Drag-and-drop.
- Audio-thread/UI-thread isolation.
- Differences between DAWs.

Start with a tiny WebView prototype inside a plugin before building a complex interface. Consider a minimal native fallback UI if commercial reliability requirements justify it.

## 7. Core subsystems

### 7.1 Sample engine

MVP:
- Load WAV.
- Handle mono and stereo sources.
- Keep immutable decoded sample data while in use.
- Support sample regions and looping.
- Handle missing files and loading failures.
- Provide downsampled waveform peak data to the UI.

Later:
- More audio formats.
- Transient detection and slicing.
- Sample library management.
- Live input recording.
- Multi-sample layers.

### 7.2 Grain engine

MVP:
- Grain pool and scheduler.
- Grain duration and density.
- Position and position jitter.
- Pitch and pitch jitter.
- Hann window.
- Safe boundaries and predictable grain termination.

Later:
- Additional windows.
- Tempo sync.
- Reverse playback and advanced looping.
- Grain-specific pan and spread.
- More sophisticated random distributions.
- Per-grain modulation and filtering if performance permits.

### 7.3 Modulation engine

Start with a limited, explicit set of sources:
- LFOs.
- Envelopes.
- Velocity.
- MIDI modulation.
- Random source.
- Macro controls.

Define parameter units, modulation ranges, smoothing, and combination rules. Add a general-purpose modulation matrix only after the basic modulation model is stable.

### 7.4 Effects

Start with a simple master gain and optionally a filter. Add effects as separate modules, such as saturation, delay, reverb, and stereo processing. Keep the grain scheduler independent from effects processing.

### 7.5 Voice and grain capacity

Example stress-test scenarios (not performance guarantees):

| Scenario | Voices | Grains per voice | Total active grains |
|---|---:|---:|---:|
| Light | 1 | 16 | 16 |
| Moderate | 4 | 32 | 128 |
| Heavy | 8 | 64 | 512 |
| Extreme | 16 | 128 | 2,048 |

Benchmark on target hardware. Cost depends on more than grain count: stereo handling, interpolation, windowing, filters, modulation, and memory access all matter.

Measure:
- Audio callback duration and worst-case spikes.
- CPU at multiple sample rates and buffer sizes.
- Active grain count.
- Dropped/skipped spawn events.
- Glitches under load.
- Memory usage with multiple sample buffers.
- Behavior during sample loads and preset changes.

## 8. Parameter, state, preset, and sample management

Separate:
1. **Host-visible parameters:** grain size, density, pitch, filter cutoff, and other automatable controls.
2. **Instrument state:** modulation assignments, sample regions, effect routing, UI-independent configuration.
3. **External assets:** sample files, impulse responses, and user libraries.

Use APVTS for appropriate host-visible parameters. Additional structured state may be required for complex configurations. Version the state/preset schema from the beginning.

### 8.1 Example preset JSON

This is a conceptual schema, not a final specification:

```json
{
  "name": "Frozen Atmosphere",
  "version": 1,
  "sample": {
    "path": "samples/atmosphere.wav",
    "start": 0.15,
    "end": 0.85
  },
  "grain": {
    "size": 120,
    "density": 25,
    "position": 0.42,
    "positionJitter": 0.2,
    "pitch": 0,
    "pitchJitter": 3
  },
  "modulation": [],
  "effects": {}
}
```

Before implementation, specify units and valid ranges. For example, decide whether `size` means milliseconds, whether `pitch` is semitones, and whether `position` is normalized to `[0, 1]`. The example uses normalized sample start/end and position values but does not define a final parameter contract.

### 8.2 Sample reference strategy

Choose how presets locate samples:
- Absolute file paths.
- Relative paths in a managed library.
- Optional embedding in a project or preset.
- Relinking UI for missing files.

For DAW projects, prioritize portability and graceful recovery when a file is moved or unavailable. Avoid storing huge waveform arrays in ordinary parameter state. Save only the necessary reference and configuration unless embedding is an intentional feature.

## 9. Suggested repository structure

```text
granular-instrument/
├── CMakeLists.txt
├── Source/
│   ├── Core/
│   │   ├── GranularEngine.h
│   │   ├── Grain.h
│   │   ├── GrainScheduler.h
│   │   ├── SampleBuffer.h
│   │   └── ModulationEngine.h
│   ├── Plugin/
│   │   ├── PluginProcessor.h
│   │   ├── PluginProcessor.cpp
│   │   ├── PluginEditor.h
│   │   └── PluginEditor.cpp
│   ├── Audio/
│   │   ├── SampleLoader.h
│   │   └── EffectsProcessor.h
│   └── Tests/
│       ├── GrainTests.cpp
│       ├── SchedulerTests.cpp
│       └── EngineTests.cpp
├── ui/
│   ├── src/
│   │   ├── components/
│   │   ├── panels/
│   │   ├── hooks/
│   │   ├── state/
│   │   ├── bridge/
│   │   └── styles/
│   ├── public/
│   ├── package.json
│   └── vite.config.ts
└── Resources/
    ├── Presets/
    └── Assets/
```

This is a suggested layout, not a requirement. The implementation agent should adapt it to JUCE's CMake conventions and the chosen plugin targets.

## 10. Development roadmap

### Phase 1 — Basic offline granular engine

**Tasks**
- Establish CMake and a C++ test target.
- Load a WAV into a known sample-buffer representation.
- Implement one grain and its window.
- Implement overlapping grains and a scheduler.
- Add grain size, density, position, and pitch.
- Add bounds checking and deterministic test modes.
- Render audio offline and inspect output.

**Exit criteria**
- Produces non-silent output for valid samples.
- No out-of-bounds reads under tested conditions.
- Grain duration and scheduler behavior match documented units.
- Unit tests cover source boundaries and parameter extremes.
- Repeatable offline tests are possible.

### Phase 2 — JUCE real-time audio and MIDI

**Tasks**
- Integrate the core into JUCE audio processing.
- Add MIDI handling and choose global-cloud or per-note voice behavior.
- Implement safe parameter handoff.
- Preallocate pools and scratch buffers.
- Test several sample rates and block sizes.
- Add stress tests and audio-thread safety checks.

**Exit criteria**
- Stable real-time playback without routine glitches.
- No file I/O, allocation, or blocking locks in the audio callback.
- Predictable behavior when grain capacity is reached.
- Correct handling of sample boundaries and voice shutdown.
- Basic CPU measurements documented.

### Phase 3 — React/WebView integration

**Tasks**
- Create a React + TypeScript UI using Vite.
- Embed it in a minimal JUCE editor.
- Establish a typed, versioned bridge protocol.
- Implement a few controls and parameter synchronization.
- Display a downsampled waveform.
- Validate packaging and runtime behavior on target platforms.

**Exit criteria**
- UI parameter changes reach the engine through safe native parameter APIs.
- UI reflects parameter changes originating in the host/native side.
- WebView failures or reloads do not destabilize audio.
- No audio processing depends on JavaScript or WebView timing.

### Phase 4 — Instrument features

**Tasks**
- Add position/pitch jitter and more window shapes.
- Add sample regions and looping.
- Add modulation sources.
- Add effects and stereo processing.
- Add preset save/load and state versioning.
- Add missing-sample handling.

**Exit criteria**
- Presets reliably restore all relevant instrument state.
- Sample references fail gracefully and can be relinked.
- Parameter ranges, units, and modulation behavior are documented.
- No regressions in the audio tests.

### Phase 5 — Visualization and optimization

**Tasks**
- Add grain-cloud visualization using snapshots.
- Add modulation visualization.
- Profile high-density scenarios.
- Optimize demonstrated hotspots.
- Improve sample loading and memory handling.
- Test UI performance at realistic refresh rates.

**Exit criteria**
- Visualization never blocks the audio thread.
- CPU and memory targets are documented and tested.
- UI stays responsive at supported grain capacities.
- Worst-case audio callback behavior remains within the deadline.

### Phase 6 — Plugin release

**Tasks**
- Build VST3 and AU targets where appropriate.
- Validate in several DAWs and standalone hosts.
- Test automation, MIDI routing, bypass, and state restoration.
- Test startup, shutdown, and repeated editor open/close cycles.
- Prepare installers, signing, and release assets.
- Review JUCE, plugin-format, and third-party licensing.

**Exit criteria**
- Plugin passes applicable format validation.
- Project/preset state restores correctly in tested hosts.
- No known reproducible audio-thread safety issues.
- Installation and missing-runtime behavior are documented.
- Compatibility matrix and known limitations are recorded.

## 11. Technology recommendations

| Layer | Recommendation |
|---|---|
| Language | Modern C++ (C++20 if supported by selected JUCE version and target toolchains) |
| Audio framework | JUCE |
| DSP | Independent C++ engine plus JUCE DSP where useful |
| Plugin parameters/state | AudioProcessorValueTreeState for suitable automatable parameters |
| UI | React + TypeScript |
| UI bundling | Vite |
| Build | CMake with JUCE integration |
| WebView | `juce::WebBrowserComponent`; verify exact API for chosen JUCE release |
| Rendering | SVG + Canvas 2D; WebGL only if warranted |
| Tests | GoogleTest or an equivalent C++ test framework, plus offline audio tests |
| Version control | Git |

Avoid adding extra frameworks until a clear requirement justifies them. Keep the core DSP independent from the UI and, where practical, independent from JUCE-specific GUI classes.

## 12. Licensing and distribution

Before commercial distribution:
- Review current JUCE licensing terms for the intended revenue, distribution, and use case.
- Review VST3/AU and any other format-specific requirements.
- Review third-party dependency licenses.
- Use properly licensed samples and impulse responses.
- Plan macOS code signing/notarization and Windows installation.
- Confirm WebView runtime and packaging requirements.
- Test plugin validation and DAW compatibility.

Licensing and distribution terms can change; verify current official documentation rather than relying on an old summary.

## 13. Testing strategy

Use multiple levels of testing.

### Unit tests
- Grain lifecycle and duration.
- Window endpoints and envelope shape.
- Playback-rate and pitch conversion.
- Scheduler interval and fractional timing.
- Sample-region boundaries.
- Reverse playback and looping, when implemented.
- Parameter clamping and invalid values.

### Offline audio tests
- Silence/non-silence conditions.
- Peak and RMS bounds.
- Clipping and unexpected DC.
- Deterministic output with a seeded random generator.
- Regression comparisons with tolerances.
- Behavior at very low/high density and grain size.

### Real-time tests
- Multiple sample rates.
- Small and large block sizes.
- High voice/grain counts.
- Repeated preset changes.
- Sample loads while audio is running.
- Repeated plugin editor open/close.
- Host automation and MIDI.

### UI/bridge tests
- Unknown message types are rejected.
- Invalid parameter IDs and out-of-range values are handled.
- Bridge messages are versioned.
- UI reconnect/reload does not disrupt audio.
- Visualization messages are bounded and coalesced.

## 14. Key design decisions the implementation agent must resolve

Do not silently assume answers to these questions. Make a decision record with rationale.

1. **Primary product:** standalone app, plugin, or both?
2. **First playback model:** global grain cloud or per-note grains?
3. **Sample scope:** one sample, multiple slots, or multi-sample layering?
4. **Parameter units:** define units and ranges for every control.
5. **Grain capacity:** set supported maximum and overflow behavior.
6. **Voice stealing:** how to handle MIDI polyphony limits.
7. **Stereo policy:** how mono and stereo samples are panned and processed.
8. **Interpolation:** linear initially, or higher-quality interpolation from the start?
9. **Randomization:** distribution, seed strategy, and reproducibility for tests.
10. **Modulation:** which sources and destinations are supported in the MVP?
11. **Preset portability:** path-based references, managed library, or embedding?
12. **WebView support matrix:** exact operating systems, runtimes, and DAW hosts.
13. **Minimum supported OS and CPU targets.**
14. **Plugin formats and licensing.**
15. **Performance budget:** maximum CPU and memory under specified test conditions.

## 15. Linked source documents and references

The following links were referenced during proposal development. Verify current content and API details before implementation.

### JUCE and C++
- JUCE documentation: https://docs.juce.com/
- JUCE `WebBrowserComponent`: https://docs.juce.com/master/classjuce_1_1WebBrowserComponent.html
- C++ reference: https://en.cppreference.com/

### Real-time audio and DSP
- Ross Bencina, “Real-time audio programming 101: time waits for nothing”: https://www.rossbencina.com/code/real-time-audio-programming-101-time-waits-for-nothing
- DSPRelated: https://www.dsprelated.com/
- Granular synthesis resources: https://www.granularsynthesis.com/

### React and UI
- React Learn: https://react.dev/learn
- Vite: https://vite.dev/

### Testing
- GoogleTest: https://github.com/google/googletest

### Product reference
- Tasty Chips official site: https://www.tastychips.com/
- The GR-1 product is a high-level reference for feature expectations; verify current product documentation independently before relying on exact feature claims.

## 16. Suggested instructions for an AI coding agent

Use the following as a starting instruction for a separate AI planning/implementation system:

> You are the technical lead for implementing this granular synthesizer. Treat this document as a product and architecture proposal, not as a complete specification. First inspect the repository and the exact JUCE version and toolchain available. Before making large changes, produce:
>
> 1. A concise implementation plan with milestones and dependencies.
> 2. A decision record for the unresolved questions in Section 14.
> 3. A proposed repository/build layout.
> 4. A parameter specification table with IDs, types, units, ranges, defaults, and automation behavior.
> 5. A typed and versioned native/WebView bridge schema.
> 6. An audio-thread safety plan and sample-buffer ownership model.
> 7. A test plan with explicit exit criteria.
>
> Implement in small, compilable increments. Start with a testable offline C++ granular engine, then JUCE real-time integration, then a minimal React/WebView bridge. Do not implement the full UI before the engine is tested. Do not allocate, perform file I/O, block on locks, or wait for UI/WebView work in the audio callback. Do not invent unsupported JUCE APIs: verify them against the installed version and official documentation. Keep the DSP engine independent from UI code. Add tests with each feature, run the build and tests, and report exact commands, results, and unresolved issues. Clearly label assumptions and do not claim tests passed unless they were actually run.

---

## 17. Final recommendation

The project is feasible with JUCE/C++ and React/WebView. The most reliable route is to build a small, measurable, independently testable DSP engine first, then connect it to JUCE and only then expand the interface and instrument features.

The first milestone should be deliberately modest: **load one sample, schedule and render overlapping grains, control grain size/density/position/pitch, and verify the result with offline audio tests.** That establishes the foundation for everything else.
