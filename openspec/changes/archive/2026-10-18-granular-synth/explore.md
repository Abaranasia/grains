# Exploration: granular-synth (audit of granular_synthesizer_proposal.md)

> Mirror of Engram observation `sdd/granular-synth/explore`.

## Current State

- Repo is a bare Projucer scaffold. `Grains.jucer`: `projectType="guiapp"`, VS2026 exporter (`Builds/VisualStudio2026`), modules: audio_basics, audio_formats, core, data_structures, dsp, events, graphics, gui_basics. **Missing** juce_audio_processors, juce_audio_devices, juce_audio_utils, juce_gui_extra, juce_audio_plugin_client. `Source/` is empty. No tests, no CMake.
- Local JUCE checkout: `H:/Proyectos/Juce/Projucer` (modules path `../../Projucer/modules`) = **JUCE 9.0.0** (minimum C++17). Contains `juce_audio_processors_headless` (JUCE 9 split), `juce_javascript`, WebControlRelays, `examples/Plugins/WebViewPluginDemo.h`. License: AGPLv3 or commercial.

## Audit table (proposal claim → verdict)

| # | Claim/decision | Verdict | Evidence / better alternative |
|---|---|---|---|
| 1 | Grain model `g = A·w·x[p + n·r]`, `y = Σg` | CONFIRMED | Standard; Clouds `grain.h` reads circular buffer with 16.16 phase, window, gains, overlap-add |
| 2 | Scheduler `T = fs/D`, fractional accumulator, block boundaries | CONFIRMED (needs detail) | Clouds: per-sample phasor, grain gets `pre_delay` = in-block sample offset. Csound partikkel adds distribution (periodic/stochastic/deterministic) + sync. Proposal omits sample-accurate in-block offsets and sync vs async scheduling |
| 3 | Fixed-capacity pools, no allocation | CONFIRMED | Clouds `Grain grains_[64]`; SC GrainBuf `maxGrains` fixed at init (default 512); partikkel `imax_grains`, oldest deleted |
| 4 | Illustrative renderer | QUESTIONABLE (4 defects) | (a) Bounds check kills grain mid-window → click; clamp/wrap or cap grain length to available span (Clouds: `min(size, buffer·0.25/pitch_ratio)`). (b) Linear interpolation aliases for pitch > 1 (SC: none/linear/cubic; Clouds: ZOH/linear/Hermite). (c) Per-sample `std::cos` → LUT (Clouds: 4096 entries). (d) No gain normalization: sum scales ~√N (random phase) to N (coherent); Clouds uses `1/√(N−1)` smoothed + window gain compensation. Also: grain length is in output samples, source span = length × rate (unstated) |
| 5 | Hann as default window | CONFIRMED | Truax/Roads: envelope essential. Hann reaches zero (Hamming doesn't → click risk); Gaussian never reaches 0; Tukey = flat-top Hann. Hann at 50% overlap is COLA; at random density only quasi-constant. Spectral claims are standard DSP knowledge, not verified from fetched sources |
| 6 | Grain size/density tables | QUESTIONABLE | Truax: grains typically 10–50 ms, densities hundreds–thousands/s for clouds; proposal's 20 gr/s @ 120 ms is sparse. Use overlap = size × density as primary derived quantity (Clouds exposes overlap directly) |
| 7 | Sync vs async granulation | MISSING | Roads 1988 / Truax 1988 taxonomy. Recommend a scheduler with a "regularity" parameter (periodic + jitter ↔ stochastic) |
| 8 | Global cloud vs per-note voices | CONFIRMED as open decision | Clouds = monophonic cloud; GR-1 hardware = 16 voices × 128 grains. Recommend MVP global cloud, MIDI note = pitch + gate |
| 9 | `juce::Synthesiser` / Voice | QUESTIONABLE | Assumes note = voice; a grain pool should be shared/voice-agnostic. Thin custom MIDI dispatch; Synthesiser only if per-note clouds chosen |
| 10 | Capacity table up to 16 voices / 2048 grains | QUESTIONABLE | Mirrors GR-1 hardware spec, not a plugin target. Back-of-envelope (not measured): 2048 grains × 48 kHz ≈ 98 M grain-samples/s ≈ 1–2 Gcycles/s. Clouds ships 64 grains @ 32 kHz. MVP target 64–128, benchmark before raising |
| 11 | Real-time rules | CONFIRMED | Bencina; Doumler: immutable data + atomics + SPSC FIFO; even `std::mutex::try_lock` unsafe |
| 12 | Sample handoff "safe ownership" | CONFIRMED but UNDERSPECIFIED | Publish immutable sample data atomically; never destruct on the audio thread (deferred free to message thread via FIFO), or double-buffer slots, or `ReferenceCountedObjectPtr` + lock-free exchange |
| 13 | WebBrowserComponent + React/Vite UI | QUESTIONABLE for MVP | Feasible in JUCE 9 (WebSliderRelay, WebViewPluginDemo). Costs: WebView2 runtime, Node toolchain, per-DAW focus/DPI/keyboard quirks, editor lifecycle. Native JUCE UI for MVP; keep state UI-agnostic |
| 14 | Build: CMake + GoogleTest | QUESTIONABLE vs repo | Proposal ignores existing `.jucer`. Pamplejuce (MIT; CMake 3.25+, CPM, Catch2 3.8.1, pluginval, CI) uses Catch2. Recommend CMake; DSP core as pure C++ static lib with Catch2, zero JUCE dependency |
| 15 | Plugin vs standalone | MISSING decision | `guiapp` cannot produce VST3/AU. Standalone is a free wrapper format of a plugin target. Migration easiest via CMake `juce_add_plugin` (FORMATS VST3 Standalone, IS_SYNTH, NEEDS_MIDI_INPUT) |
| 16 | APVTS for params + extra state | CONFIRMED | Use `getRawParameterValue` atomics; SmoothedValue for size/pitch/gain; sample path as non-parameter ValueTree state; state version from day 1 |
| 17 | Presets store sample paths | CONFIRMED | Reference not data; relative-path fallback + relink; load on background thread via AudioFormatManager |
| 18 | Modulation/effects deferred | CONFIRMED | Correctly out of MVP |
| 19 | Tasty Chips GR-1 reference | QUESTIONABLE / dead link | tastychips.com redirects to a domain-for-sale page. GR-1 is **hardware** (Raspberry Pi, 16 × 128); no plugin version found |
| 20 | MVP scope list | QUESTIONABLE (too big) | Includes WAV load, MIDI, persistence, waveform display, UI, offline + RT tests; contradicts its own §17 first milestone. Split |
| 21 | Phase order engine → JUCE RT → WebView → features | CONFIRMED | Fits Strict TDD |
| 22 | Testing strategy | CONFIRMED + gaps | MISSING: seeded injectable RNG contract, golden-file tolerance policy, allocation detection in process(), denormal and NaN/Inf tests |
| 23 | Licensing | CONFIRMED, data missing | JUCE 9 = AGPLv3 or commercial. JUCE 8 tiers (forum): Starter ≤ $20k, Indie ≤ $300k; JUCE 9 terms not verified. VST3 SDK licensing applies |
| 24 | Pitch vs position scan | MISSING | Playback-rate pitch changes consumed source span; time/pitch decoupling comes from position scan rate. Define position as knob + optional scan speed |
| 25 | Grain overflow policy | MISSING | Skip spawn (Clouds, quality tiers at 75% fill), steal oldest (partikkel), steal quietest |
| 26 | Live input / circular buffer | MISSING (later) | Clouds core is a live circular buffer with mirrored interpolation tail |

## Open-source findings

- **Mutable Instruments Clouds** (MIT): fixed `grains_[64]`; available list rebuilt per block; scheduling via deterministic phasor, probabilistic, or trigger; `pre_delay` in-block offsets; size LUT capped by pitch; overlap cubed; gain normalization `fast_rsqrt(N−1)` one-pole smoothed; quality tiers switched at > 75% pool use; envelope pre-rendered per block from 4096-entry LUT; 16.16 fixed-point phase; ZOH/linear/Hermite templated; no heap allocation.
- **SuperCollider GrainBuf**: audio-rate trigger (sample-accurate), interp none/linear/cubic, built-in Hann or buffer envelope, `maxGrains` fixed at init, per-grain parameters frozen at creation.
- **Csound partikkel**: rate in Hz, distribution periodic/stochastic/deterministic, sync input, `imax_grains` (oldest deleted).
- **GRNLR** (JUCE, Projucer): **GPL-3.0** — learning only, do not copy into closed-source code.
- **Multigrain** (JUCE + CMake, standalone): license not shown.
- **Grainflow** (Max/MSP): architecture reference only.
- Limits: Clouds `granular_sample_player.cc` 404 (details from headers); GRNLR/Multigrain/Grainflow internals not inspected.

## JUCE specifics

- `guiapp` cannot be VST3/AU. Recommend plugin target (VST3 + Standalone first; AU on macOS later).
- CMake (`juce_add_plugin`) vs editing `.jucer`: both supported; CMake is needed for CI/tests/pluginval in practice.
- JUCE 9 `juce_audio_processors_headless` may allow GUI-free processor tests (verify API before use).
- Sample loading: `AudioFormatManager::registerBasicFormats()`, `createReaderFor`, read into preallocated `AudioBuffer<float>` on a background thread; resample or store source rate and adjust playback rate.

## Recommended MVP (change 1)

Only the testable DSP core + harness, offline:

1. **Harness**: CMake project, `grains_core` static library (no JUCE), Catch2 v3 test target, runnable via `ctest`. Record the `.jucer` keep/replace decision.
2. **Core**: `SampleBuffer` (immutable, mono/stereo float), `WindowTable` (Hann LUT), `Grain`, `GrainPool` (fixed capacity, steal/skip policy), `GrainScheduler` (fractional accumulator, in-block offsets, injectable seeded RNG, periodic + jitter), `GranularEngine::process(L, R, n)` with params (size ms, density gr/s, position, position jitter, pitch st, pitch jitter, pan spread, gain), overlap normalization, boundary-safe reads, linear interpolation behind an interface.
3. **Tests**: window endpoints/sum, scheduler spacing/fractional drift/block-split invariance, grain bounds, parameter clamping, determinism (same seed ⇒ bit-identical), RMS/peak bounds/no NaN, extremes (1-sample size, max density, ±24 st), no allocation in `process()`.

Estimated ~600–800 lines incl. tests (review budget 800).

**Later changes**: (2) JUCE plugin target + APVTS + sample loader + MIDI (global cloud) + minimal native UI; (3) waveform + position UI; (4) polyphony/per-note clouds, more windows, Hermite, reverse/loop; (5) modulation + effects; (6) optional WebView/React UI; (7) presets/state versioning/relink; (8) visualization/perf; (9) release (pluginval, signing).

## Architecture recommendation

- Layers: `grains_core` (pure C++, RT-safe, unit-tested) → `grains_juce` (AudioProcessor, APVTS, SampleLoader, MIDI mapper) → `grains_ui` (native first; WebView later via same state facade).
- Key classes: SampleData (immutable), WindowTable, Grain, GrainPool<N>, GrainScheduler, Rng (xorshift/PCG), GranularEngine, ParameterSnapshot (POD per block), SampleLoader + SampleExchange (atomic publish, deferred free), MidiMapper.
- Threading: params APVTS atomics → POD snapshot per block (+ SmoothedValue); sample published atomically, released on message thread; UI telemetry via SPSC FIFO (later); no locks in processBlock.
- Tradeoffs: (A) core as separate static lib — fast JUCE-free tests, a bit more CMake (**recommended**); (B) core inside plugin target with `juce::UnitTest` — fewer files, slower, coupled; (C) per-note voices via `juce::Synthesiser` — free polyphony but multiplies pools; defer.

## Open questions (product decisions)

1. Plugin (VST3/AU) with Standalone wrapper, or standalone app only?
2. Target OS/DAWs; macOS/AU needed?
3. Commercial (JUCE license) or open-source/AGPL/personal?
4. Migrate Projucer → CMake?
5. Test framework: Catch2 vs GoogleTest vs `juce::UnitTest`?
6. Global cloud or per-note voices first?
7. File sample only for MVP, live input later?
8. Native UI for MVP, React/WebView later — or is WebView a hard requirement?
9. Which GR-1 features (hardware reference) are must-have?
10. Performance target: max grains, min CPU?
11. Mono or stereo sources from the start?
12. Engine character: Clouds-like (overlap, quality tiers) or classic Truax/Roads (explicit density)?

## Risks

- WebView2 runtime/packaging/DAW variance; Node toolchain.
- Freeing sample buffer on the audio thread during swap.
- Truncation clicks and aliasing in proposal's renderer.
- GPL (GRNLR) / AGPL (JUCE) license contamination.
- Strict TDD: harness must be the first deliverable.
- Local JUCE lives in a folder named `Projucer` at 9.0.0 — CMake path/version pinning decision needed.

## References

- Roads, "Introduction to Granular Synthesis", CMJ 12(2):11–13 (1988) — full text not retrieved
- Truax, "Real-time granular synthesis with a DSP", CMJ 12(2):14–26 (1988): https://www.sfu.ca/~truax/gran.html
- Truax, granulation of sampled sound: https://www.sfu.ca/~truax/gsample.html
- Roads, *Microsound* (2001) — not retrieved
- https://michaelkrzyzaniak.com/AudioSynthesis/2_Audio_Synthesis/11_Granular_Synthesis/
- https://ccrma.stanford.edu/~berlach/220c/granular.html
- Clouds: https://github.com/pichenettes/eurorack/tree/master/clouds/dsp
- SuperCollider GrainBuf: https://docs.supercollider.online/Classes/GrainBuf.html
- Csound partikkel: https://csound.com/docs/manual/partikkel.html
- GRNLR: https://github.com/passivist/GRNLR
- Multigrain: https://github.com/kobemertens/Multigrain
- Grainflow: https://github.com/composingcap/grainflow
- Pamplejuce: https://github.com/sudara/pamplejuce
- JUCE WebBrowserComponent: https://docs.juce.com/master/classjuce_1_1WebBrowserComponent.html
- Doumler, locks in real-time audio: https://timur.audio/using-locks-in-real-time-audio-processing-safely
- Bencina, RT audio 101: https://www.rossbencina.com/code/real-time-audio-programming-101-time-waits-for-nothing
- JUCE tier limits: https://forum.juce.com/t/revenue-limits-for-juce-tiers/61058
- GR-1 review: https://www.soundonsound.com/reviews/tasty-chips-gr-1
