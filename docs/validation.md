# Validation record — 2026-10-06

## Current local verification

Host: macOS arm64, Python 3.14. No game files are required by the test suite.

- `python3 -m unittest discover -s tests -v`: **37 tests passed** (the original
  11 plus 26 NSP inspector tests).
- `python3 tools/build.py --check`: passed, static development stage only.
- `python3 tools/build.py`: built the development package with the pinned helper.
- Synthetic build test confirms two independent builds have identical bytes.
- ZIP members are exactly `package.json` and `dualscreen/manifest.json`.
  The builder serializes JSON with different whitespace; parsed manifest content
  matches the source. No game data, native module, or runtime reader is present.
- Vendored builder, format document, and licence SHA-256 values match provenance.
- NSP tests cover bounded reads, malformed names/ranges, overlapping entries,
  oversized metadata, DTD/entities, XML structure and field validation, source
  changes, and exclusive output. Fixtures are synthetic. Existing NSO tests
  continue to prove header-only reads and metadata-only optional ROM handling.
- The NSP inspector ran against the owner's local container. It returned only
  directory/advisory metadata; source size and modification/change timestamps
  were unchanged. The report stays under ignored `private/reports/`.

Development archive SHA-256:

```text
feb78f3399c83c71dc770480906e5792e26d3596892f663ae7240625ab05218d
```

The initial handoff also recorded 11 passing tests on Linux x86-64. That is
historical evidence, not a Linux integration or current CI result.

## Device and integration status

- Authorized ADB inspection identified an AYN Thor, Android 13/API 33, arm64.
- Installed Eden Duo package `dev.igawa6.edenduo`: version 1.1.0,
  versionCode 33940730. It is not debuggable. Other Eden apps were not modified.
- After the owner's update installation, Eden Duo Info, enabled Add-ons and the
  game's own title screen showed version `2.7.7.92380`. Eden Duo Info confirmed
  title ID `01001B300B9BE000`.
- Effective updated ExeFS was captured privately through Eden's built-in dump
  route. The copied `main` hash matched the device. Its full NSO build ID is
  `2607A74F5DF7754CC0357B5DF7E496931355D8CA000000000000000000000000`;
  runtime build key `2607A74F5DF7754C`. These identify the research build only.
- Original settings and accounts were backed up privately. A separate `DuoTest`
  profile was created and selected at index 1. The owner confirmed a fresh
  non-seasonal Barbarian reached gameplay; a device screenshot also recorded
  that session. The test save is backed up privately.
- The temporary `dump_exefs` setting was restored to `false`. The exact two-line
  restoration and configuration hash were verified, preserving the test profile
  and other owner settings.
- Static package `0.1.0-dev` was installed through Add-ons. Its lower screen
  rendered the development label, unavailable-data message and placeholder
  readouts while the game was at its title screen. The
  [original lower-screen screenshot](images/development-preview-thor.png)
  records the observed layout without game artwork or personal identifiers.
- Gameplay input routing with the companion active, loading transitions and
  repeat-launch behavior remain unverified.
- Live values, pointers, build compatibility and inventory interactions remain
  unavailable. No live-functionality check is complete.
- Performance is unmeasured. A frame-rate counter in a gameplay screenshot is
  not a benchmark or an overhead measurement.
- Android NDK r28c is installed and verified under the ignored project-private
  toolchain directory. No global toolchain configuration was changed. A private
  diagnostic module passed local review and sanitizer tests, then loaded on
  this Thor. It accepted the exact full build and completed its single mapped
  four-byte read. Copied bytes were discarded. This tests the host read API,
  not a game field or sustained performance; private evidence is retained.

Independent review found and resolved malformed-encoding and partial-report-write
errors in the NSP tool. The final recheck found no remaining publication blocker
in the reviewed code/CI, and separately exercised flush/fsync/close failure paths.

The production package remains static, with no supported builds and both
`live_data_available` and `verified_on_thor` set to `false`. The latter is not
promoted by a successful installation or screenshot. The first live milestone
still requires one correct game value through two fresh launches and a different
copied character/save, checked against the game's own menu.
