# Map markers candidate — 2026-10-07

`0.2.2-dev` was implemented and packaged offline while the Thor was disconnected
for charging. **It is not installed or hardware-validated.** The last observed
device package and public download remain 0.2.1, documented below.

- Local archive: `dist/release-0.2.2-markers/01001B300B9BE000.dsmod.zip`
- Package SHA-256: `cae8329e466b3a47317544aa69e7450ade3a42dff2c45069cc53fc9db5fc7530`
- Module SHA-256: `42827d0926b6b5c3b4b4a4b1cad6308164d7e946d803127b20f7c97134e9a9e3`
- Archive: 392608 bytes; Android arm64, pinned NDK r28c, same exact game build.

The new quest/location and goblin reader suites and marker presentation suite
passed focused ASan/UBSan checks. The integrated module suite passed after the
readers were added. All 21 native UI/package checks and the separate static
manifest guard passed. Android compilation and archive membership/hash checks
passed. The first and final Android compilation produced the same module hash;
no separate-directory archive reproducibility run or broad test cycle was added.

A fresh review accepted the synthetic 1240x1080 sample and unavailable frames at
offline layout/interaction-spec scope, including the native selection binding
contract. The named review configuration was unavailable, so the fresh reviewer
applied Impeccable's review contract directly. A separate documentation check
preserved the incumbent design system and reported older documentation drift.
These are not device screenshots, gameplay evidence or measured performance.

The next device check is a focused installation and observation of genuine
quest/waypoint/entrance and goblin events, touch selection, world-change clearing
and noticeable reader overhead. Shrines, pylons and localized place names remain
unimplemented; see [marker behavior and read limits](map-markers.md).

# Map correction checkpoint — 2026-10-07

`0.2.1-dev` targets the same Diablo 2.7.7.92380 executable and Eden Duo 1.1.0 /
runtime 18 on AYN Thor. It corrects owner-reported map flicker, following and
coarse presentation. Previous 0.2.0 static captures did not establish motion
quality and must not be used as evidence that those defects were absent.

## Exact artifact

- Final package SHA-256: `89732cc3ab8b1e56e0ce7a0c4f2b840a796a5d3a97419c74e7eaa5f03286c269`
- Module SHA-256: `cbc2105ba4f54350ffee757d788394765599492f9feba38d8c1e94cfbb64db5e`
- Archive: 340055 bytes, NDK r28c. Source and asset inventories remain pinned.

## Device observation and limit

The exact module above was installed through Eden's package installer; its
on-device SHA-256 matched. An 18-second recording on the level-2 DuoTest
Barbarian in New Tristram covered movement in several directions, placing a
local pin, touch panning, zoom after panning and recentering. All 146 decoded
frames retained terrain and the white player marker. During unobstructed
walking the marker stayed near the panel center; the map and fixed-location
pin moved relative to it. After dragging, zoom returned the view to following.
The normal hidden game touch controls were restored.

The clip and published screenshot use the final module and visual parameters
**before** the final manifest-only `gpu_composite: true` setting. After the Thor
reconnected, the final archive was installed through Eden's installer. Device
readback matched the release manifest, including that flag, and module hash.
Diablo was relaunched in DuoTest; terrain and the centered player marker rendered
in New Tristram with the GPU setting enabled. The touch overlay was restored to
hidden and the Map page left open. Installation and rendering are confirmed;
GPU movement smoothness was not measured or separately recorded. No additional
movement benchmark or broad test cycle was run.

A CPU redraw path was identified and the final package opts into GPU map quads.
The recording is a short functional check, not a frame-rate benchmark. No 30/60
FPS result, sustained-combat performance or broad world compatibility is claimed.
Ordinary-town minimap rotation is confirmed; special rotation modes are not.
Terrain remains numeric ground coverage, without game artwork, building artwork,
dynamic obstacles, pylons, exits or Greater Rift timing.

## Focused local validation

Map reader, renderer and module regressions passed under ASan/UBSan. They cover
client-vs-ACD positions, fast movement between heavy scans, immutable late image
requests, centered default following, independent pin/zoom, rotated fog/terrain,
failed reads and world/owner changes. The map renderer was rechecked after the
final stroke-width change. The final manifest passed all 20 native UI/package
checks. No unrelated benchmark or extended lifecycle loop was restarted.

Private evidence: `private/device-20261007/map-motion-0.2.1.mp4`,
`map-motion-events.json`, `map-motion-frames/frames.json`,
`map-fix-receipt.json`, `gpu-install-receipt.json`,
`lower-gpu-installed-final.png`, and `lower-map-fix-final.png`. Public source contains only
the companion screenshot, not game captures or private data.

---

# Validation record

## Current preview 0.2.0-dev — 2026-10-07

