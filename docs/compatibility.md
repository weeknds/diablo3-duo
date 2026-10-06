# Compatibility

**There are no supported live-data builds.** Version `0.1.0-dev` displays static
development text and cannot supply live values on any build. Its installation
and captured lower-screen layout were verified on a Thor on 2026-10-06.

| Item | Current evidence |
| --- | --- |
| Intended game | Diablo III: Eternal Collection, Nintendo Switch |
| Target title ID | `01001B300B9BE000`; confirmed in Eden Duo Info |
| Game display version | `2.7.7.92380`, enabled update and game title screen observed |
| Full executable build ID | `2607A74F5DF7754CC0357B5DF7E496931355D8CA000000000000000000000000` (captured, reader unverified) |
| Runtime build key | `2607A74F5DF7754C` |
| Supported builds | None |
| Gameplay observed | Fresh non-seasonal Barbarian in a separate test profile; owner confirmation and device capture |
| Verified companion field/class/mode coverage | None |
| Health, resource, progression, skills, inventory, map | Unavailable |
| Package runtime declaration | Minimum runtime 18 |
| Installed Eden Duo / runtime version | Eden Duo 1.1.0 observed; runtime 18 confirmed in upstream source |
| Device | AYN Thor / Android 13 confirmed by ADB; Black Max / 1 TB is owner-specified |
| Package installation and layout on hardware | Static `0.1.0-dev` installed through Add-ons and rendered on the lower screen at the game title screen |
| Gameplay input routing with the companion | Unverified |
| Performance on hardware | Unmeasured |

The package currently contains only static text and shapes. It makes no memory
reads or writes and has no controller or touch actions. Its title-level targeting
does not imply build-level support.

The full updated executable was captured privately and hash-checked against the
device copy. This supplies an exact research target, not a verified reader.
The production flags remain `supported_builds: []`, `live_data_available: false`
and `verified_on_thor: false`: installation and static rendering alone do not
meet the live-functionality validation requirement.

See the [original lower-screen capture](images/development-preview-thor.png).
An on-screen frame-rate counter in a separate gameplay capture is not a measured
performance result.

Before adding a supported build, retain a reviewed record with the exact game
display version, complete NSO build ID, runtime build key, Eden Duo release and
runtime, Android backend and driver settings, tested device, class/mode/field
coverage, lifecycle results, and performance observations. Link to dated evidence
without publishing proprietary bytes or identifying save information.

An NSO header establishes an executable identifier, not a title or game version.
An NSP's optional CNMT XML is unauthenticated advisory metadata. Neither alone
establishes what update is running or whether a reader works. See
[validation](validation.md) and [the hardware protocol](device-validation.md).
