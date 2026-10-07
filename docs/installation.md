# Install a preview or live candidate

`0.1.2-rc.1` is a live candidate for controlled device testing. It has Character and Skills pages and an exact-build native reader. Installation, exact-build module loading, both page actions and early-level Barbarian/Wizard menu values have been observed on the Thor. Two short stationary presentation checks per condition showed no obvious large cadence regression; sustained gameplay, remaining lifecycle and broader input/field validation remain pending. `0.1.1-dev` remains a separate static design preview with Character, Combat and Map pages and no live values.

## Prepare

1. Use a separate development profile and a copy of your save. Preserve your normal setup and confirm the game reaches gameplay without the companion.
2. Check the game title ID is `01001B300B9BE000`. The live candidate targets update `2.7.7.92380` and only the full executable build recorded in [compatibility](compatibility.md). Another update is not supported by this candidate.
3. Use Eden Duo with companion runtime 18. The current test device has Eden Duo 1.1.0, versionCode 33940730, on Android 13. This is a test configuration, not a general support claim.
4. Build the chosen package or obtain its clearly labeled artifact. Keep a copy of the previously installed companion package so it can be restored.

| Package | Build command | Install this file |
| --- | --- | --- |
| Live candidate | `python3 native/build.py --ndk /path/to/android-ndk-r28c` | `dist/live/01001B300B9BE000.dsmod.zip` |
| Static preview | `python3 tools/build.py` | `dist/01001B300B9BE000.dsmod.zip` |

A CI download may be an outer ZIP: extract it and install the inner `.dsmod.zip`. From the directory containing that package and its `SHA256SUMS`, run `shasum -a 256 -c SHA256SUMS` on macOS or `sha256sum -c SHA256SUMS` on Linux. This checks download integrity, not gameplay compatibility.

Do not use Eden's reviewed save import/export UI to import a test save into a secondary profile: the reviewed implementation targets profile 0. Verify the selected test profile and preserve an independent backup before device testing.

## Install and check

Exit the game. Copy the chosen `.dsmod.zip` to a location accessible to Android's file picker. In Eden Duo, long-press the game, open **Add-ons → Install → Dual screen mods**, and select the package. Confirm the companion is enabled, then start the game with the test profile.

For the **live candidate**, expect a **DEVICE TEST** badge and two tabs, **Character** and **Skills**. Startup without a loaded character should show unavailable values. The title-screen unavailable state, both tab actions and physical +/Y buttons have been observed for this candidate. Level-2 Barbarian and level-1 Wizard statistics matched the menus, including zero cooldown/movement. Bash, Hammer of the Ancients and Magic Missile/no-rune states matched Skills; clearing/restoring Hammer or Magic Missile updated the corresponding row. Normal quit cleared all value rows and Barbarian values repeated after a second fresh enabled launch. Lower-page navigation left the game's Skills selection unchanged. A 108.47-second screen-off test left Eden paused on wake; Resume restored Wizard values. A New Tristram → The Slaughtered Calf Inn → New Tristram door trip retained the correct Barbarian values at both destinations; its brief loading interval was not captured. These checks do not establish every skill, value range, lifecycle state or character. In gameplay, compare every displayed value against the game's own menu. Unknown skill/rune information must remain unavailable. Check the tabs and **View skills** link, normal physical controls, suspend/resume and return to the main menu. Record discrepancies; do not treat a plausible number as proof.

For the **static preview**, expect **DESIGN PREVIEW**, **Live game data is not connected**, and three tabs: Character, Combat and Map. All values stay unavailable. Tabs and the skills link only switch companion pages. The current static redesign has not been validated on the Thor.

Neither package requires root or firmware changes. USB debugging is a development-test aid, not an installation requirement. Both interfaces omit character imagery and health/resource bars.

## Update or remove

Exit the game before changing add-ons. Install a newer package through the same menu. Reviewed upstream code stages and validates replacements and may preserve a disabled state, so check the enable switch after updating. Both packages target the same game: installing one replaces the companion for that title rather than adding a second independent copy.

To disable the companion, turn it off in the game's Add-ons screen. To remove it, use its delete control and confirm uninstall. Do not delete the game's save or app data. To restore an earlier companion, install the retained package through Add-ons. Recheck normal gameplay afterward.

## Troubleshooting

| Symptom | Next check |
| --- | --- |
| Game fails without the companion | Resolve the emulator/update/driver issue first. |
| Package rejected | Select the inner `.dsmod.zip`; confirm title ID, runtime and checksum, then record the exact error. |
| Lower screen missing | Check add-on enablement, dual-screen configuration and Eden Duo version. |
| Live candidate reports unavailable | Enter gameplay with the test character. Confirm the exact update/build; if still unavailable, record the state and version. Do not substitute guessed values. |
| Static preview reports unavailable | Expected: this package contains no live reader. |
| Clipped text, incorrect values or control problems | Disable the companion and record the setup and reproduction steps for the candidate being tested. |

Installation and replacement behavior were reviewed in [Eden Duo's installation guide](https://github.com/igawa6/eden-duo#install) and [pinned Add-ons source](https://github.com/igawa6/eden-duo/blob/5410366938b8aa0aca2c9c61f4e93038b71db90c/src/android/app/src/main/java/org/yuzu/yuzu_emu/fragments/AddonsFragment.kt). Broader field/input coverage, remaining lifecycle and sustained gameplay checks remain pending. The [validation record](validation.md) documents the completed short stationary presentation checks and their limits.
