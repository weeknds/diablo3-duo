# Diablo III Duo

<!-- impeccable:product-schema 1 -->

## Platform

android

An Eden Duo companion on the AYN Thor's 1240 × 1080 lower display. The native
canvas and game input rules take precedence over ordinary Android app chrome.

## Users and purpose

The player wants build and character details while playing Diablo III on the
upper display, reducing visits to game menus. The owner selected this purpose
and rejected a duplicated health display.

## Capabilities and constraints

There are two deliberately separate outputs:

- **0.1.1-dev static preview:** Character, Combat and unavailable Map pages;
  built by `tools/build.py`, with no live reader or supported-build declaration.
- **0.2.0-dev native live preview:** Character, Equipment, Skills and Map views,
  built by `native/build.py`. It adds class/Paragon, primary attributes, six
  combat statistics, equipment slots/base names and a numeric exploration map.
  This is an experimental exact-build package, not broad compatibility.

Use the real `.dsmod.zip` format, runtime 18, with bounded read-only acquisition.
The native reader accepts one complete executable build ID; that gate is not a
claim of complete compatibility. Missing or unsupported values remain unavailable.
Equipment rolls/comparison, full affixed item names, rune names, passives, buff
timers and Greater Rift/pylon/exit annotations are not implemented. Map terrain
can be partial for unsupported scenes and excludes dynamic doors/actors. Sample values belong only in labeled offline design renders.

## Brand commitments

English. The owner's four dark fantasy references establish warm black, antique
gold, cream serif headings, clear statistics and bottom navigation. Use no
character portraits, figures or replacement character art. Typography and
navigation graphics must be original or redistributable.

## Evidence on hand

Dated hardware observations, exact artifact hashes and limits are maintained in
[validation](docs/validation.md). The earlier 0.1.2 candidate established narrow
Barbarian/Wizard field and lifecycle evidence. New equipment/terrain readers
require their own evidence; old observations never stand in for a new feature.
Offline renders are design evidence, not device proof.

## Principles

- Show information that saves a menu visit.
- Keep missing information visibly unavailable.
- Preserve main-screen game controls.
- Keep live acquisition independent from presentation.
