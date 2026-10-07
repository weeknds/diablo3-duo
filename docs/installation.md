# Install the live preview

`0.2.0-dev` is an experimental, read-only companion. Check the exact game build
and tested setup in [compatibility](compatibility.md). Back up saves and use a
separate test profile. Keep the previous companion package for rollback.

1. Obtain `.dsmod.zip` and `SHA256SUMS` from the selected GitHub release, or build
   with `python3 native/build.py --ndk /path/to/android-ndk-r28c --output dist/native-0.2.0`.
2. In their directory run `shasum -a 256 -c SHA256SUMS` on macOS or
   `sha256sum -c SHA256SUMS` on Linux. This checks integrity, not compatibility.
3. Exit the game and copy the package to the Thor's Downloads folder.
4. In Eden Duo, long-press Diablo III and open **Add-ons → Install → Dual screen mods**.
   Select `01001B300B9BE000.dsmod.zip`. Check that the companion is enabled.
5. Start Diablo III using the test profile. The lower screen shows **LIVE PREVIEW**
   with Character, Map and Skills tabs. Character's Equipment button opens the
   slot sheet. Values are unavailable until a valid character is loaded.

This version targets update `2.7.7.92380`, its one recorded executable build,
Android arm64 and companion runtime 18. The observed device is an Android 13
Thor using Eden Duo 1.1.0. Another update or platform is not established support.

Compare values against the game's menus. Tapping equipment only reads its base
name; it cannot equip or compare item rolls. Map pins mark the current position
locally and disappear on a world change. Partial terrain, unavailable names and
unsupported rune names are stated explicitly.

Neither root nor USB debugging is required for ordinary installation. Preserve
the normal profile and app data. The reviewed Eden save import/export code targets
profile 0, so do not use it to import a secondary test profile.

## Update, disable or restore

Exit gameplay before changing add-ons. Install through the same menu; check the
enable switch afterward. A package for this title replaces the existing companion.
Use Eden's installer rather than pushing files into its managed folder.

Disable only **DualScreen-01001B300B9BE000** to compare normal gameplay. Leave the
update and DLC enabled. Reinstall the retained previous package to roll back.
The add-on's delete control removes the companion, not the game save.

## Troubleshooting

| Symptom | Check |
| --- | --- |
| Package rejected | Select the inner `.dsmod.zip`, not an outer CI ZIP; verify checksum, title and runtime. |
| Lower screen absent | Check add-on enablement, Eden Duo version and dual-screen configuration. |
| Values unavailable | Enter gameplay and confirm the exact update/build. Record persistent failures. |
| Partial map | Some scenes are unsupported; terrain excludes dynamic doors and actors. |
| Wrong value or control problem | Disable the companion and record setup, state and reproduction steps. |

The older `python3 tools/build.py` output is a separate **0.1.1-dev static preview**
with no live values. Both outputs use the same ZIP filename.

See [validation](validation.md) for completed checks and limits. Installation
behavior is documented by [Eden Duo](https://github.com/igawa6/eden-duo#install).
