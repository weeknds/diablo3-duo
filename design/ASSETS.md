# Design asset provenance

The supplied reference screenshots inform the visual style; no screenshot crop,
game texture, game font or extracted Blizzard art is shipped.

No character imagery is used. The package contains no portrait, figure or
replacement character artwork.

## Typography and icons

Source Sans 3 Regular and Cinzel are included under OFL-1.1; the original source
URLs and SHA256 hashes are in `fonts/sources.json`. Original notices are in
`fonts/`; both licenses accompany the package. The modified ASCII bitmap font
is named **Duo Sans**, avoiding Source Sans's reserved name. Its original metrics
use Eden's built-in MFNT parser layout, with no commercial font container copied.
The parser was inspected at Eden commit
`5410366938b8aa0aca2c9c61f4e93038b71db90c`, `src/core/mods/engine_mercury.cpp`.

`tools/design_layout.py` creates heading images, original line pictograms,
the bitmap font and native manifest. Character uses a document navigation glyph;
skill rows contain no decorative icons. `package-assets.json` inventories the
14 shipped assets by SHA256: referenced images/fonts plus their licenses and notices. Rebuilding the package needs only standard Python;
regenerating assets and desktop renders additionally needs Pillow.

## Preview scope

`tools/render_preview.py` draws the actual manifest with its atlas and image
assets. This is a desktop approximation of Eden's renderer, not a Thor capture.
Its optional `--sample` mode renders illustrative labels; `--stress` checks longer
names and values. Both write visibly labeled offline PNGs without changing the
manifest or reading game data.
