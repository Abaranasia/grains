# Design: Plugin Target (silent VST3 + Standalone shell)

## Technical Approach

Opt-in JUCE 9.0.0 plugin (`GrainsPlugin`, VST3 + Standalone) that links `grains_core`. All parameter logic is a header-only, JUCE-free layer inside `grains_core` (unit-tested, strict TDD). The JUCE glue is thin: it builds the APVTS from that table, reads raw atomics, and calls `GranularEngine`. No sample source exists, so output is silence (`process()` zero-fills when `source_ == nullptr`).

## Architecture Decisions

| Topic | Choice | Rejected | Rationale |
|---|---|---|---|
| JUCE import | `GRAINS_JUCE_DIR` PATH cache (default `${CMAKE_SOURCE_DIR}/../../Projucer`). `FATAL_ERROR` if `${GRAINS_JUCE_DIR}/CMakeLists.txt` is missing. `enable_language(C)` and `add_subdirectory(${GRAINS_JUCE_DIR} ${CMAKE_BINARY_DIR}/juce EXCLUDE_FROM_ALL)` only when ON | FetchContent, `find_package(JUCE)` | Matches the proposal. JUCE's CMake fails unless C is enabled; enabling it only in the ON branch keeps the default build CXX-only and JUCE-free |
| Param layer location | `Source/core/include/grains/ParamSpec.h`, header-only, in `grains_core` | Separate lib; inside `Source/plugin` | Reuses the existing test executable; no new target. JUCE-freedom of the header (R-PM-4) is enforced by convention and review: `grains_assert_juce_free` checks link dependencies only, not includes |
| Skew math | Same formula as `juce::NormalisableRange` (`plain = min + (max-min)·exp(ln(n)/skew)`, `n = ((v-min)/(max-min))^skew`) | Own curve | Table conversions then equal what the host sees |
| Warnings on plugin | `juce::juce_recommended_{config,lto,warning}_flags`, not `grains_set_warnings` | `/WX` / `-Werror` | JUCE module sources compile inside the plugin target; `/WX` on third-party code is brittle |
| JuceHeader | Not generated; include module headers directly | `juce_generate_juce_header` | Less generated code, explicit dependencies |
| Mono output | Preallocated scratch R buffer, sized in `prepareToPlay`; chunked loop if the host block is larger | Allocate in `processBlock` | RT-safe |
| Editor | `juce::GenericAudioProcessorEditor` | Custom UI | Out of scope |

## Data Flow

    Host/automation -> APVTS (message+audio thread) -> std::atomic<float>* [9]
         processBlock: load relaxed -> plain[9] -> params::toEngineParams -> engine.setParams
                       -> engine.process(L, R|scratch, n) -> zeros (no source)

## Interfaces / Contracts

```cpp
namespace grains::params {
enum class Id : int { GrainSize, Density, Position, PositionJitter, Pitch,
                      PitchJitter, TimingJitter, PanSpread, Gain, Count };
inline constexpr int kCount = static_cast<int>(Id::Count);
struct Spec { const char* id; const char* name; const char* unit;
              float min, max, def, skew; };
inline constexpr std::array<Spec, kCount> kSpecs{ /* built from grains::limits */ };
float toNormalised(const Spec&, float plain) noexcept;   // clamp to [min,max]; NaN -> def
float fromNormalised(const Spec&, float norm) noexcept;  // clamp to [0,1]; NaN -> def
EngineParams toEngineParams(const std::array<float, kCount>& plain) noexcept; // result .clamped()
}
```

Table. IDs are stable and saved in state, so they must never be renamed. APVTS `ParameterID{id, 1}`.

| id | range | default | skew |
|---|---|---|---|
| grainSize (ms) | kSizeMsMin..kSizeMsMax | kSizeMsDefault | 0.3 |
| density (gr/s) | 0..kDensityMax (0 = off) | kDensityDefault | 0.3 |
| position | 0..1 | 0.5 | 1 |
| positionJitter, timingJitter, panSpread | 0..1 | 0 | 1 |
| pitch (st) | ±kPitchSemitonesMax | 0 | 1 |
| pitchJitter (st) | 0..kPitchJitterSemitonesMax | 0 | 1 |
| gain | 0..kGainMax | kGainDefault | 1 |

Defaults must equal `EngineParams{}`. A test enforces this.

