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
- **0.1.2-rc.1 native test candidate:** Character and Skills pages with live level,
  four statistics and six equipped skill slots, built by `native/build.py`.
  The reader and toolchain were recovered, the binary was reproduced, and this
  redesigned candidate has been installed on the Thor.

Use the real `.dsmod.zip` format, runtime 18, with bounded read-only acquisition.
The native reader accepts one complete executable build ID; that gate is not a
claim of complete compatibility. Missing or unsupported values remain unavailable.
Map data, equipment comparison, rune names, passives and buff timers are not
implemented. Sample values belong only in labeled offline design renders.

## Brand commitments

English. The owner's four dark fantasy references establish warm black, antique
gold, cream serif headings, clear statistics and bottom navigation. Use no
character portraits, figures or replacement character art. Typography and
navigation graphics must be original or redistributable.

## Evidence on hand

Fresh 2026-10-07 candidate observations include native module/asset loading,
Barbarian level and armor matching menus, two live skill names, page navigation,
and clearing values after returning to the game menu. The owner used physical
controls to enter gameplay and open menus. These are bounded observations;
[validation](docs/validation.md) tracks remaining lifecycle, field, controller
and performance evidence. Offline renders are design evidence, not device proof.

## Principles

- Show information that saves a menu visit.
- Keep missing information visibly unavailable.
- Preserve main-screen game controls.
- Keep live acquisition independent from presentation.