This experimental preview adds character attributes, equipped-slot inspection
and live exploration/terrain to the portrait-free native dashboard. It is not a
stable or broadly supported release. The separate root static package remains
`0.1.1-dev`.

### Package and focused verification

| Artifact | SHA-256 |
| --- | --- |
| `dist/release-0.2.0-final/01001B300B9BE000.dsmod.zip` | `3b59e3aeed7b01ab616a41c4ae31abf29828629056bbc9c2ac10b7a671efbc03` |
| Android arm64 native module | `a72d57a8a04ce76bcf265c967630c050a2ce531a3089801f5c444a841c9a617b` |

The expansion passed twelve native sanitizer suites and 81 public Python tests
before the final device corrections. Focused equipment, navigation and module
sanitizer checks then passed for the corrected packed-array alignment, dirty-key
lookup and equipped-slot fixtures. The final hand-label correction passed all
20 native UI/package Python checks. NDK r28c compiled with strict warnings and
verified the packaged source/assets, exact executable gate and module hash.
Two builds of the final module matched its pinned binary hash. Static package
validation also passed. These are host checks, not broad gameplay proof.

### Thor observations for this expansion

The same Android 13/API 33 Thor runs Eden Duo 1.1.0/runtime 18 with Diablo III
update `2.7.7.92380`, NCE and Turnip T30. The exact executable gate is:

```text
2607A74F5DF7754CC0357B5DF7E496931355D8CA000000000000000000000000
```

Checks used the separate DuoTest profile and a non-seasonal level-2 Barbarian.
The normal profile was preserved. The final package was installed through Eden's Add-ons UI; device readback
matched the native-module hash above. The companion performs read-only guest access;
its touch actions select views, inspect an item or change a local map pin.

- **Character:** level 2, Paragon 0, Strength 13, Dexterity 9, Intelligence 9,
  Vitality 11, attack speed 1.20, armor 31, critical chance 5%, cooldown reduction
  0%, resource cost reduction 0% and movement bonus +0% rendered. The native
  character-menu comparisons matched the observed fields; nonzero reduction
  bonuses and high-level characters were not checked in this expansion.
- **Equipment:** the axe and shield occupy the two hand slots; the other eleven
  slots are empty. Base-name inspection resolves Weathered Hand Axe and Buckler.
  The native left-hand/right-hand enumeration is mapped to the main-hand/off-hand
  presentation observed in Inventory. Rolled affixes and complete item names are
  not claimed.
- **Map:** New Tristram terrain and explored coverage render around the player.
  Zoom, recenter and a local pin work. The terrain reader accepts the observed
  four-byte-aligned grid allocation and preserves its bounded ownership checks.
  This observation does not establish all dungeon geometry or dynamic obstacles.
- **Skills:** Bash and Hammer of the Ancients render with no rune selected; four
  slots are unassigned. Actual rune names remain outside the implementation.
- **Startup:** character values clear to unavailable before entering gameplay.
  The final package uses the production module, not the earlier private diagnostic.

Companion-only captures are linked from the README. Game-menu comparisons and
raw device evidence remain private. The release includes no game art, ROM,
firmware, keys, saves or executable dumps.

### Deliberate limits

At the owner's request, this completion pass did not repeat extended benchmark,
lifecycle, death, class or equipment-change cycles. The earlier 0.1.2 observations
below remain historical; they do not measure the new readers' performance.
Sustained gameplay overhead, broader classes/languages/modes and all travel/error
states remain unverified. No stable-support declaration is enabled.

Automatic pylons/exits, Greater Rift progress/timing, item rolls/comparison,
sheet damage/toughness/recovery, critical damage, actual rune names, passives and
buff timers are not implemented. Terrain is static ground coverage; unsupported
scenes may be omitted. Missing or rejected data remains unavailable.

## Earlier candidate 0.1.2-rc.1 — 2026-10-07

`0.1.2-rc.1` is a live test candidate, not a stable or broadly supported release. It includes a bounded, read-only native reader and the Character/Skills interface. The separate static `0.1.1-dev` package and its root metadata remain unchanged in scope. No supported build has been declared.

### Host and package checks

Host verification used macOS arm64, Python 3.14 and Android NDK r28c (`28.2.13676358`). No ROM, save, firmware, key or executable dump is an input to these public tests or builds.

- The full public Python suite passed **76 tests**.
- `python3 native/test.py` passed **five C sanitizer suites** and **15 Python tests** covering UI bindings, numeric layout, page actions and candidate packaging.
- Two independent NDK builds produced byte-identical candidate archives. The unchanged native module matched its previously recorded hash.
- Static manifest/asset validation remains separate; the original static builder has not been weakened to accept an unverified live stage.
- Preserved source hashes, ABI headers, native binary and package contents are checked by the [native build](../native/README.md). The source architecture and read/failure limits are documented in [the reader guide](reader.md).

