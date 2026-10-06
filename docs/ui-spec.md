# Development preview UI

This manifest is a static status page for the first Eden Duo engine-load test. It does not display live Diablo III data or provide game controls.

## Layout

The logical canvas is 1240 × 1080 with 72-pixel outer margins. One page places the project name above a prominent **DEVELOPMENT PREVIEW** banner and the message **Live game data is not connected**. A single inset panel lists three planned readouts: health, class resource and equipped skills. Each value is an empty em-dash shape. A final line identifies device validation as the next milestone.

The palette uses off-black (`#11100F`), warm ivory (`#F1E9DC`) and muted crimson (`#AA6F6B`). The design uses the runtime's built-in font, literal text and plain rectangles. It includes no Blizzard artwork, game font, item icons or other external assets. Horizontal rectangles draw the em-dashes so placeholder rendering does not depend on Unicode coverage in the built-in font.

## Behavior

The manifest declares runtime 18 and explicitly sets `nav` to `false`. All widgets are static `label` or `rect` widgets. There are no actions, taps, binds, points, derived values, native modules, game-memory reads, writes or button injection. The readouts remain empty for every game state and executable build.

The package name includes “Development Preview”. The planned readouts are design intent and do not claim that those integrations work. The first engine-load test should check whether the page appears, whether the text is legible and whether normal gameplay input remains unaffected.

## Validation limits

The manifest was checked against the pinned companion `docs/PACKAGE_FORMAT.md` and the declarative shape of upstream examples. Local structural checks verify identity, runtime, canvas, static widget types, documented fields and numeric bounds. Separately, installation and text placement were observed on an AYN Thor with Eden Duo 1.1.0 on 2026-10-06; see the [original lower-screen capture](images/development-preview-thor.png). Controller behavior during gameplay with this package remains unverified. The original preview's next-milestone caption is retained in the captured artifact; current work is recorded in [next-step.md](next-step.md).
