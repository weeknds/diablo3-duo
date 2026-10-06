# Development preview UI

This manifest is a static status page for the first Eden Duo engine-load test. It does not display live Diablo III data or provide game controls.

## Accepted product direction — 2026-10-06

The owner selected **build and character details** as the lower display's primary
purpose: reduce trips into Inventory while playing. The intended live layout
prioritises equipped skills, selected runes, passives and useful character
statistics. Critical chance, cooldown reduction, resistances and other selected
bonuses are investigation priorities, not shipped fields.

The owner explicitly rejected a health display because the upper screen already
shows the health bar. Do not reserve permanent lower-screen space for duplicated
HUD bars. Health is an internal test value for validating the attribute reader,
not a planned default feature. Apply the same test of usefulness before adding
any status readout: it should reduce a menu visit or supply useful missing detail.

Only independently verified fields will enter the live interface. Character
level is the first private milestone; skills, passives and statistics remain
unverified. The final supported subset depends on reliable
read-only access and comparison with the game's menus. XP, maps and equipment
interaction are deferred. No equipment-changing controls are planned for the
first read-only release.

Use legible sentence-case text, consistent alignment and original graphics.
Keep diagnostics outside the normal interface. Missing data must clear promptly
and explain its unavailable state without displaying guessed values. Check the
finished layout and input routing on Thor before treating this direction as a
validated interface.

## Layout

The logical canvas is 1240 × 1080 with 72-pixel outer margins. One page places the project name above a prominent **DEVELOPMENT PREVIEW** banner and the message **Live game data is not connected**. A single inset panel lists three planned readouts: health, class resource and equipped skills. Each value is an empty em-dash shape. A final line identifies device validation as the next milestone.

The palette uses off-black (`#11100F`), warm ivory (`#F1E9DC`) and muted crimson (`#AA6F6B`). The design uses the runtime's built-in font, literal text and plain rectangles. It includes no Blizzard artwork, game font, item icons or other external assets. Horizontal rectangles draw the em-dashes so placeholder rendering does not depend on Unicode coverage in the built-in font.

## Behavior

The manifest declares runtime 18 and explicitly sets `nav` to `false`. All widgets are static `label` or `rect` widgets. There are no actions, taps, binds, points, derived values, native modules, game-memory reads, writes or button injection. The readouts remain empty for every game state and executable build.

The package name includes “Development Preview”. The planned readouts are design intent and do not claim that those integrations work. The first engine-load test should check whether the page appears, whether the text is legible and whether normal gameplay input remains unaffected.

## Validation limits

The manifest was checked against the pinned companion `docs/PACKAGE_FORMAT.md` and the declarative shape of upstream examples. Local structural checks verify identity, runtime, canvas, static widget types, documented fields and numeric bounds. Separately, installation and text placement were observed on an AYN Thor with Eden Duo 1.1.0 on 2026-10-06; see the [original lower-screen capture](images/development-preview-thor.png). Controller behavior during gameplay with this package remains unverified. The original preview's next-milestone caption is retained in the captured artifact; current work is recorded in [next-step.md](next-step.md).
