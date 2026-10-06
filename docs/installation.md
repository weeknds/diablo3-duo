# Install the development preview

Version `0.1.0-dev` is a static lower-screen status page with no live values.
Installation through Add-ons and lower-screen rendering were observed on a Thor
with Eden Duo 1.1.0 on 2026-10-06. Gameplay input and live functionality remain
unverified. Update/removal behavior below also draws on reviewed upstream source.

## Prepare

1. Use a separate development profile and a copy of your save. Preserve your
   normal setup. Confirm the game reaches gameplay without a companion.
2. Check the title ID shown for your game is `01001B300B9BE000` and record its
   update version. There are currently no verified executable builds.
3. Use an Eden Duo release providing companion runtime 18. Upstream 1.1.0 is the
   release reviewed for this guide; check the installed device version.
4. Obtain a clearly labelled development preview `.dsmod.zip` from this project's
   CI artifacts, or build it with `python3 tools/build.py`. A CI download is an
   outer ZIP: extract it and install the inner `.dsmod.zip`.

Keep a provided `SHA256SUMS` beside the inner package and run
`shasum -a 256 -c SHA256SUMS` on macOS or `sha256sum -c SHA256SUMS` on Linux.
The checksum checks download integrity, not gameplay compatibility.

## Install and check

Copy the `.dsmod.zip` to a location accessible to Android's file picker. In Eden
Duo, long-press the game, open **Add-ons → Install → Dual screen mods**, and select
the package. Check that the companion is enabled, then start the game.

The expected lower screen says **DEVELOPMENT PREVIEW** and **Live game data is
not connected**. Health, resource and skills remain unavailable. Record whether
it appears and whether text, placement and normal game input work. This would
establish package loading only.

No root, firmware change or USB debugging is required by this static package.

## Update or remove

Exit the game before changing add-ons. Install a newer `.dsmod.zip` through the
same menu. Current upstream stages and validates a replacement before installing
it and may preserve the previous disabled state, so check the enable switch.
Keep the old package if you need to restore that version.

To disable the companion, turn it off in the game's Add-ons screen. To remove it,
use its delete control and confirm uninstall. Do not delete the game's save or
app data. Recheck normal gameplay after removal.

## Troubleshooting

| Symptom | Next check |
| --- | --- |
| Game fails before the companion is installed | Diagnose the emulator/update/driver setup first. This companion does not fix game launch. |
| Package rejected | Select the inner `.dsmod.zip`, confirm title ID and runtime, and record the exact error. |
| Lower screen missing | Check add-on enablement, dual-screen configuration, and installed Eden Duo version. |
| Empty health/resource/skills | Expected for this static preview. No live reader exists. |
| Clipped text or input problems | Disable the companion and record setup and reproduction steps. The captured layout covers one Thor setup; gameplay input remains unverified. |

Sources: [installation](https://github.com/igawa6/eden-duo#install) and
[reviewed Add-ons code](https://github.com/igawa6/eden-duo/blob/5410366938b8aa0aca2c9c61f4e93038b71db90c/src/android/app/src/main/java/org/yuzu/yuzu_emu/fragments/AddonsFragment.kt).
