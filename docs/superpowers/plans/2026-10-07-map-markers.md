# Map markers implementation plan

Approved scope: quest objectives, named kinds of locations, nearby goblin alerts,
and shrine/pylon markers wherever the exact game build supplies identifiable data.
Preserve exploration, the existing map camera, the journal design and game input.
The owner requested implementation while the Thor is disconnected for charging.

## Architecture and constraints

Read the game-owned minimap marker list with a bounded, exact-build reader.
Validate player/world ownership, list links, finite positions and stable identities.
Unknown marker kinds remain omitted. No guest writes, calls, fabricated markers,
full-map reveal, bundled game art or proprietary fixtures.

Keep dynamic points separate from immutable terrain images. A presentation helper
filters markers against exploration, keeps a stable selected point, and provides
short labels, player-relative direction and a brief alert for a newly seen goblin.
Original small glyphs inherit the warm-black/gold palette. Extra colors and shapes
identify marker types. Every drawing comes from a validated current-world point.

## Deliverables

- [x] Derive marker list fields and category/visibility meanings from the local
  executable; retain evidence privately and document the reader contract publicly.
- [x] Implement `markers_probe.c/.h` and synthetic malformed/changed-list cases.
  Limit output to 64 points and bound every memory read and list traversal.
- [x] Implement `markers_view.c/.h`, native marker bindings and original glyphs.
  Keep fog intact; hide stale points; never revise terrain for marker movement.
  Preserve Center, pin, clear-pin and zoom controls; add a next-marker control.
- [x] Run focused reader/presentation/module/package checks, inspect one combined
  offline render, compile and package an explicitly device-unverified preview.
  Record the final artifact and exact remaining Thor check without requesting
  reconnection while the device is charging.

## Review focus

- Unexplored, wrong-world and unknown points must not become visible.
- Reused nodes, changed links and failed reads must not leak stale identities.
- Goblin alerts must clear on disappearance/travel and avoid repeating every tick.
- Reordered lists must retain the selected identity, not switch to another point.
- Marker updates must not invalidate terrain or change the camera unexpectedly.

Prior device evidence describes 0.2.1 only. This change cannot claim hardware
support until the newly built reader and marker behavior are observed on the Thor.

Delivered quest, entrance/exit and waypoint classification plus a separate
`goblins_probe.c/.h` resident-resource reader. Shrine/pylon semantics were not
proven and remain omitted. Fresh review accepted the offline layout/spec; the
documenter preserved the incumbent design system. Exact artifact and remaining
device check are recorded in `docs/validation.md`.
