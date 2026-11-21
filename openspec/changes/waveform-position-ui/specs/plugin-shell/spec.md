# Plugin Shell Specification (delta)

## MODIFIED Requirements

### R-PS-6 Editor
The plugin MUST provide a custom editor that embeds `juce::GenericAudioProcessorEditor`, showing all 9 parameters, together with the sample-loader UI defined in `sample-source` (Load button, drag-and-drop, file/status label) and the waveform/position view defined in `waveform-display` (`WaveformView` above the generic editor). The editor window MUST have a fixed size. MIDI note handling and any other custom parameter UI MUST NOT be provided. The set of parameters MUST remain exactly the 9 of R-PS-3; the waveform view only drives the existing `position` parameter and MUST NOT add parameters.

(Previously: editor embedded the generic editor and loader UI only; no waveform view.)

#### Scenario: Editor lists 9 controls, load UI and waveform view [Manual]
- Given the Standalone launched
- When the editor window opens
- Then it shows the waveform view above 9 parameter controls labelled with the table display names, a Load button, and a file/status label

#### Scenario: Parameter count unchanged [Manual]
- Given the plugin with the waveform view
- When its parameter list is enumerated
- Then there are still exactly 9 parameters with the table IDs (R-PS-3)

#### Scenario: Fixed window size [Manual]
- Given the editor open
- When the user attempts to resize it
- Then the size does not change

## Out of scope (explicit) (MODIFIED)
"waveform display" is REMOVED from the plugin-shell out-of-scope list. The remaining items are unchanged: MIDI note handling and gating, custom parameter UI (beyond the waveform/position view), recent files, embedded audio in state, runtime (pitch-shift) aliasing fixes, interpolator or oversampling changes in `processBlock`, removal of the engine's source-rate compensation, AU, presets, lookahead limiting and any user-adjustable ceiling or drive, plugin CI job, FetchContent JUCE fallback.
