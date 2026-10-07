# Working on Diablo III Duo

## Objective and present evidence

Develop a read-only Diablo III companion for Eden Duo on AYN Thor. Native
`0.2.1-dev` extends the earlier live candidate with character attributes,
equipment inspection and exploration/terrain. The separate root builder still
produces static `0.1.1-dev`. Read `docs/next-step.md` and `docs/validation.md` for
the exact current device evidence and unfinished work; never promote historical
observations or synthetic fixtures into a hardware claim.

Read `README.md`, `project.json`, `vendor/UPSTREAM.json`, and `docs/next-step.md` before continuing. Refer to the pinned package format for syntax and the current upstream C++ parser when behavior is uncertain.

## Development rules

- Do not invent offsets, build IDs, test results, numerical game values or compatibility claims.
- Keep unknown values visibly unavailable. The static preview keeps its development message. Native preview values must remain unavailable when the reader cannot verify them.
- Treat `01001B300B9BE000` as a target to confirm, not an identity inferred from the NSO header.
- Keep private game material outside the package and release archive. Do not commit ROMs, keys, firmware, saves, extracted art or memory dumps.
- Work from a copy of the game profile and saves. Begin with read-only data access; interactive inventory work is a later milestone.
- Do not change device firmware, root the Thor, or enable debugging without the user's authorization for that step.
- Desktop tests do not prove Android NCE compatibility. Test on the actual device before declaring support.
- Keep a verified record of game version, full build ID, emulator version, evidence and performance before declaring support. The root `supported_builds` describes only the separate static package.
- The root builder validates the static package; `native/build.py` validates the separate exact-build live preview. Keep both inventories, hashes and unsupported-state checks intact.
- Review public source, history and release assets before publication. Publication is authorized by the owner; development previews must be clearly marked, and a working release requires documented live and hardware evidence. Do not contact upstream developers or announce on other services.
- Use English for project text and communication. Keep dated records of verified findings and missing evidence.

## Commands

`python3 native/test.py`

`python3 native/test.py --ndk /path/to/android-ndk-r28c --output dist/verify-VERSION`


`python3 tools/build.py --check`

`python3 tools/build.py`

`python3 -m unittest discover -s tests -v`
