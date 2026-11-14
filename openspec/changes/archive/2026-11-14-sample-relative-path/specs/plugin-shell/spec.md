# Plugin Shell Specification (delta)

## MODIFIED Requirements

### R-PS-7 State persistence
`getStateInformation` / `setStateInformation` MUST save and restore the APVTS state, including the loaded sample's reference properties (`samplePath`, `sampleName`, and, when anchored, `sampleAnchor` and `sampleRel`; see `sample-source` R-SS-8), so parameter values and the loaded file survive a host session reload, including after the sample folder or the user account changed; a restored reference MUST be reloaded asynchronously. State saved by the previous format (absolute path only) MUST still restore. Invalid or empty state data MUST be ignored without crashing.

#### Scenario: State round trip [Manual]
- Given parameters set to non-default values and a sample loaded
- When the state is saved and restored (host reload or Standalone state)
- Then all 9 values are restored and the sample reference is restored and reloaded

#### Scenario: Bad state data [Manual]
- Given empty or corrupt state bytes
- When `setStateInformation` is called
- Then parameters and loaded sample are unchanged and there is no crash

#### Scenario: Old-format state still restores [Manual]
- Given state saved with only the absolute `samplePath` property and 9 parameter values
- When `setStateInformation` is called
- Then the 9 values are restored and the sample reloads from the path

#### Scenario: Moved sample folder restores [Manual]
- Given state saved with a sample, then the sample moved to a folder reachable through the stored anchor or a search directory
- When the state is restored
- Then the sample loads and the label reads "Loaded: <name> (relocated)"

#### Scenario: Missing sample label unchanged [Manual]
- Given state whose sample exists at no candidate location
- When the state is restored
- Then the label reads "Missing: <name>" and the 9 parameter values are restored

## Out of scope (explicit)
MIDI note handling and gating, custom parameter UI, waveform display, recent files, embedded audio in state, load-time resampling, AU, presets, lookahead limiting and any user-adjustable ceiling or drive, plugin CI job, FetchContent JUCE fallback.

## Assumptions (pending user confirmation)
Placeholder metadata (company "Grains", 4-char codes); output is bounded to |y| < 1.0 by a fixed soft limiter in the engine with no user control, and the drone is always on once a sample is loaded; the sample reference (absolute path, name, and an optional anchor-relative path) is stored in state, and the absolute path is still written; JUCE licensing (AGPLv3 vs commercial) is the user's distribution decision.
