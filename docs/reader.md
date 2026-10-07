# Native reader architecture

This describes the **0.2.0-dev** native companion source. It is a development
preview with separate Character, Equipment, Skills and Map views. The
[native build guide](../native/README.md) covers reproduction;
[validation](validation.md) records acceptance evidence and remaining checks.
The architecture and bounds below do not establish hardware compatibility or
complete coverage of the game's data.

## Exact build and access boundary

[module.c](../native/src/module.c) accepts title `01001B300B9BE000` and this
complete 32-byte build ID:

```text
2607A74F5DF7754CC0357B5DF7E496931355D8CA000000000000000000000000
```

The module checks the host ABI version, structure size and hash, required
callbacks, extension support and the no-tick-when-hidden capability. Creation
rejects a mismatch. Each sample checks the host and game identity again; a
rejected instance remains rejected until recreated. The native package also
restricts its module to this build and disables hidden ticking. The older static
preview's project settings are a separate build path.

Guest access uses only the host's `is_mapped` and `read_memory` callbacks.
[probe_internal.h](../native/src/probe_internal.h) checks arithmetic overflow,
alignment, mapping and read budgets before copying. Readers do not call guest
functions, modify game memory or request a guest mailbox. Equipment inspection,
zoom and map pins affect the companion only.

## Player identity and character details

[details_probe.c](../native/src/details_probe.c) reads the character block, six
stored skill selections, ten statistics and resident skill names. Each stage
works against the same selected-player identity. The common snapshot requires
exactly one selected local player and checks its client, player, actor and pool
ownership. Readers compare repeated observations and recheck their owners before
acceptance. These checks detect observed changes; they are not an atomic snapshot
of a running game.

The character block provides level, class and Paragon level. Level must be 1–70;
class and Paragon have their own range checks and availability flags. An invalid
class or Paragon value does not substitute a guessed label or number.

The statistics are:

| Group | Fields |
| --- | --- |
| Primary attributes | Strength, Dexterity, Intelligence, Vitality |
| Combat details | Attacks per second, armor, critical chance |
| Bonuses | Cooldown reduction, primary-resource cost reduction, movement speed bonus |

[stats_probe.c](../native/src/stats_probe.c) compares two observations of the
attribute cache and its ownership. Dirty caches, cycles and chains beyond eight
nodes are unavailable. Critical chance combines the researched inputs using the
native menu formula, including its cap and clamp. Resource-cost reduction uses
the character's observed primary-resource selector; an unknown selector does not
choose an assumed resource.

For cooldown reduction, critical-chance inputs and resource-cost reduction, a
missing cache entry can use the game's current registered default only after a
terminal null proves absence. An unreadable or overlong chain does not prove
absence. The registration identity and repeated raw value must match. No literal
zero is substituted. Formatting rejects nonfinite values, unsupported ranges and
output-buffer overflow.

The six skill slots retain the game's stored order; controller-button mapping is
not established. Empty slots show **Unassigned**. A selection with no rune shows
**No rune selected**; other rune names remain **Rune unavailable**. Passives and
active-buff timers are not read.

[names_probe.c](../native/src/names_probe.c) resolves skill labels from already
resident string resources. It validates descriptors, resource ownership, hash
nodes, language/override gates and repeated text. The supported route requires
an empty override bucket and plain, terminated printable ASCII without markup
delimiters. Text must fit the 512-byte buffer and the presentation check.
Unsupported languages, missing resources or unsupported lookup paths remain
unavailable; the reader does not load resources into the game.

## Equipment occupancy and inspection

[equipment_probe.c](../native/src/equipment_probe.c) reads 13 body slots: head,
torso, main hand, off-hand, hands, waist, feet, shoulders, legs, bracers, right ring,
left ring and neck. Each slot is read twice through the selected player's
inventory. An occupied slot must resolve to the same item handle and item owner.
**Empty slot** requires the actual empty-cell value. A missing slot key, failed
lookup or changed item is **Unavailable**, not empty.

Raw slot keys remain 1–13 in the native enumeration. Presentation maps the
main-hand row to source index 3/key 4 and the off-hand row to index 2/key 3,
matching the observed axe/shield Inventory comparison on the target build.

The ledger presents occupancy as **Equipped item** or **Empty slot**. Selecting a
slot requests [equipment_inspect.c](../native/src/equipment_inspect.c), which
revalidates that equipped item and attempts its resident localized **base item
name**. It checks item identification, catalog ownership, resource identity,
loaded/fallback state and repeated text. An unidentified item does not reveal a
name. The same plain-ASCII and 512-byte text bounds apply.

This is not full item inspection: rolled affixes, generated rare names, item
artwork, weapon comparisons and equip actions are not implemented. An available
occupancy result can coexist with an unavailable base name.

## Exploration and terrain

[map_probe.c](../native/src/map_probe.c) reads the player's world position and
checks that the character's world matches the visible world. Exploration comes
from the game's minimap coverage masks. The reader preserves their unseen,
partially revealed and fully revealed states. Coverage rectangles alone do not
describe floors, walls or traversable routes; a fully revealed town tile can be
a large rectangle.

