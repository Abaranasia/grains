# Plugin Shell Specification (delta)

## MODIFIED Requirements

### Test-layer legend (modified)
Test-layer legend: **[Manual]** = manual verification (no automated coverage; JUCE glue). **[Catch2]** = automated.

(Previously: "**[Manual]** = manual verification in verify phase (no automated coverage; JUCE glue).")

### R-PS-2 Placeholder metadata
Plugin metadata MUST use placeholders pending user confirmation: company name "Grains", plugin name "Grains", a 4-character manufacturer code, and a 4-character plugin code (manufacturer code with at least one uppercase letter per JUCE convention). The plugin MUST be an instrument that accepts MIDI input and ignores it (no note, gate, or pitch handling). Final identity (company, codes, bundle ID) is explicitly deferred.

(Previously: "...ignores it in this change (no note, gate, or pitch handling); MIDI handling is a later change. Final identity ... is explicitly deferred." No behaviour change; wording only.)

#### Scenario: Metadata present [Manual]
- Given a built plugin
- When its reported name, manufacturer, and codes are inspected
- Then they match the placeholders and codes are exactly 4 characters

(Unchanged from previous.)
