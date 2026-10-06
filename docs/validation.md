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

GitHub CI passed on Ubuntu with Python 3.10 and 3.14, and macOS with Python 3.14
at commit `c96a0575e177c1cc54062a3705e1360aec18cd88`
([run](https://github.com/weeknds/diablo3-duo/actions/runs/37498853820)).
This validates tooling and static packaging, not game integration.

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
- A separate private research module displayed candidate character level **1**,
  matching the game's Inventory menu for the test Barbarian through two fresh
  game launches. Startup, quitting gameplay and the returned main menu showed
  **UNAVAILABLE**. Captures and the tested artifact hashes are retained privately.
  The owner then played the Barbarian to level 2 using the Thor's physical
  controls; both Inventory and the companion showed 2. A fresh normal,
  non-seasonal Wizard subsequently matched at level 1. Hero selection also
  showed unavailable. These observations establish the first level milestone,
  with further lifecycle and field validation still required before release.
- A five-second screen-off paused Eden and left the lower display black. After
  wake and the app's Resume action, the Wizard's level 1 matched again. A tap and
  swipe directed to the lower display left the game's Inventory state unchanged.
  These are bounded observations, not complete input or long-sleep coverage.
- No live reader is included in the public package. Health, resource, skills and
  inventory interactions remain unavailable.
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
has the two-launch, controlled level change and second-character evidence.
Death, travel and measured performance remain unverified. No claim is made for
other levels, classes, modes or game builds.

## Later private build-details checkpoint

Private research `0.0.8-research` installed and rendered its combined interface
on the same Thor setup. Its font, long skill label and observed unavailable
states fit without clipping in retained captures. This is screenshot-level
visual evidence, not an owner assessment of long-term reading comfort.

- Wizard level 1, Magic Missile with no rune, attack speed 1.20 and armor 16
  matched the menus; values repeated after a fresh guest restart.
- Changing to Barbarian level 2 showed Bash and Hammer of the Ancients with no
  runes, attack speed 1.20 and armor 31. Skill names were independently correlated
  in an earlier probe by clearing and restoring each slot through normal game
  controls. Unknown skill and rune names remain unavailable.
- The unchanged stat-reader core was tested by removing/restoring the Barbarian's
  weapon (attack speed 1.20 → 1.00 → 1.20) and shield (armor 31 → 22 → 31).
  All three states matched Character Details. Equipment was restored afterward.
- Movement bonus matched at +0% on both characters; no nonzero change was tested.
  Cooldown reduction stayed unavailable because the cache key was absent; the
  current reader does not infer a numeric default.
- Startup and normal quit cleared every field. After approximately 90 seconds
  asleep, Eden remained paused with the lower display black; its Resume action
  restored the Wizard values. This is a bounded suspend test, not overnight or
  process-death coverage.
- A tap and swipe directed to the lower display left the upper Skills selection
  and assignment unchanged. Full physical-controller coverage is still pending.

Synthetic sanitizer tests and independent review covered exact build/ABI gates,
stale-state clearing, bounded cache reads, field-specific failures, shared
identity changes and the font interface. An independent Android rebuild matched
the candidate binary. The maximum per sample is 198 reads and 2,054 requested
bytes; this is a work bound, not a measured performance result.

The combined prototype and its private evidence remain outside the public
package. Three early skill names and no populated runes are not broad skill
support. Death, travel, measured overhead and further useful-field coverage
remain release work. Production metadata continues to declare no supported
builds and no live reader.