[nav_probe.c](../native/src/nav_probe.c) optionally adds terrain from resident
scene navigation resources. It copies numeric ground-cell grids with their world
transforms. It validates scene/world ownership, resource handles and SNO identity,
loaded/fallback flags, grid headers, dimensions, crops, cell data and repeated
ownership. Resource reference counts and resident last-used ticks are treated as
volatile bookkeeping rather than identity.

Supported terrain uses 2.5-unit cells and planar scene transforms. The bounded
reader omits parent scenes with unresolved child overrides, non-planar transforms
and other unsupported scene data. A result may therefore contain only fragments
of an area. The UI identifies partial terrain; omitted areas are not inferred to
be blocked or filled with invented geometry. Read failures or observed ownership
changes discard the terrain sample.

[map_view.c](../native/src/map_view.c) projects those owned numeric grids into a
map image and clips terrain pixels against exploration coverage in the same
world. It does not assume that the terrain and minimap scene identifiers are
equivalent. Unseen coverage stays hidden, and partial coverage retains its darker
shade. If terrain is unavailable, the map can still show exploration coverage;
if coverage is unavailable, a valid player position can still be shown.

Terrain describes static navigation ground. It does not include doors, moving
objects, pylons, exits, rift progress or rift timers. There are no bundled game
textures or character pictures. Zoom, recenter and a single pin at the current
position are local presentation actions. The pin clears when the world or player
context is lost or changes.

## Sampling, caches and image workers

Character details run on every host `sample()` call. Equipment occupancy,
selected-item inspection and exploration normally refresh every **15 runtime
ticks**, approximately **4 Hz** at the nominal 60 Hz runtime cadence. Terrain
normally refreshes every **60 ticks**, approximately **1 Hz**. First acquisition,
owner/world changes and equipment-selection actions can bring work forward;
these intervals are scheduling rules rather than wall-clock guarantees.

The module keeps accepted equipment/text results, numeric map data and an owner
identity token between refreshes. Guest-address traversal remains inside the
readers; the token is used to detect a different owner. Every sample with a
visible cached map checks the current world again, and every accepted details
sample closes with another selected-player check. World failure clears the map,
terrain and pin and schedules a fresh acquisition. Player failure clears the
optional caches and prevents earlier character details from being published.

Map updates, actions and snapshot copying share a mutex. The separate image
worker copies the current `MapView` under that mutex and renders its owned data
after releasing the lock. It performs no guest-memory reads. The RGBA image is
1,152 × 580 pixels. Image-content changes advance a revision; read counters and
diagnostic strings alone do not.

Images use keys of the form `module:exploration:<revision>`. The callback checks
the requested revision against the current map before taking its copy. An
already obsolete revision returns a successful transparent 1 × 1 image rather
than an earlier world's data or a failed-key cache entry. Malformed keys and
render/allocation failures are rejected.

## Work bounds

These are the current per-call ceilings from the linked headers, not measured
latency or performance guarantees. All traversals also have explicit structural
limits. The name and inspection readers charge mapping attempts to their budgets;
the common reader counts mapped copy attempts.

| Reader | Read budget | Byte budget |
| --- | ---: | ---: |
| [Player](../native/src/player_probe.h) | 28 | 232 |
| [Skills](../native/src/skills_probe.h) | 51 | 408 |
| [Statistics](../native/src/stats_probe.h) | 352 | 4,826 |
| [Skill names](../native/src/names_probe.h) | 298 | 13,200 |
| [Equipment occupancy](../native/src/equipment_probe.h) | 520 | 7,200 |
| [Selected-item inspection](../native/src/equipment_inspect.h) | 600 | 30,000 |
| [Position and exploration](../native/src/map_probe.h) | 600 | 60,000 |
| [Current-world recheck](../native/src/map_probe.h) | 64 | 1,024 |
| [Terrain navigation](../native/src/nav_probe.h) | 8,192 | 700,000 |

The details aggregate is **729 reads / 18,666 bytes**. The module's closing player
check uses another player budget. A sample running every stage, including selected
inspection, terrain and the world/closing checks, has a source-budget sum of
**10,733 reads / 817,122 bytes**. Most samples do not run every optional stage.

Exploration is limited to 128 list nodes, 64 retained tiles and at most 32 × 32
two-bit cells per tile. Terrain scans at most 1,024 scene slots and retains at
most 64 grids with 65,536 total cells in one 8,192-byte ground-bit arena. Individual
terrain reads are at most 256 bytes. Budget reservations leave room for final
ownership checks; a limit is an unavailable result, not permission to skip them.

## Unavailable states and maintenance

A changed or unreadable shared identity invalidates the affected reader. The
details aggregate clears all its fields on a shared-player failure; an isolated
field or text failure can leave independently accepted fields visible. Optional
readers clear failed observations instead of publishing partial payloads. Valid
terrain grids may be retained when other scenes are explicitly unsupported, with
the result marked partial.

With a valid host API, each sample replaces every published row, including
unavailable states, between `begin_output()` and `end_output()`. If the host ABI
itself is invalid, the module stops calling it. UI readiness and error gates hide
live bindings when the module cannot supply them.

A new game build needs fresh identity and layout evidence, focused synthetic
tests and the separate acceptance checks before expanding the build gate. Run
`python3 native/test.py`; the optional `--ndk` path also checks native reproduction.
Review source changes before updating `native/SOURCES.json`, module hashes or
package pins. Keep private research material outside the package and public
documentation, and do not weaken a failure check simply to make data appear.
