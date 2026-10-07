# Character fields: exact-build research, 2026-10-07

These are independently implemented, read-only observations of one executable.
The new fields require a game-menu comparison on the Thor before a release
claims hardware support. Synthetic tests establish reader behavior, not correct
values in a running game. Missing, changing or invalid observations stay
unavailable. No guest function is called, and no guest memory is written.

## Executable identity

- Full NSO build ID: `2607A74F5DF7754CC0357B5DF7E496931355D8CA000000000000000000000000`.
- Private dumped `main` SHA-256: `7f3910c7a15d4bc053cdc8204f1abcac526c57f205c584d1d97779a1a10a98c3`.
- The NSO segments were decompressed locally and checked against their header
  SHA-256 hashes. The executable itself remains in ignored private storage.
- Instruction addresses below are relative to this executable's main image.
  They are factual research references, not portable offsets or a title-ID claim.

## Character identity

The existing single-local-player route still checks the live game type, active
player selection, full player handles, actor pool bounds and actor generation.
The 12-byte block at `Player + 0xD688` contains class, character level and Paragon
level. The block is read twice and bracketed by full ownership observations.
Either block or ownership changing clears the complete character observation.

The class getter at `0x521960` reads `Player + 0xD688`. The message handler at
`0xB7C90` and class resolution at `0x51E078` populate that field. The seven-way
class-name selection at `0x523880` establishes this mapping:

| Value | Class |
| --- | --- |
| 0 | Demon Hunter |
| 1 | Barbarian |
| 2 | Wizard |
| 3 | Witch Doctor |
| 4 | Monk |
| 5 | Crusader |
| 6 | Necromancer |

The hero UI reads `Player + 0xD690` at `0x1ECB3C` and passes it to `0x3684C0`.
That function formats the value into the `.alt_level` UI element when positive.
The `Alt_Level` registration at `0x67C81C` sets the upper limit to 20000 at
`0x67C834`. The reader accepts 0–20000; zero is a valid observed Paragon value,
never a fallback. Invalid class or Paragon ranges make only that optional field
unavailable. Character level retains its existing 1–70 validation.

The complete player probe is bounded to **28 reads and 232 bytes**.

## Menu attributes

The existing ACD ownership, full generation, clean attribute-group state and
direct/shared cache checks are retained. Each input's complete bounded cache
traversal is observed twice. At most eight nodes are followed for any key. An
observed terminal null proves absence; cycles, unreadable nodes and a ninth link
do not. Final ownership validation still runs after individual input failures.

| Output index | Output | Native input |
| --- | --- | --- |
| 0 | Attack speed | `0xFFFFF0C9` |
| 1 | Cooldown reduction | `0xFFFFF0D3` |
| 2 | Armor | `0xFFFFF026` |
| 3 | Movement speed bonus | `0xFFFFF0B9`, minus 1 |
| 4 | Strength | `0xFFFFF00E` |
| 5 | Dexterity | `0xFFFFF00F` |
| 6 | Intelligence | `0xFFFFF010` |
| 7 | Vitality | `0xFFFFF011` |
| 8 | Critical hit chance | Six-input menu formula below |
| 9 | Primary-resource cost reduction | Resource-keyed total below |

The primary-attribute registrations are at `0x67BF34`–`0x67C004`. The Character
Details builder at `0x1C9B40` starts with full key `0xFFFFF00E` and advances through
the four keys. Its float formatting calls request zero decimal places. The
companion applies the already audited ordinary-range nearest-even conversion
used for Armor; values outside that supported formatting range are unavailable.

### Critical hit chance

The Character Details builder at `0x1CAA64` calls `0x7383B0` with argument zero.
That exact function performs the following float operations in order:

1. Add `Crit_Percent_Base` (`0xFC`), `Crit_Percent_Bonus_Capped` (`0xFD`),
   `Weapon_Crit_Chance_CurrentHand` (`0x4B7`) and
   `Crit_Percent_Bonus_Hide_From_DPS` (`0xFF`).
2. Cap the sum at `Crit_Percent_Cap` (`0x100`).
3. Add `Crit_Percent_Bonus_Uncapped` (`0xFE`).
4. Clamp the result to 0–1.

All six use the full key with `0xFFFFF000` as its high bits. The registrations
at `0x67F2F8`–`0x67F3C8` identify the five critical-percent fields; the current-hand
weapon field is registered at `0x6930E8`–`0x693150`. Every dependency must be
coherent and finite. An overflowing intermediate is rejected. The result is a
menu statistic, not a simulation of effective damage.

### Primary-resource cost reduction

The native getter at `0x47AEC0` reads the integer primary-resource enum using
full key `0xFFFFF094`. Character Details obtains that enum at `0x1CC7DC`, then
passes attribute ID `0x2E3` and the enum to `0x47A840` at `0x1CCBA8`.
The key constructor at `0x69AFC0` combines `attributeID | (resource << 12)`.
The registered attribute is `Resource_Cost_Reduction_Percent_Total`, not the
separate global-only modifier. The reader requires a present, coherent enum
between 0 and 8 and observes the corresponding keyed total twice. It does not
guess a resource from character class. Demon Hunter's second resource is not
part of this output.

### Proven absence and runtime registrations

The native float getter at `0x69F680` selects its registered default from
`main + 0x191EAD8 + 64 * attributeID + 4` when the clean cache proves absence.
The extended reader permits this route only for the existing CDR field, the six
critical-chance inputs and the keyed cost total. It reads the registration ID
and value together, validates the ID, checks the main-image extent and mapping,
and requires two identical observations. It never substitutes literal zero.
Primary attributes and the integer primary-resource enum require cache entries.

All 16 inputs plus the final shared ownership check fit a strict budget of
**352 reads and 4826 bytes**. Each field failure stays independent; a shared
identity failure clears every stat. Percentages use two decimal places in the
companion. The primary-resource menu's adaptive decimal presentation is not
reproduced.

## Remaining unsupported fields

Sheet damage (`0x486370`), Toughness (`0x486C00`) and Recovery (`0x4867F0`) are
computed getters. Their complete read-only dependency routes are not
implemented. In particular, the damage path changes a guest attribute while
evaluating different weapon hands; the companion must not call it.

Critical damage (`0x738470`) combines cached inputs with a base value from a
game-balance asset. That asset's live ownership route is not yet established.
The familiar base value is therefore not inserted as a constant. These fields
must remain unavailable until their complete data routes and menu comparisons
are established.

## Local verification

The player and stat suites run with Clang C11, strict warnings, AddressSanitizer
and UndefinedBehaviorSanitizer. Coverage includes all seven class values,
Paragon zero and limits, changing identity blocks, per-read failures, direct and
packed shared caches, traversal limits, nonzero runtime defaults, corrupt
registration IDs, each derived dependency failing or changing, resource-key
changes, critical-chance caps, nonfinite/overflow handling, final ownership
failures after late dependency failures, and the exact worst-case read budget.
Fixtures are synthetic and contain no copied game memory.
