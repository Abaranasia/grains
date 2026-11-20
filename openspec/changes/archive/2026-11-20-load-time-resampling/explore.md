# Exploration: load-time-resampling (roadmap P3)

Engram: `sdd/load-time-resampling/explore` (observation 684). This file mirrors the phase summary.

## Current state (verified in code)

- **Decode path:** `SampleLoader::load` (`Source/plugin/SampleLoader.cpp`) reads at most 2 channels, caps them at `floor(300 * fileRate)` frames, then calls `grains::buildSampleBuffer` with the file's own rate. The builder is JUCE-free and R-SS-5 says it MUST NOT resample. The loader does not know the host rate.
- **Engine behavior:** `GranularEngine::spawn` computes `rate = 2^(semis/12) * srcSr / sampleRate_`, so pitch and speed are already correct when the file rate differs from the host rate. Grain length depends only on the engine rate (R-GR-7).
- **Quality gap:** playback uses `LinearInterpolator` with no anti-alias filter, so downsampling aliases. Load-time resampling fixes the base-rate part only, not aliasing from pitch-up.
- **Host rate changes:** `prepareToPlay` only calls `engine_.prepare(sr, seed)` and allocates scratch. `setStateInformation` can run before `prepareToPlay`, so the target rate may be unknown at restore time.
- **Roadmap:** P3 appears as "out of scope; the builder does not resample" in archived reports and in the `plugin-shell` and `sample-source` specs.
- **Tests:** Catch2 covers core only. Loader and plugin scenarios are manual.

## Approaches

1. JUCE interpolators (Lagrange / WindowedSinc) in the loader. Low effort; not testable in the JUCE-free core harness; Lagrange aliases on downsampling.
2. **Own windowed-sinc in `grains_core` (recommended).** Medium effort; fully Catch2-testable; cutoff `min(1, dst/src)`; abortable per chunk; runs on the loader thread.
3. Hybrid (core interface, JUCE implementation). Splits the logic and loses testability.

Resampling inside `prepareToPlay` is not viable: it runs on the host thread and would have to be async anyway for long files. Choice on rate change: keep the original in RAM (about 2x memory) or re-decode from disk.

## Recommendation

- Pass the loader a target rate, resample to it, publish a buffer whose `sampleRate()` equals the target.
- Keep the engine's `srcSr / sampleRate_` factor as a fallback so playback continues unresampled during a reload.
- On a `prepareToPlay` rate change, re-request the load through the generation gate; do nothing when the rate is unchanged.
- Bit-exact passthrough when the rates already match.
- Apply the 300 s cap at the file rate before resampling and re-check at the target rate.
- Leave `processBlock` untouched.

## Open questions

1. Goal: anti-aliasing quality, or only pre-converting so the engine runs at ratio 1?
2. On a host rate change: automatic reload, or manual? Is playing the unresampled buffer during the gap acceptable?
3. Memory: keep the original in RAM (about 2x) or re-decode from disk (fails if the file moved)?
4. Should the 5-minute cap stay at the file rate? Upsampling 22 kHz to 192 kHz grows a file about 8.7x.
5. Quality target (about 60 dB vs about 100 dB stopband) and load-time budget for a 5-minute file.
6. Bit-exact passthrough when rates match (recommended)?
7. Keep the engine's rate-ratio compensation that R-GR-7 relies on (recommended)?
8. Is pitch-shift aliasing in scope? Load-time resampling does not fix it.

## Risks

- Forecast 600 to 900 lines against the 800-line budget; consider a core resampler plus spec change, then plugin rate-change wiring.
- Peak load memory: a 300 s stereo file at 192 kHz is about 460 MB of decode buffer plus the output buffer.
- Races between `prepareToPlay` and an in-flight load, restore before prepare, repeated `prepareToPlay` at the same rate.
- R-SS-5 and R-GR-7 wording must be amended consistently; plugin side has manual verification only.
- Pitch-shift aliasing remains; users may expect it fixed.
