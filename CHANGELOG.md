# Changelog

## 0.2.1-dev — map motion correction

- Separate cached terrain from the moving player marker and camera. Late image
  workers retain immutable current-world generations instead of blanking them.
- Read the same client presentation XY used by the game's minimap every sample.
  Match its ordinary-world orientation and use a fixed player-centered scale.
- Add native touch panning and pinch zoom, with recentering after zoom controls
  and world changes. Keep the local pin independent of terrain rendering.
- Render finer terrain outlines at twice the previous resolution; remove the
  large rectangular exploration fill when terrain is available.


## 0.2.0-dev — expanded live preview — 2026-10-07

- Added class, Paragon, primary attributes, critical chance and resource cost reduction.
- Added thirteen equipment slots with read-only base-name inspection.
- Added exploration coverage, supported numeric terrain, a player marker, zoom, recentering and one local pin. Pins clear on world changes.
- Expanded the portrait-free gold/black interface to Character, Equipment, Skills and Map. Added original equipment icons and a larger bitmap-font atlas.
- Added bounded equipment/map readers, repeated ownership/world checks, thread-safe map snapshots and synthetic failure/renderer tests.
- Kept unsupported values unavailable. Full item rolls/comparison, affixed names, rune names, pylons/exits and Greater Rift timing/progress remain unimplemented.

This is an experimental preview. See [validation](docs/validation.md) for the
exact artifact, observed hardware behavior and remaining coverage. Older release
measurements below do not describe the new readers.

## Validation follow-up — 2026-10-07

No package or code change. On the published `0.1.2-rc.1` binary, a level-2
softcore Barbarian death and town resurrection were observed. The character
sheet stayed visible during death; level and all four statistics matched the
game menus after resurrection. Shield removal/restoration matched armor
31 → 22 → 31. The shield and original hidden-overlay setting were restored.
Broader loading, process termination, nonzero cooldown/movement and sustained
combat measurements remain open. The release-time record below is unchanged.

## 0.1.2-rc.1 — live candidate — 2026-10-07

- Connected the portrait-free native design to the exact-build, read-only character reader, with Character and Skills pages.
- Added bindings for level, attacks per second, cooldown reduction, armor, movement bonus and six equipped skill names. Missing or rejected data stays unavailable; actual rune names are not implemented.
- Removed the Map destination from this candidate. Buff timers, passives, item comparison and equipment actions remain outside its scope; no health/resource HUD or character imagery is included.
- Recovered all 45 files of the private reader source/assets and reproduced the unchanged native module with NDK r28c against its recorded binary hash.
- Added host checks for live UI bindings, numeric layout and page navigation alongside the native sanitizer suites.
- Kept a separate live-candidate build and archive; the original static builder and its support guards remain intact.

Installed through Add-ons on the Thor; package/module identity, exact-build
loading, title-screen rendering with unavailable data and both page actions
were observed. Barbarian level 2, attacks per second 1.20, cooldown reduction
0.00%, armor 31, movement +0%, Bash and Hammer of the Ancients/no-rune matched
the game menus. Hammer clear/restore correctly updated slot 2. Axe
removal/restoration matched attacks per second 1.20 → 1.00 → 1.20. A different
Wizard matched level 1, attacks per second 1.20, armor 16, zero cooldown/movement
and Magic Missile/no-rune; skill clear/restore updated the row correctly. Physical
controls and lower-page navigation worked in the observed menu states. Normal
quit cleared all value rows; Barbarian values returned after a second fresh
enabled launch. Nonzero stats, broader input coverage
and remaining lifecycle checks are pending. A 108.47-second screen-off test left
Eden paused on wake; Resume restored Wizard values and Magic Missile. A New
Tristram/Slaughtered Calf Inn round trip retained correct Barbarian values at the
destinations; the brief loading interval was not captured. Death and broader
travel/loading remain unverified. Short
stationary enabled/disabled presentation checks showed no obvious large cadence
regression; zero overhead and sustained-combat performance are not established. This is not a verified working release. Earlier private prototype
observations do not establish compatibility for this candidate.

## 0.1.1-dev — native design preview — 2026-10-07

- Replaced the single status page with Character, Combat and Map pages on the native 1240 × 1080 canvas.
- Added antique gold styling, original navigation icons, Cinzel headings and the OFL-derived Duo Sans bitmap font. No character imagery is used.
- Organized Character around a full-width statistics sheet and six numbered skills; widened Combat skill/rune rows and simplified the Map unavailable state.
- Added bottom tabs and a skills link whose only actions select companion pages. Controller navigation remains disabled.
- Kept every installed game value unavailable, with a visible design-preview badge and disconnected-data message. Map, equipment, buff timers and live integration remain unimplemented.
- Added exact asset hashes and desktop renders. Sample and long-value stress modes appear only in labeled offline PNGs.

This version has not been installed or validated on the Thor. The historical
`0.1.0-dev` capture and private reader experiments do not validate the new layout.
No supported builds or live-functionality flags were enabled.

## 0.1.0-dev — development preview

This version is a static status page, not a working gameplay companion.

- Original 1240 × 1080 layout with a visible development and unavailable-data message.
- Reproducible `.dsmod.zip` packaging using the pinned upstream builder.
- Local NSO header inspection and bounded NSP metadata inspection.
- Synthetic tooling tests and package checks in CI.
- English setup, compatibility, contribution, and device-validation documentation.
- Static package installation and lower-screen layout observed on AYN Thor with Eden Duo 1.1.0; original screenshot included.

No supported live-data builds, verified game values, inventory actions,
or measured performance results are claimed.
