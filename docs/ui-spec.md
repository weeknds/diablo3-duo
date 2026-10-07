# Native live interface

The `0.2.0-dev` native preview uses a 1240 × 1080 canvas for the Thor's lower
display. The owner's reference sets its warm black, antique gold and cream
palette. Cinzel headings, a readable Duo Sans bitmap font and original line icons
carry the theme without character artwork or a duplicate health/resource HUD.

Character opens first. Three persistent bottom tabs select Character, Map and
Skills; Equipment is a nested Character view with its own return button.

| View | Content and interaction |
| --- | --- |
| Character | Class, level and Paragon; a four-column primary-attribute strip; six combat statistics in two columns. Equipment opens the slot sheet. |
| Equipment | Thirteen labeled slots in two columns. Each reports occupied, empty or unavailable. Tapping a slot opens an inline base-name inspection panel. It does not change equipment. |
| Skills | Six numbered equipped-skill rows. Empty slots say Unassigned. Known no-rune states and unavailable rune names are distinguished. |
| Map | Explored coverage, supported terrain and player marker. Controls zoom, center on the player, pin the current location and clear that pin. World changes discard the pin. |

The image on Map is rendered from copied numeric observations. It contains no
extracted game textures. Partial terrain is stated in the footer; dynamic doors,
enemies, pylon/exit labels and Greater Rift timing/progress are not provided.

All pages carry **LIVE PREVIEW**. Module readiness/error gates replace values with
unavailable states; startup does not display sample data. Equipment base names
are explicitly labeled, rather than presented as complete affixed names or item
rolls. Critical damage and sheet damage/toughness/recovery are not in this reader.

The live manifest disables controller navigation. Touch actions either change
companion pages, select a read-only inspection or change the companion's local
map view. They perform no game actions. The native image worker receives an
owned numeric snapshot under a mutex; it does not read guest memory.

Content uses 44 px side margins, an 84 px brand bar and a 98 px bottom-navigation
region. Equipment rows and map controls are 70 px tall. These dimensions describe
the 1240 × 1080 target, not support for other display classes.

See [validation](validation.md), [asset provenance](../design/ASSETS.md) and
[the native builder](../native/README.md). The older root `0.1.1-dev` builder
remains a separate static design preview; its offline sample renders and metadata
do not describe the live package.
