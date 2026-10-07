# Device validation protocol

This is a reusable procedure, not a completed test record. See
[validation](validation.md) for observations and pending checks. Retain raw
recordings, saves and research captures under `private/`; publish only authorized,
redacted observations.

## Record and preserve the setup

Record date, device model, Android version, Eden Duo release/runtime, driver and
settings, CPU backend, title ID, game version, full executable build ID and
add-ons. Record the candidate ZIP/module hashes. Keep device serials private.

Use a separate test profile/save. Back up its save and the current companion
before replacement; preserve the normal gaming profile and settings. Verify
actual profile/save paths before import or export rather than assuming the
selected profile controls the destination. Install through Eden's Add-ons UI;
avoid replacing app-owned configuration with `adb push`.

USB debugging is optional for manual checks and requires owner authorization.
Do not root or flash the device. Record temporary overlay/display changes and
restore them after testing.

## Distinguish the two packages

Both ZIPs are named `01001B300B9BE000.dsmod.zip`; their directories and contents
differ. Verify the version/hash before installation.

- **Static `0.1.1-dev`, `dist/`:** Character, Combat and unavailable Map pages.
  Expect **DESIGN PREVIEW** and **Live game data is not connected**. It cannot
  validate live acquisition.
- **Native `0.1.2-rc.1`, `dist/live/`:** Character and Skills pages, **DEVICE TEST**
  label and an exact-build native module. Expect unavailable values at the
  title screen, then only accepted character data during gameplay.

First confirm controllable gameplay with the companion disabled. Exit before
installing the candidate through Add-ons, then launch again. Record package
loading, module loading and UI rendering separately. Check text, clipping,
contrast, touch targets and both native pages on the physical lower screen.

## Validate values, lifecycle and input

- Compare level, each available statistic and assigned skill names with the
  game's own menus. Record class, mode, exact values, actions and screenshot
  timestamps. Change equipment or skills through normal game controls and
  verify that the companion follows the change and restoration.
- Check CDR against the menu, including the registered-default path if its cache
  key is absent. Verify movement values beyond +0% before claiming broader
  coverage. Rune names and passive/buff data remain outside the current reader.
- Completely exit and relaunch twice; record both results and relocation.
  Repeat with another character in the test profile, comparing its own menus.
- Check title/menu states, loading, character changes, death, travel,
  suspension/resume and game restart. Verify unavailable states and recovery;
  stale values must not survive a lost or changed player identity.
- Check Character ↔ Skills navigation and View skills. Tap/swipe the lower
  display, then use physical movement, attacks, menus and normal controls.
  Record whether companion interaction affected the main-screen game.
- Check unsupported-build rejection without substituting guessed offsets or
  modifying the game. Record what is genuinely exercised on hardware and what
  remains a synthetic guard test.
- Disable/remove the companion and verify the game still starts. Restore the
  original companion/settings if the candidate fails.

Keep bounded-read, type, ownership and lifetime evidence separate from UI
observations. Never insert guest writes to discover whether a read is correct.

## Measure overhead

Use the same device, test save, scene/route, settings, frame limit and driver for
companion-disabled and enabled runs. Record warm-up, sample duration,
repetitions/order, thermal state, charger/power conditions and measurement source.
Use repeated runs in both states and compare frame times/FPS plus visible input
responsiveness. Retain raw samples privately and report variability and scope.
An idle scene cannot establish combat performance; battery claims require an
actual battery-measurement protocol.

Investigate material regressions before enabling support. Package loading,
correct values, lifecycle, controls and performance are separate outcomes;
a pass in one category cannot fill in another.