| Artifact | SHA-256 |
| --- | --- |
| `dist/live/01001B300B9BE000.dsmod.zip` | `5b15700157f5a4dd10523ed0888b499b19a9c83050cd2c63f231f3e883bad5c5` |
| Android arm64 native module | `9736a2ed41d0f302f5883e3e508238c9022b216816d70fc63b1633799ab53644` |

These results prove the stated synthetic behavior and reproducible artifact, not full Android NCE, gameplay or performance compatibility. Public-source CI passed all jobs at commit `f93f3e7` ([run 37584002649](https://github.com/weeknds/diablo3-duo/actions/runs/37584002649)), covering synthetic tooling tests, native host checks and static packaging. It does not perform device tests.

### Fresh Thor observations

The death/resurrection and shield-change follow-up below was performed after publication on the same unchanged `0.1.2-rc.1` package. It adds evidence without changing the binary or its experimental status.

The current device is an AYN Thor on Android 13/API 33, with Eden Duo 1.1.0 (versionCode 33940730). Diablo III update `2.7.7.92380` is enabled. The live module loaded for title `01001B300B9BE000` and full executable build:

```text
2607A74F5DF7754CC0357B5DF7E496931355D8CA000000000000000000000000
```

The owner installed `0.1.2-rc.1` through Add-ons. Device readback matched its package/version and native-module hash. Tests used the separate test profile; normal game-memory writes and equipment actions are not part of the companion.

| Check | Current-candidate observation | Limit |
| --- | --- | --- |
| Rendering | Actual 1240 × 1080 Character and Skills pages rendered on the lower screen | Captures establish these observed states, not all text/language cases |
| Startup | Title-screen status was clear; level, stats and skills were unavailable | Other failures/process-death behavior remain untested |
| Level | Barbarian level 2 matched Inventory and returned after a second fresh enabled launch; ordinary hero selection to Wizard matched level 1 | Only these early-level non-seasonal characters were checked |
| Attacks per second | Both characters matched 1.20; Barbarian axe removal/restoration matched 1.20 → 1.00 → 1.20 in Character Details | Axe restored; broader equipment/value coverage pending |
| Cooldown reduction | 0.00% matched Character Details on Barbarian and Wizard; Barbarian value repeated after the second fresh enabled launch | Nonzero reduction and broader character coverage pending |
| Armor | Barbarian 31 and Wizard 16 matched Character Details; Barbarian value repeated after the second fresh enabled launch | Follow-up: shield removal/restoration matched 31 → 22 → 31 in Character Details and the companion; shield restored |
| Skills/runes | Bash, Hammer of the Ancients and Magic Missile/no-rune matched Skills; Wizard's five empty/locked slots matched. Clearing/restoring Hammer (row 2) and Magic Missile (row 1) through the game changed each to Unassigned and back | Three names and no-rune states only; populated-rune and broader skill coverage remain pending |
| Movement bonus | +0% matched the current menus on both classes and repeated after the second fresh enabled Barbarian launch | Nonzero movement pending |
| Navigation/input | Both page actions were logged and captured; physical +/Y and ordinary skill clear/restore worked. Lower Skills-page navigation while the game's Skills menu was open left its selection unchanged | Full controller/input-routing coverage pending |
| Normal quit | Return to the main menu cleared level, all four stats and all six skill rows | Broader travel/loading and process-death checks remain pending |
| Town/inn travel | Ordinary A-button door interaction New Tristram → The Slaughtered Calf Inn → New Tristram; both pages retained Barbarian level 2, attacks per second 1.20, cooldown reduction 0.00%, armor 31, movement +0% and Bash/Hammer no-rune at the destinations | First captured sample had already arrived; brief loading/unavailable interval not observed. One town/inn route does not establish dungeon/waypoint travel coverage |
| Death/resurrection | Follow-up on the unchanged released candidate: the normal level-2 Barbarian died to Risen in The Slaughtered Calf Inn. The character sheet stayed visible during the death screen. Resurrect in Town returned to New Tristram; level 2, attacks per second 1.20, cooldown reduction 0.00%, armor 31 and movement +0% then matched the game menus | One softcore death and town resurrection only; corpse/checkpoint resurrection, Hardcore and process termination remain untested. The earlier Wizard attempt produced no death |
| Suspend/resume | After 108.47 seconds screen-off, both displays were black. Eden was paused on wake with the lower display black; Resume restored Wizard level 1, attacks per second 1.20, cooldown reduction 0.00%, armor 16, movement +0% and Magic Missile | One bounded suspend test; long sleep/process-death behavior remains untested |

Public companion-only captures: [Character](images/live-candidate-character-thor.png), [Skills](images/live-candidate-skills-thor.png), [startup](images/live-candidate-startup-thor.png). Game-menu comparison and quit-clearing captures are retained privately. No game artwork or save data is required to build the public package.

### Short stationary presentation checks

Two windows with the companion disabled and two with it enabled measured presentation cadence at the same fresh-spawn New Tristram waypoint with the level-2 Barbarian stationary. Scene captures confirmed the same position and facing. These were repeated windows within one launch per condition, not four independent launches. An initial enabled window at a slightly different position measured 52.874 events/second and was excluded from the controlled comparison.

| Condition/window | Presentation events/second | 95th-percentile presentation interval |
| --- | ---: | ---: |
| Disabled 1 | 51.2177 | 33.375 ms |
| Disabled 2 | 52.1460 | 33.376 ms |
| Enabled 2 | 51.5668 | 33.382 ms |
| Enabled 3 | 50.5926 | 33.376 ms |

Each window covered about 32 seconds of wall time and 31.5–31.9 seconds of presentation timestamps. The collector polled SurfaceFlinger's actual-present timestamps every 500 ms, deduplicated overlapping 128-entry ring buffers and excluded the pre-window buffer. The metric counts presentation events; it does not identify unique guest-rendered frames.

Settings were Eden Duo 1.1.0/runtime 18 on the Android 13 Thor, NCE, Turnip Adreno T30, frame generation disabled, 100% speed limit, touch overlay off and USB charging. Enabled windows used the Character page. Before the enabled repeat, the battery read 91% and 35.0°C with thermal status 0; the thermal HAL was unavailable. These readings were not paired thermal controls.

No obvious large cadence regression appeared in these short stationary windows. They do **not** establish zero overhead, sustained-combat performance, unique game FPS, CPU use or battery impact. Raw performance records and scene-comparison captures are retained privately; sustained gameplay measurement remains release work.

### Release limits

Nonzero cooldown/movement coverage, broader physical-input testing, broader travel/loading, process-death behavior and sustained gameplay measurements remain release work. The current Barbarian values returned after a second fresh enabled launch; the short presentation checks above do not close the performance requirement. The static root metadata still has `supported_builds: []`, `live_data_available: false` and `verified_on_thor: false`; these flags describe the static preview, not a denial of the narrow live observations above. No stable-support flags have been enabled for the candidate.

## Historical evidence — 2026-10-06

The observations below concern earlier artifacts. They are not validation of the current candidate. After the accidental local folder deletion, all 45 files of private `0.0.10-research` source/assets were recovered with matching recorded hashes; complete earlier private evidence was not recovered.

### Static `0.1.0-dev` and early probes

The original static package contained only `package.json` and its manifest; **37 synthetic tests** passed at that stage. Its archive SHA-256 was `feb78f3399c83c71dc770480906e5792e26d3596892f663ae7240625ab05218d`. Tooling/static CI passed at commit `c96a0575e177c1cc54062a3705e1360aec18cd88` ([historical run](https://github.com/weeknds/diablo3-duo/actions/runs/37498853820)). Those counts, contents and hash do not describe the current packages.

The static package installed and rendered at the game title screen; the [historical capture](images/development-preview-thor.png) contains only the companion UI. Separate private level probes matched a level-1 Barbarian through two fresh launches, the owner's physical-control level-up to 2, and a fresh level-1 Wizard. Startup/main-menu/hero-selection states cleared the level. These established the original single-value milestone, not the current candidate's full release criteria.

### Private `0.0.8-research`

The earlier combined prototype matched Wizard level 1, Magic Missile with no rune, attacks per second 1.20 and armor 16. It also matched Barbarian level 2, Bash and Hammer of the Ancients with no runes, attacks per second 1.20 and armor 31. Earlier slot-clear/restore comparisons correlated those three skill names.

Removing/restoring the Barbarian's weapon changed attacks per second 1.20 → 1.00 → 1.20, and its shield changed armor 31 → 22 → 31, matching Character Details. Equipment was restored afterward. Movement matched only at +0%; cooldown reduction was unavailable in that version when the cached key was absent. The current candidate's reviewed runtime-default path is a later change, with its fresh zero-cooldown comparison recorded above.

That prototype cleared fields at startup/normal quit and recovered Wizard details after an approximately 90-second suspend and Eden's Resume action. A lower-display tap/swipe left the upper Skills selection unchanged. Complete physical-input, death/travel and performance coverage was not established.

Earlier synthetic sanitizer tests and review covered identity/ABI gates, bounded reads, stale-state clearing, field failures and the font interface. Their old reader budgets do not describe the later name lookup; current limits are in [reader architecture](reader.md). Historical research observations do not extend supported levels, classes, modes, languages or builds.
