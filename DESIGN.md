---
name: Diablo III Duo
description: Native dark fantasy companion design preview
colors:
  canvas: "#11110E"
  panel: "#191813"
  selected: "#242117"
  rule: "#49412E"
  gold: "#D7B574"
  text: "#F1EBDD"
  muted: "#BDB4A0"
typography:
  display:
    fontFamily: Cinzel
    fontSize: 42px
  brand:
    fontFamily: Cinzel
    fontSize: 29px
  body:
    fontFamily: Duo Sans
spacing:
  content-margin: 44px
---

# Diablo III Duo visual system

## Overview

The owner's four October 7 references establish the warm black, antique gold
and serif visual direction. The native layout centers statistics and skills.
The owner rejected all character imagery; no portrait or replacement art is used.

## Colors

Canvas `#11110E`; panels `#191813`; raised selected surface `#242117`;
rules `#49412E`; antique gold `#D7B574`; text `#F1EBDD`;
secondary text `#BDB4A0`. No health/resource bars or animated decoration.

## Typography

Cinzel headings (rasterized from the included OFL source), with Duo Sans,
an ASCII bitmap derivative of Source Sans 3, for native labels. Main headings
42 px, brand 29 px. Native body cap heights 20–25 px; principal statistics
use 50 px cap height, with a 40 px minimum for longer values.
The native engine scales cap height as five times `text_scale`.

## Static preview layout

1240 × 1080 logical canvas; 44 px content margins. Compact 84 px brand bar,
100 px bottom navigation. Character places level beside the heading, four
statistics in a full-width 2 × 2 sheet and six numbered equipped skills in
two columns below. Combat uses two columns of three wide skill/rune rows.
Map presents a compact, centered unavailable state without a large bordered
panel. Bottom icon/label groups are centered using the font metrics.

## Static preview components

Bottom-tab touch regions are 413, 413 and 414 px wide by 98 px high.
The View skills link is 228 × 88 px. Page-only actions change between Combat, Map
and Character. `nav:false` disables companion controller navigation; game input
routing remains unverified for this layout. Gold denotes the current page. Unavailable data uses `--`, never
a plausible zero. No active buff countdowns or item comparison controls exist.

## Live candidate adaptation

The installed `0.1.2-rc.1` candidate uses two pages, Character and Skills, with
620 px-wide bottom targets. Level, statistics and skill rows bind to the native
reader; missing values display `Unavailable`. Numeric text may shrink to 20 px
cap height for the longest bounded values. The Map tab is omitted. A visible
`DEVICE TEST` badge distinguishes the candidate from a verified working release.
Both page actions and the layout have been observed on the Thor; see the current
[validation record](docs/validation.md) for input and field coverage.

## Assets and evidence

No character imagery is used. Navigation icons are original code-drawn graphics;
Character uses a document glyph. Skill rows use numbers rather than decorative
icons. Typography and asset provenance live in `design/ASSETS.md`. The package
asset allowlist records exact hashes. Offline sample and stress renders are
visibly labeled; their values are never written to the installable manifest.

## 0.2.1 map motion adaptation

Keep the existing journal palette and layout. The map now uses a native moving
camera over cached, finely outlined terrain, with a small cream position marker
and gold pin. No character illustration. Camera motion, pins and zoom must never
force a new terrain bitmap. See docs/ui-spec.md for the runtime constraints and
docs/validation.md for the separate movement evidence.
