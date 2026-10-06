# Continue live integration

## Current verified state — 2026-10-06

- Production package remains `0.1.0-dev`, static UI only, runtime 18; no supported
  builds or live reader. All 37 synthetic tooling tests pass; review fixes complete.
- Owner installed the game update. Eden Duo Info and enabled Add-ons show
  Diablo III `2.7.7.92380`, title `01001B300B9BE000`, DLC 100–102 enabled. The game's
  own title screen also shows `2.7.7.92380`.
- AYN Thor/Android 13/API 33/arm64 and Eden Duo 1.1.0 (versionCode 33940730) observed
  through authorized ADB. No other Eden installation was modified.
- Effective updated ExeFS was captured through Eden's built-in `dump_exefs` route
  and copied privately; `main` SHA-256 matched the device. Full NSO build ID:
  `2607A74F5DF7754CC0357B5DF7E496931355D8CA000000000000000000000000`.
  Runtime data key: `2607A74F5DF7754C`. This identifies a build, not a supported reader.
- Original settings and account database are backed up privately. Account backup
  hash matched the device. No existing Diablo III save was found. A new `DuoTest`
  profile was created and selected (index 1); the original account remains.
- The owner confirmed that a fresh non-seasonal Barbarian reached gameplay, and
  a device capture recorded that session. The test save is backed up privately.
- Static `0.1.0-dev` was installed through Add-ons and rendered correctly on the
  lower screen at the game title screen. The
  [original lower-screen capture](images/development-preview-thor.png) shows the
  development message and unavailable live data. Gameplay input routing with
  that static package active has not been verified.
- The temporary `dump_exefs` setting is restored to `false`; the exact two-line
  restoration and resulting configuration hash were verified. An ADB copy
  initially left this file shell-owned; Eden recreated it from cached settings
  to restore app ownership. Parsed settings were verified afterward. Avoid
  replacing app-owned configuration files with `adb push`.
- NDK r28c is installed and verified in the project-private toolchain directory.
  A separate private diagnostic module passed review and sanitizer tests, loaded
  on the Thor, and completed one bounded four-byte read. This confirms the
  host API only. A subsequent private level probe displayed candidate level 1
  matching the Barbarian's Inventory menu through two fresh game launches.
  It showed unavailable at startup and after quitting to the main menu.
  A physical-control play session changed the Barbarian from level 1 to 2;
  Inventory and the probe both showed 2. A fresh non-seasonal Wizard matched
  at level 1. With the private probe active, a tap and swipe directed to the
  lower display left the game's Inventory state unchanged. The first level
  milestone is observed; further fields, full lifecycle/input checks and
  performance measurement remain pending.

## Immediate continuation

1. Check `private/device/session-state.json` for current device ownership and
   research state before interacting. Preserve owner settings, the test profile
   and the private save backup. Leave `dump_exefs` disabled.
2. Review the retained private level and attribute-reader evidence. Complete
   gameplay and controller/touch coverage with the final live candidate; the
   static preview's title-screen rendering is separate evidence.
3. Follow the accepted [build and character details direction](ui-spec.md).
   Prioritise equipped skills, selected runes, passives and useful menu-only
   statistics. Health is an internal attribute-reader test and must not become
   a default readout merely because it is easy to validate. Keep the private
   research outside production until each field's evidence has been reviewed.
4. Correlate a useful field with normal gameplay changes, derive a reproducible
   pointer route and validate type, bounds, ownership, relocation and lifetime.
   Apply the level milestone's menu comparison and repeat-launch/character
   checks to each added field, then exercise further loading, death, travel
   and suspend/resume transitions.
5. Measure performance and companion overhead on the Thor before claiming live
   support. Update compatibility and production flags only after the required
   evidence has been reviewed.

No game-memory writes, sentinel writes, inventory actions, emulator fork, root,
firmware changes or paid services are part of the current work.

## Environment and safety findings

Native modules cannot load on macOS. Android release builds omit developer
memory scanners and profiling, but retain the native bounded-read module API.
A small module can be compiled on the Mac with the standalone Android NDK;
no Java/Gradle or emulator rebuild is required for that path. NDK r28c is installed
under ignored `private/toolchains/`; its download was verified against Google's
repository manifest. No global toolchain setting was changed.

Save import/export in reviewed upstream code targets profile 0 even after
selecting another profile. Do not use that UI to import a test save. The actual
selected profile and effective NAND/save roots must be verified independently.
See [upstream research](upstream-research.md) and [device protocol](device-validation.md).

## Publication checkpoint

English documentation, CI, the original lower-screen screenshot and the exact
`public-files.txt` source allowlist have passed prepublication review. The owner
has authorized publication of truthful development source on `main`, now available
at [weeknds/diablo3-duo](https://github.com/weeknds/diablo3-duo). Tooling CI passed
on Python 3.10/3.14 after correcting a test-only failure-injection portability issue.
Never stage `private/` or `.serena/`. A working release remains gated on useful
verified live values, lifecycle/input testing, measured overhead and matching
compatibility records.
