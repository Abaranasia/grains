# Exploration: webview-ui (roadmap #5, plugin UI technology)

Mirror of engram `sdd/webview-ui/explore` (full text there). Facts below are marked verified (V) or from memory (M).

## Question
Build the plugin UI with React, plain HTML, or native JUCE?

## Current state (verified in code and specs)
- `GrainsEditor`: fixed-size editor with a native `WaveformView`, a `GenericAudioProcessorEditor` for the 9 parameters, a Load button, a status label and drag-and-drop.
- UI logic already lives in JUCE-free core and is Catch2-tested: `WaveformPeaks`, `WaveformGeometry`, `ParamSpec`. Peaks are an immutable `shared_ptr`, easy to serialize.
- Plugin CMake sets `JUCE_WEB_BROWSER=0`; targets are VST3 and Standalone, no AU. JUCE is pinned at 9.0.0 with a single-SHA pin file. CI builds core on Windows MSVC and Linux clang; the Windows plugin job is build-only.
- AGPL-3.0-or-later with SPDX tags required on first-party files.
- plugin-shell R-PS-6 and waveform-display R-WD-4 mandate the native view and a fixed size; WebView UI is out of scope today.

## WebView facts
- (V) JUCE 9.0.0 `WebBrowserComponent::Options` has `withBackend`, `withNativeIntegrationEnabled`, `withNativeFunction`, `withEventListener`, `withUserScript`, `withInitialisationData`, `withResourceProvider`, `withOptionsFrom`; native functions run on the message thread.
- (V) The Windows default backend is still IE; WebView2 needs `Backend::webview2`, `JUCE_USE_WIN_WEBVIEW2=1` (or static linking) and `NEEDS_WEBVIEW2`, and falls back silently to IE.
- (V) The WebView2 SDK arrives through NuGet at configure time; the Evergreen runtime ships with Windows 11 and most Windows 10 machines, otherwise a bootstrapper is needed. A plugin has no installer by default.
- (V) Relay classes (`WebSliderRelay`, `WebToggleButtonRelay`, `WebComboBoxRelay`) and `Web*ParameterAttachment` bind JS controls to parameters. JS-side gesture mapping to begin/end gestures is not verified (M) and needs a spike.
- (V, forum) Linux support is rough; keyboard focus can be swallowed by the WebView in some hosts (FL Studio, Reaper reports).
- Not verified: JUCE 9 WebView changes, multi-instance and DPI behaviour per host, CPU cost of a 30 Hz bridge, exact WebKitGTK package names.

## Approaches
1. **Native JUCE end to end**: custom components and `LookAndFeel`, keep `WaveformView`. Low-Medium effort. No new runtime, toolchain or CI impact; polish ceiling and no hot reload.
2. **WebView, plain HTML/CSS/JS**: resource provider, relay attachments, canvas waveform. Medium-High. WebView2 runtime dependency with an unusable IE fallback, NuGet at configure time, focus issues, rough Linux, new async bridge.
3. **WebView, React plus bundler**: everything in 2 plus Node and an npm lockfile in a hash-pinned C++ repo. High. Heavy for 9 sliders and one canvas.
4. **Hybrid**: ship native first, then run a time-boxed plain-HTML WebView2 spike behind `GRAINS_UI_WEB=OFF` and decide from measurements. Low now, bounded later.

## Recommendation
Approach 4. Native first; if WebView is pursued, plain HTML/CSS/JS before React. Keep all UI logic in the JUCE-free core so approaches stay swappable, and keep `GenericAudioProcessorEditor` as a fallback when the WebView2 runtime is missing.

## Proposed spike (optional)
Behind `GRAINS_UI_WEB=OFF`, a WebView2 editor serving one static page with a `position` slider via `WebSliderParameterAttachment`. Measure: exactly one begin/end gesture per drag in the DAW automation lane; behaviour with the runtime missing; spacebar reaching the host; two instances open. Output is a go/no-go report, not shipped code.

## Testability (Strict TDD)
Automated for any approach: core logic in Catch2. Automated for web only: JS unit tests with `node --test`, and a bridge schema contract test if generated from `ParamSpec`. Manual for all: JUCE glue, WebView2 runtime handling, DAW focus, gesture recording.

## Open product questions
1. Is a custom, branded, animated look needed, or is a clean functional native look enough for v1?
2. Which platforms and DAWs must v1 support (Windows only, plus macOS/AU, plus Linux)?
3. Live grain display now or after the UI technology is decided?
4. Is a hard WebView2 runtime requirement acceptable on Windows?
5. Installer, or a plain VST3 folder?

## Risks
Missing WebView2 runtime without an installer; host keyboard focus; Linux WebView; NuGet download weakening hash-pinned reproducibility; unverified relay gesture semantics on JUCE 9.

## Sources
Microsoft WebView2 distribution docs; JUCE 9.0.0 `juce_WebBrowserComponent.h`, `juce_WebControlRelays.h` and `BREAKING_CHANGES.md`; JUCE forum threads on Linux WebView, JUCE 8 WebView2 and keyboard propagation.
