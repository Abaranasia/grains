# Design: Load-time resampling (P3)

## Technical Approach

A JUCE-free, throw-free `grains::resample` in `grains_core` converts planar float channels offline with a Kaiser-windowed sinc evaluated from a polyphase table. Each output frame is computed independently at its exact input time, so the result has zero delay, chunk-invariant output and the exact `max(1, round(n*dst/src))` length (R-RS-1, R-RS-5). `buildSampleBuffer` gains a target-rate overload that caps, resamples and publishes at the target. `SampleLoader` captures a target rate per request and gains `retarget(rate)`, which `prepareToPlay` calls. `processBlock` and the engine stay unchanged (R-GR-7 keeps the `srcSr/sampleRate_` fallback).

## Architecture Decisions

| Decision | Options | Tradeoff | Choice |
|---|---|---|---|
| Kernel | Lagrange; JUCE `WindowedSincInterpolator`; own Kaiser sinc | JUCE is not testable in core and does not reach 100 dB | Kaiser-windowed sinc, cutoff at `fc = min(1, dst/src)` of the source Nyquist (transition centre) |
| Filter length | transition 0.9 to 1.0 fc (about 142 taps); 0.9 to 1.1 fc (about 72 taps) | Spec R-RS-3 puts the stopband at `>= 1.1 fc`, which halves cost | Design A = 110 dB (10 dB margin): `beta = 0.1102*(110-8.7) = 11.16`. Half-width `H = 36` lower-rate periods, so `2*ceil(36/fc)+1` input taps (73 up, 145 for 96k to 48k) |
| Phase strategy | exact rational L/M; direct `sin` per tap; oversampled table | Rational needs integer rates and a large L. Direct evaluation is slow | Table with `P = ceil(1024*fc)` rows per input sample (1024 per zero crossing, about 300 KB for any ratio). Linear blend of two adjacent row dot products; skip the second when `frac == 0` (96k to 48k, 1:2) |
| Position | phase accumulator; `t = m*(src/dst)` per frame | An accumulator drifts and depends on chunking | `t = double(m) * ratio`, `i = floor(t)`. Phase error is below 1e-8 samples for `m < 2^27` |
| Edges / latency | pre-roll and trim; zero padding by index clamp | Trimming adds a copy | Symmetric kernel centred on `t`, taps outside `[0,n)` skipped. No delay and no extra buffer |
| Gain | analytic scale; per-row normalisation | | Each row normalised to sum 1 (exact DC); rows stored as `float`, built in `double` |
| Accumulation | float; double; SIMD intrinsics | MSVC `/fp:precise` will not vectorise float reductions | `double`, four fixed-order partial sums. Deterministic. The [perf] test guards the budget |
| Passthrough | always filter; copy | | `src == dst` (exact `==`) copies bytes (R-RS-2). No target (before prepare) uses the old 3-argument builder |
| Rate-change source | keep original in RAM; re-decode | User decision 2 | Re-decode from disk. A failed reload keeps the current buffer |
| Restore before prepare | guess 48k; file rate | User decision 1 | Target `0` means unknown, so the file is published at its own rate |
| Reload trigger | processor-side bookkeeping; loader `retarget` | The loader already owns `lock_` and the gate | `retarget` under `lock_`. Atomic with `request()` |

## Interfaces / Contracts

```cpp
// grains/ChannelView.h (moved out of SampleBuilder.h, which includes it)
// grains/Resampler.h
inline constexpr std::size_t kResampleChunkFrames = 65536;
enum class ResampleStatus { Ok, Aborted, InvalidArgument, OutOfMemory };
struct ResampleOptions { std::size_t maxFrames = SIZE_MAX; std::size_t chunkFrames = kResampleChunkFrames; };
struct ResampleResult { ResampleStatus status; std::vector<std::vector<float>> channels; bool capped = false; };
std::size_t resampledFrames(std::size_t n, double src, double dst) noexcept;  // max(1, llround), 0 if invalid
// shouldAbort is polled before every chunk; true => Aborted, channels cleared.
ResampleResult resample(const ChannelView* ch, std::size_t numChannels, double src, double dst,
                        const std::function<bool()>& shouldAbort, ResampleOptions = {}) noexcept;
// grains/SampleBuilder.h: BuildError gains Aborted.
BuildResult buildSampleBuffer(const ChannelView*, std::size_t, double fileRate, double targetRate,
                              const std::function<bool()>& shouldAbort) noexcept;
// Caps input at maxSourceFrames(fileRate), output at maxSourceFrames(targetRate); invalid target => BadSampleRate.
```

