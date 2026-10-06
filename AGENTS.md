# Working on Diablo III Duo

## Objective and present evidence

Develop a Diablo III companion for Eden Duo on AYN Thor. The first live milestone is one correct game value on the lower screen through two fresh launches and a different copied character/save, checked against the game's own menu. Private research on 2026-10-06 matched character level across two launches, a Barbarian level-up from 1 to 2 and a fresh level-1 Wizard. Startup and main-menu states cleared the value. The production package remains a static status page. Further fields, complete lifecycle coverage and measured performance are still required before a working release. See docs/next-step.md for current evidence and dependencies.

Read `README.md`, `project.json`, `vendor/UPSTREAM.json`, and `docs/next-step.md` before continuing. Refer to the pinned package format for syntax and the current upstream C++ parser when behavior is uncertain.

## Development rules

- Do not invent offsets, build IDs, test results, numerical game values or compatibility claims.
- Keep unknown values visibly unavailable. The static preview must keep its development message until replaced by verified live code and accurate failure states.
- Treat `01001B300B9BE000` as a target to confirm, not an identity inferred from the NSO header.
- Keep private game material outside the package and release archive. Do not commit ROMs, keys, firmware, saves, extracted art or memory dumps.
- Work from a copy of the game profile and saves. Begin with read-only data access; interactive inventory work is a later milestone.
- Do not change device firmware, root the Thor, or enable debugging without the user's authorization for that step.
- Desktop tests do not prove Android NCE compatibility. Test on the actual device before declaring support.
- Keep a verified build record with game version, full build ID, emulator version, evidence and performance observations before enabling a build in `supported_builds`.
- The existing builder deliberately permits only the current static stage. Extend its validation deliberately when a real live implementation is ready; do not bypass it to publish unverified support.
- Review public source, history and release assets before publication. Publication is authorized by the owner; development previews must be clearly marked, and a working release requires documented live and hardware evidence. Do not contact upstream developers or announce on other services.
- Use English for project text and communication. Keep dated records of verified findings and missing evidence.

## Commands

`python3 tools/build.py --check`

`python3 tools/build.py`

`python3 -m unittest discover -s tests -v`
