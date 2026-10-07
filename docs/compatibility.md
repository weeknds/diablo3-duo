# Compatibility

**No supported live-data build is declared yet.** `0.1.2-rc.1` is an exact-build live candidate with installation, rendering and early-level Barbarian/Wizard menu comparisons observed; remaining lifecycle and sustained gameplay validation remain pending. `0.1.1-dev` is a separate static design preview. Root `project.json` continues to describe that static package, with `supported_builds: []`, `live_data_available: false` and `verified_on_thor: false`.

## Live candidate target

| Item | Candidate or observed configuration |
| --- | --- |
| Game | Diablo III: Eternal Collection, Nintendo Switch |
| Title ID | `01001B300B9BE000`, confirmed in Eden Duo Info |
| Game display version | `2.7.7.92380`; enabled update reconfirmed on the connected device on 2026-10-07 |
| Full executable build ID | `2607A74F5DF7754CC0357B5DF7E496931355D8CA000000000000000000000000` |
| Runtime build key | `2607A74F5DF7754C` |
| Module platform | Android arm64; built using NDK r28c (`28.2.13676358`) |
| Companion runtime | Minimum 18 |
| Emulator | Eden Duo 1.1.0, versionCode 33940730, reconfirmed 2026-10-07 |
| Device | AYN Thor / Android 13; Black Max / 1 TB is owner-specified |
| Candidate pages | Character and Skills; page-only touch actions, controller navigation disabled |
| Candidate fields | Level, attacks per second, cooldown reduction, armor, movement bonus and six equipped skill names |
| Rune coverage | No-rune state only; actual rune names are not implemented |
| Name language | Printable ASCII only; current menu comparisons used English |
| Excluded features | Health/resource HUD, character imagery, passives, maps, buff timers, item comparison and equipment actions |
| Fresh candidate installation | Installed through Add-ons; package/version and native-module hash matched device readback |
| Fresh candidate startup/layout | Exact-build module loaded; 1240 × 1080 title-screen UI rendered with unavailable values and clear startup status |
| Fresh candidate page actions | Logs recorded both Character and Skills actions after lower-display taps |
| Fresh candidate matched values | Barbarian level 2 / armor 31 and Wizard level 1 / armor 16; both matched attacks per second 1.20, cooldown reduction 0.00% and movement +0% in the game menus |
| Fresh candidate controlled stat change | Barbarian axe removal/restoration matched attacks per second 1.20 → 1.00 → 1.20; axe restored |
| Fresh candidate matched skills | Bash, Hammer of the Ancients and Magic Missile/no-rune matched Skills; Wizard's five empty/locked slots matched. Clearing/restoring Hammer and Magic Missile changed their rows to Unassigned and back |
| Fresh candidate physical controls | +/Y and ordinary game skill clear/restore actions worked; lower-page navigation left the game's Skills selection unchanged. Broader input coverage pending |
| Fresh candidate normal quit | Main-menu return cleared level, all four stats and all six skill rows |
| Fresh candidate suspend | 108.47 seconds screen-off; both displays black. Eden was paused on wake with the lower display black; Resume restored Wizard level 1, stats and Magic Missile |
| Fresh candidate repeat launch | Barbarian level/stat values and displayed skills returned after a second fresh enabled launch |
| Fresh candidate travel | New Tristram → The Slaughtered Calf Inn → New Tristram through ordinary door interaction; both pages retained correct Barbarian values at the destinations. Brief loading interval not captured |
| Fresh candidate remaining checks | Nonzero cooldown/movement, death, broader travel/loading/input coverage, other lifecycle states and sustained gameplay measurements |
| Short stationary presentation checks | Disabled 51.22–52.15 vs enabled 50.59–51.57 presentation events/second; approximately 33.38 ms p95 intervals in both conditions. No obvious large cadence regression in these short windows; no zero-overhead or sustained-combat claim |

The reader accepts only the recorded title/build and validates its host ABI. It performs bounded reads and replaces unavailable data rather than retaining stale character values. It does not write game memory. These safeguards and synthetic tests do not establish Android NCE correctness or broad class, mode, language or update coverage.

The unchanged recovered native module rebuilt to SHA-256 `9736a2ed41d0f302f5883e3e508238c9022b216816d70fc63b1633799ab53644`. This proves binary reproduction; it does not prove that the new package works on hardware.

## Earlier evidence and its limits

Private `0.0.8-research` testing on 2026-10-06 matched level across two fresh launches, a Barbarian level-up from 1 to 2 and a fresh level-1 Wizard. Magic Missile, Bash and Hammer of the Ancients matched equipped slots. Attacks per second and armor matched menu values and changes after removing/restoring a weapon or shield. Movement was checked only at +0%, and only the no-rune state was observed.

That earlier prototype cleared values at startup and normal quit, recovered after an approximately 90-second suspend, and left the game's menu selection unchanged during one lower-display tap/swipe check. The current candidate now has fresh zero-cooldown/movement matches on both classes, Bash/Hammer of the Ancients/Magic Missile comparisons, a controlled weapon-stat change and skill clear/restore evidence. Normal quit cleared all value rows. Nonzero cooldown/movement and broader skill-name coverage remain pending. The current candidate also retained correct values through one New Tristram/inn round trip. Death, broader travel/loading, full input coverage and sustained gameplay measurements remain pending. Current short stationary presentation results are recorded in [validation](validation.md); they are separate from the earlier prototype evidence. Earlier research is not final-candidate evidence.

Static `0.1.0-dev` was installed through Add-ons and rendered at the game title screen. Its [historical lower-screen capture](images/development-preview-thor.png) does not validate `0.1.1-dev` or the live candidate. Current `design-*.png` files are offline approximations. The [live Character](images/live-candidate-character-thor.png), [Skills](images/live-candidate-skills-thor.png) and [startup](images/live-candidate-startup-thor.png) images are actual lower-display captures from 2026-10-07; only the comparisons stated above have been verified.

## Requirement for declared support

Before enabling a build, retain a reviewed record of the game version, full build ID, Eden Duo/runtime version, device/backend/driver settings, tested class/mode/fields, fresh-launch comparisons, unavailable/error states, lifecycle/input results and measured overhead. Do not publish game bytes, saves or private memory captures with that record.

An NSO header establishes an executable identifier, not a title or running update. An NSP's optional CNMT XML is unauthenticated advisory metadata. Neither alone establishes compatibility. See [current work](next-step.md), [validation](validation.md) and the [device protocol](device-validation.md).