**Loader** (`lock_` guards everything; lock order unchanged: `lock_`, then the gate):
- `Request` gains `double targetRate` (0 means file rate) and `Origin::Reload`. `request()` copies `target_`.
- New state: `target_`, `latest_` (the last issued Request), `settled_` (its generation, written at the end of `load`), `published_` {path, original, displayName, label, rate}. These are set after an accepted commit and reset on a clear. They are written under `lock_` *after* `commitGated` because the gate callback must not take `lock_`. The window in between still reads as "in flight", which is safe.
- `bool retarget(double rate)`: equal to `target_` returns no-op. Otherwise it stores the rate. If `latest_` is in flight it reissues that request with a new generation, so a restore before prepare decodes only once. Otherwise, if `published_->rate != rate`, it issues `Reload{ {path}, original, displayName }`.
- Reload does not post "Loading:". On failure it keeps the source and reposts `published_->label` (never "Missing"). On success it posts the normal "Loaded:" text.
- `load` calls the target overload with `shouldAbort = !isCurrent()`. `Aborted` returns silently. The decode buffer scope ends before staging.

**Processor**: `prepareToPlay` calls `loader_.retarget(sampleRate)` after `engine_.prepare`. `onLoadResult` writes the four APVTS properties for `loaded && User` **before** the generation filter, because results arrive in publish order. Only the label is filtered. This fixes the case where a reload supersedes an unprocessed User result and the path is never persisted.

## Data Flow

    prepareToPlay(sr) -> retarget(sr) --lock_--> reissue latest_ | Reload(published_) | no-op
    loader: decode (capped @file rate) -> resample (poll gate/64k frames) -> free decode
          -> stage -> commitGated -> published_ -> post -> onLoadResult

## RT-safety and memory

Audio thread: unchanged (`acquire` only). `retarget` allocates strings on the prepare thread, never in `processBlock`. Peak = decode + output + table (about 0.3 MB) + the live published buffer. This is below today's decode + copy at equal rates. Worst case is a 384k file to 48k: 921 MB + 115 MB. Output is limited to 115.2M frames.

## File Changes

| Slice | File | Action | Lines |
|---|---|---|---|
| 1 | `Source/core/include/grains/{ChannelView,Resampler}.h` | Create | ~60 |
| 1 | `Source/core/src/Resampler.cpp` | Create (Bessel I0, table, render) | ~160 |
| 1 | `Source/core/{include/grains/SampleBuilder.h,src/SampleBuilder.cpp}` | Overload, `Aborted` | ~50 |
| 1 | `Source/core/CMakeLists.txt`, `tests/CMakeLists.txt` | Add sources | 2 |
| 1 | `tests/test_resampler.cpp` | Create | ~240 |
| 1 | `tests/test_sample_builder.cpp`, `tests/test_grain_render.cpp` | R-SS-5 and R-GR-7 scenarios | ~90 |
| 2 | `Source/plugin/SampleLoader.{h,cpp}` | Target, retarget, Reload | ~100 |
| 2 | `Source/plugin/PluginProcessor.cpp` | retarget, persist order | ~10 |

Code is about 710 lines, plus the existing spec deltas (about 250). That is over the 800 budget for one PR, so two slices are recommended (slice 1 about 600 plus specs, slice 2 about 110).

## Testing Strategy

Strict TDD; `[resampler]` cases are named `resample: ...`. Measurement uses double-precision least-squares sine fits on the steady-state region (H output frames trimmed at each end), with no FFT:
- Ripple: fitted amplitude within 0.1 dB, 100 Hz to 0.9 fc (96k to 44.1k, 44.1k to 96k). Fitted phase matches the analytic phase within 1e-4 rad (no time shift). An impulse at an integer ratio lands at `i*dst/src`.
- Stopband (down): tones at 1.1, 1.3, 1.6 and 1.9 x `dst/2`. Output RMS is at most -100 dB. Images (up): the residual after fitting the 10 kHz tone is at most -100 dB.
- Length table, bit-exact passthrough, finite edge inputs, deterministic across `chunkFrames` 65536 and 777, abort on the 3rd poll (at most 2 chunks rendered, no channels), invalid arguments, identical stereo channels.
- `[.][perf]` "resample: 300 s stereo 96k to 48k within 3 s" is hidden, so `catch_discover_tests` and CI skip it. Run it manually in Release.
- Plugin: manual (R-SS-15, R-PS-8).

## Threat Matrix

N/A: no routing, shell, subprocess, VCS/PR automation, executable-file classification or process-integration boundary.

## Migration / Rollout

No migration is needed. State is unchanged; old sessions reload at the host rate.

## Open Questions

- [x] R-RS-3 scenario "0.55 x src" (52.8 kHz) was above the 96k input Nyquist. Fixed in the spec delta to 0.275 x src (26.4 kHz = 1.1 x cutoff). Tests use the design tones.
- [ ] Extreme downsampling (384k to 8k, about 3500 taps) may take several seconds. It can be aborted and is outside the budget.
