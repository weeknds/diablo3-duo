# Native design preview

`0.1.1-dev` is a static, three-page Eden Duo companion. The October 7 redesign
uses the owner's dark fantasy references: warm black, antique gold and cream
serif headings, with no character imagery. It has not been installed or tested on
the Thor. [PRODUCT.md](../PRODUCT.md) records purpose and constraints;
[DESIGN.md](../DESIGN.md) records the visual system.

## Product direction

The owner selected **build and character details** to reduce visits to the game's
menus. Equipped skills, selected runes, passives and useful statistics remain the
intended live direction. A duplicated health/resource display is not part of this
layout. Critical chance, resistances and additional bonuses are investigation
priorities, not shipped integrations.

Earlier private research has narrow menu-comparison evidence for level, three
skill names, attack speed and armor, plus movement at +0%. Those private readers
are not included here. Only independently verified fields may enter a future
live package. Equipment changes and other game actions remain a later milestone.

## Pages and layout

The native manifest uses a 1240 × 1080 logical canvas, 44 px content margins,
an 84 px brand bar and 100 px bottom-navigation region. Character opens first.
Every page shows **DESIGN PREVIEW** and **Live game data is not connected**.

| Page | Current presentation |
| --- | --- |
| Character | Level beside the heading; attacks per second, cooldown reduction, armor and movement bonus in a full-width 2 × 2 sheet. Statistics are `--`. Six numbered unavailable skills appear below in two columns. View skills opens Combat. |
| Combat | Two columns of three wide numbered skill/rune rows, followed by an unavailable runes/effects message. |
| Map | Compact centered unavailable state with a plain explanation that map support is not included. |

No portraits, figures or replacement character artwork are used. There are no
equipment comparison controls or buff timers.
Cinzel heading images and the Duo Sans ASCII bitmap font provide typography;
original code-drawn pictograms identify pages. Character uses a document glyph;
skill rows use numbers. Bottom icon/label groups are centered using font metrics. OFL licenses and notices
travel with the package. See [asset provenance](../design/ASSETS.md).

## Behavior

The manifest declares runtime 18, `requires_module: false` and `nav: false`.
Its only actions have `kind: page`; the three tabs and Character's skills link
select local companion pages. Bottom tap regions are 413, 413 and 414 px wide
by 98 px high; the View skills link is 228 × 88 px. These are manifest dimensions,
not verified device touch behavior. Controller navigation is disabled in the
manifest; preservation of normal gameplay input still needs device testing.

All content uses static labels, rectangles and packaged images. There are no
memory reads, writes, derived game values, game-button injection or live binds.
Unavailable values remain unavailable in every game state and executable build.

## Desktop previews and evidence

`python3 tools/render_preview.py` draws the actual manifest with its packaged
font atlas and images. The renderer approximates native drawing on the desktop;
its output is not a device capture. `--sample` substitutes illustrative labels
only while rendering PNGs, with a visible sample-data message. `--stress` checks
long illustrative names and values offline. Neither mode changes the installable
manifest or establishes game accuracy or coverage.

The earlier single-page `0.1.0-dev` package was installed and rendered on the
Thor on 2026-10-06; its [original capture](images/development-preview-thor.png)
remains historical evidence for that version only. Fresh installation, font and
image rendering, tab hit testing, gameplay input and performance checks are
pending for this redesign. See [next-step.md](next-step.md).
