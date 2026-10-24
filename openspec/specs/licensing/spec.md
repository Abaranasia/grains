# Licensing Specification

## Purpose

Grains is published under the GNU Affero General Public License v3.0 or later, with the license text, per-file identification and README notice needed to make that clear and compatible with JUCE's AGPLv3 option.

Test-layer legend: **[CI]** = verified by automated CI. **[Manual]** = manual verification or inspection.

## Requirements

### R-LIC-1 License file
The repository root MUST contain a file `LICENSE` holding the verbatim, unmodified GNU AGPLv3 text as published at https://www.gnu.org/licenses/agpl-3.0.txt. The text MUST NOT be retyped, reformatted or abridged.

#### Scenario: LICENSE matches upstream [Manual]
- Given the `LICENSE` file and a fresh download of `agpl-3.0.txt` from gnu.org
- When their SHA256 digests are compared
- Then the digests are identical

### R-LIC-2 SPDX identifier
Every first-party source file, CMake file, test file and the README MUST carry the line `SPDX-License-Identifier: AGPL-3.0-or-later`, expressed in the comment syntax of the file type (`//` for C++, `#` for CMake files, an HTML comment for Markdown). First-party files are those under `Source/`, `tests/`, `cmake/`, plus `CMakeLists.txt` and `README.md`. The line MUST appear within the first lines of the file. Vendored or generated content (`LICENSE`, `Builds/`, `JuceLibraryCode/`, `Grains.jucer`, fetched dependencies) MUST NOT be tagged or modified.

#### Scenario: All first-party files tagged [Manual]
- Given the set of first-party files defined above
- When each is searched for the exact line `SPDX-License-Identifier: AGPL-3.0-or-later`
- Then every file contains it exactly once

#### Scenario: Untouched third-party and Projucer files [Manual]
- Given the diff against the base commit
- When `Grains.jucer`, `Builds/`, `JuceLibraryCode/` are inspected
- Then there are no modifications

#### Scenario: Tags do not affect the build [CI]
- Given the SPDX lines are present
- When the default configure, build and `ctest` run
- Then the suite is green

### R-LIC-3 README licensing section
`README.md` MUST contain a licensing section that states the project is licensed under AGPL-3.0-or-later and points to `LICENSE`. It MUST state that JUCE is dual-licensed (AGPLv3 / commercial) and that Grains uses JUCE under its AGPLv3 option, so combined distribution is compatible. It MUST document the JUCE sourcing order (local `GRAINS_JUCE_DIR`, then the pinned fetch controlled by `GRAINS_JUCE_FETCH`).

#### Scenario: README states license and JUCE compatibility [Manual]
- Given `README.md`
- When its licensing section is read
- Then it names AGPL-3.0-or-later, links `LICENSE`, and explains that JUCE is used under its AGPLv3 option

#### Scenario: README documents JUCE sourcing [Manual]
- Given `README.md`
- When the build section is read
- Then it describes `GRAINS_JUCE_DIR`, the fetch fallback and `GRAINS_JUCE_FETCH=OFF`
