# Compatibility

The current source also builds a local **0.2.2-dev marker candidate** for the
same exact target below. Its new quest/location and goblin readers are verified
only with synthetic fixtures; Thor behavior and overhead are pending. The last
installed/observed package remains 0.2.1. See [marker scope](map-markers.md).

**0.2.0-dev is an experimental live preview for one game build.** It is not a
stable-support or all-classes claim. See [validation](validation.md) for dated
observations and the exact tested artifact.

| Item | Target and scope |
| --- | --- |
| Game | Diablo III: Eternal Collection, Nintendo Switch |
| Title ID | `01001B300B9BE000`, confirmed in Eden Duo Info |
| Update | `2.7.7.92380` |
| Full executable build ID | `2607A74F5DF7754CC0357B5DF7E496931355D8CA000000000000000000000000` |
| Runtime build key | `2607A74F5DF7754C` |
| Native platform | Android arm64; built with NDK r28c (`28.2.13676358`) |
| Emulator/runtime | Eden Duo 1.1.0, versionCode 33940730, companion runtime 18 |
| Observed device | AYN Thor, Android 13/API 33, NCE, Turnip Adreno T30 |
| Canvas | 1240 × 1080 lower display |
| Views | Character, Equipment, Skills and Map |
| Character reader | Class, level, Paragon, four primary attributes, attack speed, armor, critical chance, cooldown reduction, primary resource cost reduction and movement bonus |
| Equipment reader | Thirteen body slots; observed empty/occupied state; localized base-name inspection |
| Skills | Six equipped slots; printable ASCII names, known no-rune states; actual rune names unavailable |
| Map | Explored coverage, supported static terrain, player position, zoom, recenter and one local pin |
| Input | Companion touch actions only; controller navigation disabled; no game-memory writes or equipment actions |

Missing or inconsistent data stays unavailable. The exact-build and host-ABI
gates reject other executables; a build gate alone is not compatibility evidence.

Names currently support printable ASCII only. Base item names omit affixes,
rare names, rolls, sockets and comparisons. Unidentified or unverifiable item
names remain unavailable. Sheet damage/toughness/recovery, critical damage,
passives, buff timers, pylon/exit labels and Greater Rift progress/timing are not
implemented. Character imagery and redundant health/resource bars are omitted
by the owner's design choice.

Terrain uses supported planar leaf scenes and is clipped to explored coverage.
Unsupported scene transforms/parent overrides may leave gaps, which the UI
reports. This is static ground information; dynamic doors and actors are not
navigation guarantees. Changing worlds clears the local pin.

Early non-seasonal Barbarian and Wizard observations from `0.1.2-rc.1` remain
historical. They do not prove new equipment/terrain behavior or its overhead.
Broader classes, high-level builds, nonzero reduction bonuses, languages,
seasonal/Hardcore/multiplayer play and sustained combat are not established.

The separate root `0.1.1-dev` package is a static preview. Its `project.json`
continues to declare no supported builds or live data. Those flags describe that
output, not the native preview. Build the correct package as described in
[installation](installation.md).


### 0.2.1 map correction

Targets the same full executable build and runtime 18 as 0.2.0. The changes are
confined to map position, presentation, camera controls and terrain image lifetime.
The ordinary-town rotation is known; special mode/world rotations remain outside
this evidence. The runtime caps ordinary visual publications near 30 Hz; a 60 Hz
map or exact reproduction of the game's map artwork is not claimed.
