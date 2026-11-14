# SPDX-License-Identifier: AGPL-3.0-or-later
# Single source of the pinned JUCE fetch (used when GRAINS_JUCE_DIR has no JUCE). The CI JUCE cache
# key hashes only this file, so changing the pin here invalidates the cached download.
set(GRAINS_JUCE_VERSION "9.0.0")
set(GRAINS_JUCE_SHA "f8f8864172464b9adf9eba6101e1f784838d1597")
set(GRAINS_JUCE_SHA256 "0bef95e4d4e6e8e1aaa98dfa9aae7529235152e140a7710525bc38b03f8c93a9")
