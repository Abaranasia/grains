# Proposal: Load-time resampling (P3)

## Intent
When the file's sample rate differs from the host's, the engine fixes the rate with a per-grain `LinearInterpolator` (`srcSr / sampleRate_`). That fixes pitch, but linear interpolation has no anti-alias filter. Downsampling (for example, a 96k file in a 44.1k host) aliases. Goal: anti-aliasing **quality**. Resample each file once on the loader thread with a band-limited filter, so the engine runs at a base rate of 1.

## Scope
### In Scope
- New JUCE-free windowed-sinc resampler in `grains_core`. Cutoff `min(1, dst/src)`, abortable per chunk, deterministic.
- Bit-exact passthrough when file rate == target rate.
- The loader takes a target rate from `prepareToPlay`. The published `SampleBuffer::sampleRate()` equals the target.
- Host rate change: re-request the load through the existing generation gate. No-op at an unchanged rate. Re-decode from disk. If the file is gone, keep the current buffer.
- Caps: 300 s at the file rate before resampling (R-SS-5 unchanged). Output capped at `floor(300 * target)` frames, which is at most 115.2M frames at 384k, so INT_MAX-safe.
- Quality: stopband at least 100 dB, passband ripple at most 0.1 dB up to 0.9 x cutoff.
- Load budget: 300 s stereo 96k to 48k in at most 3 s (Release, reference machine, Catch2 `[perf]`).

### Out of Scope
- Pitch-shift aliasing during playback. A future P-item (runtime interpolator or oversampling).
- Any `processBlock` change. Removing the engine's `srcSr` compensation.
- Keeping the original decode in RAM.

## Capabilities
### New Capabilities
- `sample-resampling`: core resampler contract (quality, length, passthrough, abort, determinism).
### Modified Capabilities
- `sample-source`: R-SS-5 drops "MUST NOT resample" and adds the output cap. The loader gains a target rate and re-request on rate change. Out-of-scope list updated.
- `grain-rendering`: R-GR-7 clarifies that the `srcSr / sampleRate_` fallback is kept for mismatched buffers. Grain length still depends only on the engine rate.
- `plugin-shell`: `prepareToPlay` notifies the loader, with no source allocation on the audio thread. Out-of-scope list updated.

## Approach
- Kaiser-windowed sinc (filter shape and length chosen in the design phase), run offline on the loader thread. It checks `isCurrent()` between chunks.
- `prepareToPlay` stores the rate atomically. If the rate changed and a source exists, it calls `loader_.request(Reload)`.
- If restore runs before prepare, there is no target rate yet: publish at the file rate (passthrough). The first prepare then reloads if the rate differs.
- Tradeoff (user may overrule): re-decoding means no 2x RAM, but the file must still exist and the reload hits disk I/O. Keeping the original in RAM costs about 2x memory (around 460 MB worst case) but survives a moved file.

## Affected Areas
| Area | Impact |
|---|---|
| `Source/core/src/Resampler.*` (new) | New |
| `Source/core/src/SampleBuilder.cpp` | Modified |
| `Source/plugin/SampleLoader.{h,cpp}` | Modified |
| `Source/plugin/PluginProcessor.cpp` (`prepareToPlay` only) | Modified |
| `tests/test_resampler.cpp` (new) | New |
| specs: sample-source, grain-rendering, plugin-shell | Delta |

## Risks
| Risk | Likelihood | Mitigation |
|---|---|---|
| `prepareToPlay` races an in-flight load and a stale rate publishes | Med | Generation gate. Target rate captured per request. |
| Restore runs before prepare | High | Passthrough, then reload on the first prepare. |
| Repeated `prepareToPlay` at the same rate | High | Compare the rate and skip. |
| Peak memory: decode buffer + output | Med | Caps above. Free the decode buffer immediately after resampling. |
| File gone on reload | Low | Keep the current buffer. The engine compensates. |
| User expects pitch aliasing fixed | Med | Documented non-goal. |

## Rollback Plan
Revert the slice commits. The engine fallback means playback stays correct without the resampler.

## Success Criteria
- [ ] Catch2: stopband at least 100 dB, ripple at most 0.1 dB, length `round(n * dst / src)`, passthrough bit-exact, finite output, deterministic, abort honored.
- [ ] `[perf]` budget met.
- [ ] Manual: a 96k file in a 44.1k host gives a published rate of 44.1k. A rate change reloads with no dropout. The same rate gives no reload.

## Review Workload Forecast
About 790 lines (resampler 200, tests 250, loader/builder 150, processor 40, specs 150). That is close to the 800 budget. **Split recommended:** Slice 1 is the core resampler, tests and the `sample-resampling` + R-SS-5 deltas (about 500 lines). Slice 2 is the loader/processor wiring + the remaining deltas (about 290 lines).

## Proposal question round (assumptions for user review)
1. Is passthrough before the first `prepareToPlay` OK, instead of guessing 48k?
2. Re-decode from disk vs. keep the original in RAM. Re-decode is the current choice.
3. Is 3 s acceptable as the load budget for 5-minute 96k files?
