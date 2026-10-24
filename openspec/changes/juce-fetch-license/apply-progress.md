# Apply progress: juce-fetch-license (Group 1, tasks 1.1-1.16; 1.17 commit pending, orchestrator)

## Recorded values
- JUCE tarball SHA256 (f8f88641...1597.tar.gz, 23680384 bytes): `0bef95e4d4e6e8e1aaa98dfa9aae7529235152e140a7710525bc38b03f8c93a9`
- actions/cache: tag `v5.1.0` -> commit `caa296126883cff596d87d8935842f9db880ef25` (lightweight tag, no `^{}` entry; `v5` floating tag points to the same SHA)

## RED (1.1)
`cmake -S . -B build/red -DGRAINS_BUILD_PLUGIN=ON -DGRAINS_JUCE_DIR=<empty>`: CMake Error at CMakeLists.txt:32 "GRAINS_JUCE_DIR='...' does not contain a JUCE CMakeLists.txt. Set -DGRAINS_JUCE_DIR=<path to a JUCE checkout> or leave GRAINS_BUILD_PLUGIN=OFF."

## GREEN results
| Check | Result |
|---|---|
| G1 (1.8) | build/g1: fetching STATUS shown, hash in download stamp, configure 0, `grains_plugin` Debug --parallel build exit 0 |
| G2 (1.9) | fetch OFF: FATAL_ERROR naming path, GRAINS_JUCE_FETCH=OFF and GRAINS_JUCE_DIR; `_deps` has only catch2 (from tests/), no juce-* |
| G3 (1.10) | wrong hash (0->1 first digit): "Hash mismatch, removing..." expected 1bef.. actual 0bef.., configure failed, juce-src empty; edit reverted, `cmp` identical to good copy |
| G4 (1.11) | local ../../Projucer: "using local JUCE" STATUS, configure OK; `_deps` only catch2 |
| G5 (1.12) | default OFF, -Werror=dev -Werror=deprecated, Debug build, ctest 122/122 |
| G6 (1.13) | fetch configure with -Werror=dev -Werror=deprecated: exit 0, no warnings at all (only note: CMake 4.4 prints "The error=dev option is deprecated. Use -Werror=author instead.") |
| G8 (1.14) | in a fresh dir Catch2 (tests/) fails first under FETCHCONTENT_FULLY_DISCONNECTED; so primed build/g8 with a plugin-OFF configure, then reconfigured with plugin ON + DISCONNECTED: guard FATAL_ERROR "fetched JUCE not available at '.../_deps/juce-src'" |
| 1.15 | `git diff --stat -- Grains.jucer Builds JuceLibraryCode` empty |
| 1.16 | ci.yml: 3 jobs, all `uses:` SHA-pinned, permissions contents: read, concurrency intact, jobs 1-2 GRAINS_BUILD_PLUGIN=OFF |

## Notes
- Job id for the new job: `windows-msvc-plugin`, name "Windows MSVC plugin (informational)".
- CMakeLists.txt, README.md, Source/plugin/CMakeLists.txt keep CRLF; ci.yml is LF.
