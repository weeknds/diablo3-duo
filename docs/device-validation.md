# Device validation protocol

All observations below are **pending**. This document is a procedure, not a
completed test record. Keep raw recordings and research captures in `private/`.
Publish only redacted observations or recordings you have permission to share.

## Record the setup

Record the date, device model, Android version, Eden Duo release and runtime,
graphics driver/settings, CPU backend, game title ID, display version, full
executable build ID, and enabled add-ons. Do not publish the device serial.
Use a copied test profile/save and retain the original unchanged. Stop before
an installation or import that could overwrite the normal setup.

USB debugging is optional for a manual preview check. Do not root or flash the
device. Enabling debugging or pairing requires the owner's authorization.

## Check the game and preview separately

1. With the companion disabled, launch the game, load the copied save, and reach
   controllable gameplay. Record any game-launch issue before adding a companion.
2. Exit the game, install the development ZIP, and launch again. Record whether
   the lower screen displays `DEVELOPMENT PREVIEW` and
   `Live game data is not connected`.
3. Check text visibility, clipping, display placement, and normal controller
   input. Touching the static page should not trigger game actions.
4. Disable or remove the companion and confirm the original game still starts.

## Validate the first live reader

This phase requires an implemented reader for the exact executable build.

- Compare a value with the game's own menu at controlled, reproducible states.
  Record the observed values and the exact actions that changed them.
- Completely exit and relaunch twice; record both launch results and relocation.
- Repeat with a different copied character/save. Record class and game mode.
- Check title menus, loading a save, character changes, death, travel, suspension,
  resume, and game restart. Record valid, unavailable, and recovery behavior.
- Confirm no stale numbers remain when the character/session is unavailable.
- Reject an unknown build visibly; never reuse another build's offsets.

Retain the pointer route, bounds, type, ownership, object-lifetime checks, and
failure handling separately from the UI. A fixed heap address from one run is
not a supported reader. Do not insert writes to discover whether a read is correct.

## Measure overhead

Use the same device, copied save, route, settings, frame limit, driver, and
measurement method for companion-disabled and companion-enabled runs. Record
warm-up, duration, repetitions, thermal/power conditions, and the measurement
source. Compare frame time or FPS and visible input responsiveness. Retain the
raw samples privately and state variability and limitations. Battery claims
require an actual battery measurement protocol.

Investigate a material regression before enabling the build. Keep package-load,
value-correctness, input, and performance results as separate outcomes; a pass
in one category must not fill in another.
