# Native reader architecture

This describes the source used by the `0.1.2-rc.1` live test candidate. Build
acceptance and host tests do not establish complete game, language or hardware
compatibility. The [native build guide](../native/README.md) explains reproduction;
[validation](validation.md) records the separate device evidence.

## Identity and data flow

`module.c` accepts title `01001B300B9BE000` and this complete 32-byte build ID:

```text
2607A74F5DF7754CC0357B5DF7E496931355D8CA000000000000000000000000
```

`supports()`, `valid_host()` and `valid_identity()` check the build, ABI version,
structure size/hash, required callbacks and the host's no-tick-when-hidden
capability. `create()` rejects a mismatch. `sample()` rechecks the host and game
identity; a rejected module instance stays rejected until recreated. The manifest
also limits the native module to the exact build and disables hidden ticking.

`details_probe()` starts with `player_probe()`, then reads skills, statistics and
skill names against the same accepted `PlayerProbeIdentity`. `snapshot()` follows
the current client and single-player selector, requires exactly one selected
player, and checks player, actor and pool ownership. Level must be 1–70. The
level read is bracketed by matching snapshots; later stages repeat ownership
checks before accepting their observations. This is a consistency check, not an
atomic snapshot of the running game.

All guest access goes through the host's `is_mapped` and `read_memory` callbacks.
`read_at()`, `read_offset()` and `pointer()` check address arithmetic, alignment,
mapping and budgets before copying into local storage. The reader performs no
guest writes or guest function calls, and retains no game pointers or values
between samples.

## Fields and failure behavior

`skills_probe()` reads six stored power/rune pairs twice and rechecks ownership.
An empty power slot is **Unassigned**; a rune ID of -1 produces **No rune selected**.
Other rune IDs remain **Rune unavailable**. Slot numbers describe stored order,
not a verified controller-button mapping. Passives are not read.

`stats_probe()` brackets two field observations with matching player/ACD/cache
ownership. Dirty caches are unavailable; cache chains have an eight-node limit.
The four field keys and published bindings are:

| Field | Cache key | UI binding |
| --- | --- | --- |
| Attacks per second | `0xFFFFF0C9` | `stats.0.value` |
| Cooldown reduction | `0xFFFFF0D3` | `stats.1.value` |
| Armor | `0xFFFFF026` | `stats.2.value` |
| Movement speed bonus | `0xFFFFF0B9` | `stats.3.value` |

Only cooldown reduction has a missing-cache path. `field_snapshot()` must observe
a terminal null that proves the key absent. A cycle, unreadable node or exhausted
limit does not prove absence. `cdr_default_snapshot()` then reads the current
runtime registration, checks definition ID `0xD3`, and compares the repeated raw
value and lookup path. It does **not** substitute a guessed zero. `stats_format()`
rejects nonfinite values and unsupported formatting ranges; the module's bounded
output buffer rejects overflow instead of truncating a number.

`names_probe()` follows the already resident **PowersStringList** cache through
checked descriptors, arenas, handles and hash-node ownership. `discover()` maps a
power's internal name plus `_name` to cached display text. This bounded route
requires the relevant override bucket to be empty; it does not load missing
resources or implement every lookup path. Text must be terminated, printable
ASCII without markup delimiters, within 512 bytes, and fit the presentation
check. Non-ASCII languages, unsupported cache layouts and rune-name lookup are
not supported. Payloads are reread, and `close_one()` rechecks recorded metadata,
including language/override gates, before names are accepted.

A failed or changed shared player identity makes `details_probe()` clear the
whole result. A field-only failure can leave other independently accepted fields
visible. Changed skill pairs clear skills/names; changed name metadata clears the
affected names. With a valid host API, `sample()` replaces every published row on
every sample, including unavailable states, using `begin_output()`/`end_output()`.
If the host ABI itself is invalid, it stops calling the unsafe API. The UI also
gates live bindings on module readiness and hides them on module errors.

## Bounds and maintenance

The combined ceiling is **498 read attempts and 15,270 requested bytes per
sample**: level 27/212, skills 51/408, statistics 122/1,450, names 298/13,200.
The name stage charges mapping failures to its attempt budget and reserves
137 reads/2,760 bytes for closing checks. These are work bounds, not measured
latency or performance guarantees.

| Source | Maintainer entry points |
| --- | --- |
| [module.c](../native/src/module.c) | `create()`, `sample()`: identity gate and complete output publication |
| [probe_internal.h](../native/src/probe_internal.h) | `read_at()`, `snapshot()`: checked reads and local-player ownership |
| [details_probe.c](../native/src/details_probe.c) | `details_probe()`, `clear_all()`: shared consistency and clearing |
| [skills_probe.c](../native/src/skills_probe.c) | `skills_probe()`: repeated six-slot observations |
| [stats_probe.c](../native/src/stats_probe.c) | `field_snapshot()`, `cdr_default_snapshot()`, `stats_format()` |
| [names_probe.c](../native/src/names_probe.c) | `shared()`, `discover()`, `close_one()`, `names_probe()` |

Offsets, layouts and limits belong to this exact executable. A new game build
needs fresh build identification, evidence for each changed route, synthetic
reader tests and device comparisons before its identity gate or support record
is extended. Run `python3 native/test.py`; use `--ndk` for the separately pinned
native build and archive reproduction. Update `native/SOURCES.json` and binary
pins only after reviewing the corresponding source changes. Do not weaken a
failure check simply to make a value appear.
