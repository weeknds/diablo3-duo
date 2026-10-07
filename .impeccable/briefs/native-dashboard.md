# Lower-screen dashboard

Mode: Operate. Platform: Eden Duo native canvas on AYN Thor, 1240 x 1080.
Artifact: native/live_ui.py and the read-only native module.

## Direction contract

THESIS: A compact field journal for the current hero: readable character details,
inspectable equipment and a truthful exploration view within one thumb's reach.

OWN-WORLD: Preserve the approved warm-black, antique-gold, cream Cinzel and Duo
Sans system. Fine rules and restrained equipment glyphs; no character imagery.

STORY: Glance at the hero's actual attributes, inspect a slot, or orient around a
saved location while the game remains controlled on the upper display.

FIRST VIEWPORT: Class title with level and Paragon; Equipment action at upper
right. A four-column attribute strip above a two-column combat-stat ledger.
Character, Map and Skills tabs anchor the lower edge. Equipment opens a compact
13-slot ledger and inline detail panel. Map uses live data only.

FORM: Extend the incumbent native journal system and the owner's supplied
reference images. This is code-led work; no generated comp or character art.

INTERACTION: Touch selects a slot and reveals its actual base item name inline.
Map controls change only the companion's view and pin. Values update without
entrance animation; loading and errors clear data rather than preserve stale facts.

FINISH: unreviewed and undocumented is unfinished; this build ends with the finish review, the verdict, DESIGN.md, and every shipping raster carrying its provenance


Current correction: the owner reported flicker, unattractive rectangular terrain,
wrong character tracking and non-fluid motion in 0.2.0. Static captures did not
establish motion quality. 0.2.1 replaces pose-driven rasters with a native map,
client presentation XY, the ordinary minimap projection, constant physical follow
scale and thinner terrain boundaries. Review the actual short movement recording
and controls as well as the final still. A town check is not universal map proof.
