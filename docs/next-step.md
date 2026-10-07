# Live candidate checkpoint — 2026-10-07

`0.1.2-rc.1` combines the portrait-free native design with the recovered read-only reader. It has two pages, **Character** and **Skills**, for level, attacks per second, cooldown reduction, armor, movement bonus and six equipped skill names. Actual rune names, passives, maps, buffs and item comparison are outside this candidate. No health/resource HUD or character imagery is included.

The root builder and `project.json` still describe static `0.1.1-dev`. The candidate builds separately with `python3 native/build.py --ndk /path/to/android-ndk-r28c`, producing `dist/live/01001B300B9BE000.dsmod.zip`. Its **DEVICE TEST** badge remains until final device evidence has been reviewed. No supported build or working-release claim is enabled.

## Completed

- Recovered all 45 files of private `0.0.10-research` source/assets with matching recorded hashes. The frozen recovery remains separate; complete historical private evidence was not recovered.
- Rebuilt the unchanged reader using NDK r28c to SHA-256 `9736a2ed41d0f302f5883e3e508238c9022b216816d70fc63b1633799ab53644`. Passed 76 public Python tests, five native sanitizer suites and 15 native UI/package Python tests. Two NDK builds produced identical candidate ZIPs; [validation](validation.md) records the hash. Public source at `f93f3e7` passed all CI jobs ([run](https://github.com/weeknds/diablo3-duo/actions/runs/37584002649)).
- Reconfirmed Android 13, Eden Duo 1.1.0 (versionCode 33940730) and enabled game update `2.7.7.92380`. Installed through Add-ons, matched package/module identity and exact-build loading, and captured both 1240 × 1080 live pages. Startup and normal quit cleared all value rows.
- Matched Barbarian level 2, attacks per second 1.20, cooldown reduction 0.00%, armor 31, movement +0%, Bash/no-rune and Hammer of the Ancients/no-rune. Clearing/restoring Hammer changed slot 2 to Unassigned and back. Values returned after a second fresh enabled launch. Axe removal/restoration matched attacks per second 1.20 → 1.00 → 1.20; axe restored.
- Switched through ordinary hero selection to Wizard and matched level 1, attacks per second 1.20, armor 16, zero cooldown/movement, Magic Missile/no-rune and five empty/locked slots. Clearing/restoring Magic Missile correctly changed its row to Unassigned and back. The owner separately used physical +/Y controls; lower-page navigation left the game's Skills selection unchanged.
- Completed a 108.47-second screen-off test: both displays were black; Eden remained paused on wake with the lower display black. Resume restored Wizard level 1, attacks per second 1.20, cooldown reduction 0.00%, armor 16, movement +0% and Magic Missile.
- Completed ordinary door travel New Tristram → The Slaughtered Calf Inn → New Tristram. Both companion pages retained Barbarian level 2, attacks per second 1.20, zero cooldown/movement, armor 31 and Bash/Hammer no-rune at the destinations. The brief loading interval was not captured; this is one town/inn route only.
- Short stationary presentation windows measured 51.22–52.15 events/second disabled and 50.59–51.57 enabled, with similar approximately 33.38 ms p95 intervals. No obvious large cadence regression appeared; unique guest FPS, zero overhead, CPU/battery impact and sustained combat were not established. See [method and limits](validation.md).

Earlier private `0.0.8-research` observations are dated separately in [validation](validation.md). They do not substitute for the remaining current-candidate checks.

Follow-up after publication: the unchanged beta passed one level-2 softcore Barbarian death in the Inn and town resurrection. The sheet stayed visible during death; level/all four statistics matched the game menus afterward. Shield removal/restoration matched armor 31 → 22 → 31, and the shield was restored. The Thor was left alive in New Tristram, companion enabled and touch overlay hidden.

## Remaining before a working release

1. Broaden nonzero stat/rune coverage where available. Bash, Hammer of the Ancients and Magic Missile/no-rune comparisons are complete; Hammer and Magic Missile clear/restore checks passed. Restrict declared field/class/mode support to observed behavior; actual rune names remain outside this candidate.
2. Finish broader dungeon/waypoint travel, loading intervals, process termination and other unavailable/error states, plus broader physical-input checks. Startup, normal quit, two fresh Barbarian launches, the different Wizard comparison, 108.47-second suspend/resume and one town/inn round trip are already observed. One subsequent Barbarian death/town-resurrection check passed. Other resurrection routes and Hardcore remain outside the verified scope.
3. Extend the short stationary enabled/disabled presentation checks to representative sustained gameplay. Retain comparable conditions and the procedure/results; presentation cadence alone does not measure CPU cost, battery impact or unique guest FPS.
4. Review source, dependency notices, package contents and dated device evidence. Only then update compatibility/support metadata and publish a working release. If a required check fails, keep the artifact labeled as a candidate and record the limitation.

Use only read-only data access. Preserve the normal profile/settings, leave `dump_exefs` disabled, and do not replace app-owned configuration files with `adb push`. The reviewed Eden save import/export UI targets profile 0 even when another profile is selected, so do not use it to import the test save. See the [device protocol](device-validation.md).

No game-memory writes, inventory actions, emulator fork, root, firmware changes or paid services are part of this milestone. Private game material, recovery records and game-display captures remain outside public source and archives. The published lower-display captures contain only the companion UI.