Density dead zone: the range stays 0..kDensityMax, but `toEngineParams` (via `limits::sanitizeDensity` / `clamped()`) snaps values in (0, kDensityMin) up to kDensityMin. With skew 0.3 that zone is the bottom ~6% of the knob (normalised (0.1/1000)^0.3 ≈ 0.063). The param-layer tests must expect this snap; only an exact 0 means off.

## PluginProcessor

- Buses: one stereo output, no input. `isBusesLayoutSupported` accepts mono or stereo output only.
- MIDI: `IS_SYNTH TRUE` and `NEEDS_MIDI_INPUT TRUE` are kept, because hosts route MIDI to instruments and the next change adds note handling. In this change MIDI is accepted and ignored: `processBlock` clears the MIDI buffer.
- Constructor: the APVTS layout is a loop over `kSpecs` (`AudioParameterFloat`, `NormalisableRange(min,max,0,skew)`). Cache the 9 `getRawParameterValue` pointers.
- `prepareToPlay`: `engine.prepare(sampleRate, fixedSeed)`. `GranularEngine::prepare` takes no block size; the maximum block size only sizes the mono scratch buffer. `releaseResources` does nothing.
- `processBlock`: `ScopedNoDenormals`, clear the MIDI buffer, map, `setParams`, then `process`. No locks, no allocation, no logging.
- State: `apvts.copyState()` to XML to binary. Restore checks the tag name before `replaceState`.
- Metadata (placeholders): `COMPANY_NAME "Grains"`, `PLUGIN_MANUFACTURER_CODE Grns`, `PLUGIN_CODE Grn1`, `PRODUCT_NAME "Grains"`, `BUNDLE_ID com.grains.grains`, `IS_SYNTH TRUE`, `COPY_PLUGIN_AFTER_BUILD FALSE`. Definitions: `JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0`.

## File Changes

| File | Action | ~Lines |
|---|---|---|
| `CMakeLists.txt` | Modify: replace the guard, add the JUCE branch | 20 |
| `Source/plugin/CMakeLists.txt` | Create: `juce_add_plugin`, sources, links | 35 |
| `Source/core/include/grains/ParamSpec.h` | Create | 110 |
| `tests/test_param_spec.cpp` | Create | 150 |
| `tests/CMakeLists.txt` | Modify: add the test source | 1 |
| `Source/plugin/PluginProcessor.{h,cpp}` | Create | 170 |
| `Source/plugin/PluginEditor` | None. `createEditor` returns the generic editor inline | 0 |
| `docs`/README build note | Create/Modify | 20 |

Budget: the <800-line review budget counts code + tests + CMake (reviewer burden): about 490 lines. Docs are reported separately: the build note (~20) plus OpenSpec artifacts (proposal, explore, design, delta specs, ~350). Unchanged: CI, Projucer files, `Source/core/CMakeLists.txt`.

## Testing Strategy

| Layer | What | Approach |
|---|---|---|
| Unit (strict TDD, RED first) | 9 unique IDs; ranges and defaults from `limits`; defaults == `EngineParams{}`; normalised/plain round-trip (tolerance 1e-4) incl. endpoints; skew midpoint matches the JUCE formula; NaN and out-of-range clamp; `toEngineParams` field mapping and clamping, incl. density in (0, kDensityMin) snapping to kDensityMin and exact 0 staying 0 | Catch2 in `grains_tests` |
| Regression | Existing 84/84 tests | Default configure, plugin OFF |
| Manual | Configure with `-DGRAINS_BUILD_PLUGIN=ON` (and once with a bad `GRAINS_JUCE_DIR` to see the error); build VST3 and Standalone; Standalone shows 9 parameters, outputs silence with no dropouts; automation and state save/restore round-trip in a host; mono layout loads | Recorded in the verify report |

## Threat Matrix

N/A: no routing, shell, subprocess, VCS/PR automation, executable-file classification, or process-integration boundary.

## Migration / Rollout

No migration. The option is OFF by default. Rollback: revert the PR. Core API is only extended (a new header).

## Open Questions

- [ ] Final plugin identity and codes before distribution (placeholders are fine for local builds).
- [ ] Licensing (AGPLv3 vs commercial) is the user's decision; it does not block the build.
- [ ] `GRAINS_SANITIZE=ON` together with the plugin is untested and unsupported in this change.
