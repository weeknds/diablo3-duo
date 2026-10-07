# Live preview checkpoint — 2026-10-07

`0.2.1-dev` provides Character, Equipment, Map and Skills views on the Thor's
lower display. It preserves the approved warm-black/gold journal design with no
character picture or repeated health/resource HUD.

The current downloadable preview is
[v0.2.1-dev](https://github.com/weeknds/diablo3-duo/releases/tag/v0.2.1-dev).
The exact archive/module hashes and dated observations are in
[validation](validation.md). The root builder and `project.json` still describe
the separate static `0.1.1-dev` package.

## Prepared while the Thor charges

The local `0.2.2-dev` candidate adds explored quest/location markers, a selectable
target direction, and visible goblin highlights with a short notification.
It keeps terrain and player following independent of marker updates.
[Marker details](map-markers.md) document conservative omissions and read limits.
The new candidate is not installed; the last observed device package remains
0.2.1. No device connection was requested or polled during this pass.
Next device work is installation and one focused check of marker visibility,
selection, travel clearing and noticeable overhead. Do not restart a broad audit.

## Delivered

- Class, level, Paragon, four primary attributes and six combat statistics.
- Thirteen equipment slots with occupied/empty state and tap-to-inspect localized
  base item names. Main-hand/off-hand labels match the observed game Inventory.
- Live explored coverage, supported terrain grids, player marker, zoom, recenter
  and one local pin. A world change clears cached map state and the pin.
- Six equipped skill names with no-rune/unavailable-rune states.
- Actual Thor captures and an exact-build, read-only production module.

The observed character is the DuoTest level-2 Barbarian in New Tristram with axe
and shield restored. The normal profile and game-owned equipment are preserved.

## Map correction

The owner reported flicker and incorrect tracking in 0.2.0. The 0.2.1 correction
separates immutable terrain images from the native camera/player marker, uses
client presentation XY and the ordinary game minimap orientation, and follows
at a fixed scale by default. Touch drag/pinch and Center on you control the view.
Terrain outlines are thinner and the default view shows more surroundings.
See the dated movement evidence in validation.md; this is not a new broad
performance or compatibility test.

The final package additionally enables Eden's GPU map compositor. After the
Thor reconnected, it was installed through Eden's installer. Device readback
matched the release manifest and module, including `gpu_composite: true`.
Diablo was relaunched in DuoTest and the map rendered in New Tristram. The game
touch overlay was restored to hidden and the Map page left open. The earlier
motion recording uses the CPU path; no second movement benchmark was run.

## Limits and future work

This is an experimental preview, not broad game compatibility. Shrines/pylons,
Greater Rift progress/timing, full item rolls/comparison, sheet damage,
toughness/recovery, critical damage, actual rune names, passives and buff timers
are not implemented. Terrain can omit unsupported scenes and dynamic obstacles.
Entrance/exit, quest, waypoint and goblin markers are implemented in 0.2.2 but
remain unverified on hardware. Location labels currently identify kinds, not names.

The owner requested a quick completion without repeated testing. The current
pass ends with focused correctness checks and packaging, not another extended
benchmark/lifecycle run. Historical 0.1.2 device evidence remains separate.
Broader build/class/value/travel/language coverage and sustained performance are
future validation work, not claims about this release.

Any future reader work should add one evidence-backed field at a time. Unknown
or rejected data stays unavailable. Keep all guest access read-only, private
game material out of public files, and normal saves/settings intact.
