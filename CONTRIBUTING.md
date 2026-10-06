# Contributing

Diablo III Duo is an unofficial GPL-3.0 project in the static preview stage.
Read [AGENTS.md](AGENTS.md), [the next step](docs/next-step.md), and
[compatibility](docs/compatibility.md) before proposing live integration.

Use Python 3.10 or newer and run:

```sh
python3 -m unittest discover -s tests -v
python3 tools/build.py --check
python3 tools/build.py
```

These commands need no game files, device, keys, or additional Python packages.
Keep automated fixtures synthetic or independently redistributable. Add tests
for changed behavior and failure cases; prose changes do not need tests.

Keep changes focused and write project text in English. Preserve the upstream
licence and provenance in `vendor/UPSTREAM.json`. Dependency updates must identify
the source commit and reviewed changes. Do not edit vendored files casually.

Private research belongs under ignored `private/`. Never submit game archives,
keys, tickets, certificates, firmware, saves, extracted art, executable bytes,
or memory dumps. Review every staged file and the history before pushing;
ignore patterns alone are not a publication audit. Redact personal paths and
account or device identifiers from issue reports.

For a reader change, describe the exact executable build, data type, pointer
ownership, relocation and lifetime checks, unsupported-build behavior, and
evidence from the game's own UI. Separate synthetic tests, desktop observations,
and physical Thor results. Follow [the device protocol](docs/device-validation.md).
Never enable a build in `supported_builds` based only on a discovered address.

The current builder deliberately accepts only static content. Extend its
validation when a verified live implementation exists, retaining the explicit
package allowlist and evidence requirements. Memory writes and equipment actions
require a separate design, disposable saves, and verification on Android NCE.
