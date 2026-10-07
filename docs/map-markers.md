# Map markers — 0.2.2-dev candidate

Implemented offline for the same exact executable as 0.2.1. The Thor was
disconnected for charging during this work. These new readers, touch selection
and their runtime cost have **not been checked on the device**.

## Behavior

- Terrain retains Diablo's exploration masks. Entering a new world clears the
  previous map, marker selection, pin and goblin-alert history.
- Gold quest symbols identify game-provided destinations. Gold arches identify
  entrances/exits; teal diamonds identify waypoints. Labels describe the marker
  kind; localized location and quest names are not read yet.
- Tap an icon to select it, or use **Next marker** to cycle through known targets,
  including those outside the current view. Tap the same icon again to deselect.
  The arrow and text describe its direction **from your character**, so manual
  map panning does not change their meaning. They do not move the game character.
- Green loot symbols identify observed treasure-goblin actors. A newly observed
  identity produces a six-second message. A missing or rejected observation
  removes its marker and alert. The last 128 identities per world are remembered
  to prevent a repeated notification on every scan.
- Markers update on the existing four-per-second map acquisition cadence. Player
  following keeps its existing fast position path. Marker selection/movement
  neither rebuilds terrain images nor recenters the camera.

Only known, explored positions are eligible. There is no full-map reveal, remote
goblin search, guessed quest position, guest-memory write or guest function call.
Directions are straight bearings, not a walkable route through walls.

## Supported data and limits

Quest/location classification compares the game's resolved texture settings for
Destination, Waypoint, DungeonEntrance and DungeonExit against its two objective
tables and generic marker list. It applies world, visibility and distance gates;
the game's far-quest exceptions and unexplored markers are conservatively omitted.

Goblin classification uses the native minimap actor list, generation-checked
client/ACD ownership and loaded Actor/Monster resources. The game's own goblin
notification identifies monster family 11. The reader requires the selected
player's visibility attribute, native suppression/ownership flags and explored
simulation and presentation positions. It does not reproduce every native
icon-style, quest-tag or distance override. It can therefore omit valid goblins;
complete parity with Diablo's icon renderer is not claimed.

Shrines and pylons are **not implemented**: the broad PowerUp category does not
prove either identity. Their reserved icon assets do not make them live features.
Greater Rift progress, remaining time, route finding and localized place names
also remain unavailable.

Both readers bound traversal and reread ownership before accepting a batch.
Quest/location limits are 128 nodes per container, 256 buckets, 900 reads and
65,536 bytes. Goblin limits are 256 actor-list nodes, 16 allocator blocks,
128 classified Actor resources, 9,000 reads and 384,000 bytes. Those are ceilings,
not measured per-frame costs. Missing data or exceeded limits clears that route.
The other optional route can remain visible, with a partial-data message.

At most 64 markers are presented; goblins have priority when combining batches.
These finite limits can omit dense areas. Stable slots retain selection across
list reordering; disappearance clears selection before a slot can be reassigned.

## Local evidence

Focused synthetic reader suites cover exact class resolution, exploration/world
filters, resource ownership, malformed containers, changing data and read failures.
The presentation suite covers stable selection, directions and goblin-alert
lifetime. Module integration and package checks remain separate from hardware
observations recorded in [validation](validation.md).

The offline sample uses original glyphs, actual native layout/font assets and
synthetic terrain rendered by the map code. It is not a live-game screenshot.
