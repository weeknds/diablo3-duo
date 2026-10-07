# Live preview checkpoint — 2026-10-07

`0.2.0-dev` provides Character, Equipment, Map and Skills views on the Thor's
lower display. It preserves the approved warm-black/gold journal design with no
character picture or repeated health/resource HUD.

The current downloadable preview is
[v0.2.0-dev](https://github.com/weeknds/diablo3-duo/releases/tag/v0.2.0-dev).
The exact archive/module hashes and dated observations are in
[validation](validation.md). The root builder and `project.json` still describe
the separate static `0.1.1-dev` package.

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

## Limits and future work

This is an experimental preview, not broad game compatibility. Pylons/exits,
Greater Rift progress/timing, full item rolls/comparison, sheet damage,
toughness/recovery, critical damage, actual rune names, passives and buff timers
are not implemented. Terrain can omit unsupported scenes and dynamic obstacles.

The owner requested a quick completion without repeated testing. The current
pass ends with focused correctness checks and packaging, not another extended
benchmark/lifecycle run. Historical 0.1.2 device evidence remains separate.
Broader build/class/value/travel/language coverage and sustained performance are
future validation work, not claims about this release.

Any future reader work should add one evidence-backed field at a time. Unknown
or rejected data stays unavailable. Keep all guest access read-only, private
game material out of public files, and normal saves/settings intact.
