# Contributing

Diablo III Duo is an unofficial GPL-3.0 project with a static design preview and
a separate native live test candidate. Read [AGENTS.md](AGENTS.md),
[the next step](docs/next-step.md), [compatibility](docs/compatibility.md) and the
[reader architecture](docs/reader.md) before changing live acquisition.

Use Python 3.10 or newer for the standard-library tooling:

```sh
python3 -m unittest discover -s tests -v
python3 tools/build.py --check
python3 tools/build.py
```

These commands build only the static `0.1.1-dev` preview. For the separate native
`0.1.2-rc.1` candidate:

```sh
python3 native/test.py
python3 native/build.py --ndk /absolute/path/to/android-ndk-r28c
```

The native tests require clang with sanitizers; Android compilation requires
explicit macOS NDK r28c (28.2.13676358). Neither build needs game files, keys,
a device or additional Python packages. See [native verification](native/README.md)
for repeat-build checks. Keep fixtures synthetic or independently redistributable.
Test changed behavior and failure cases; prose changes do not need tests.

Keep changes focused and project text in English. Preserve licences and pinned
provenance in `vendor/UPSTREAM.json` and `native/vendor/UPSTREAM.json`. The native
source inventory and hashes are recorded in `native/SOURCES.json`; changes to
source or binary pins need review and corresponding evidence.

Private research belongs under ignored `private/`. Never submit game archives,
keys, tickets, certificates, firmware, saves, extracted art, executable bytes or
memory dumps. Review the explicit `public-files.txt` inventory, every staged file
and all history before pushing. Ignore patterns alone are not a publication
audit. Redact personal paths and account/device identifiers from reports.

For a reader change, describe the exact executable build, types, ownership,
relocation/lifetime checks and failure behavior. Compare new fields against the
game's UI and follow the [device protocol](docs/device-validation.md). Separate
synthetic tests from physical Thor results. A discovered address or successful
compilation does not establish supported builds or measured performance.

Keep `tools/build.py`'s static guard intact. Extend the separate native validator
when its verified scope changes, preserving exact inventories, hashes and
unavailable states. Guest writes and equipment actions remain a later milestone
requiring a separate design, disposable saves and Android NCE verification.
