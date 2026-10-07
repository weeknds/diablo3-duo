# Native live test candidate

This separate build produces **0.1.2-rc.1**, an exact-build Android test candidate.
It does not change the static preview builder or declare hardware support.
The UI shows character level, four menu statistics and six equipped skill slots;
unknown values remain unavailable. It performs read-only, bounded memory access.
There are no maps, inventory actions, rune-name lookup or game assets.

## Build

Use Python 3.10 or later and an explicitly installed **macOS Android NDK r28c
(28.2.13676358)**. The ordinary build uses only Python's standard library, with
no network calls, automatic downloads or device access.

```sh
python3 native/build.py --ndk /absolute/path/to/android-ndk-r28c
```

Output: `dist/live/01001B300B9BE000.dsmod.zip`. Staging and build receipts remain
under ignored `dist/live/`. The build checks preserved source hashes, the source
file inventory, static UI/assets, exact native binary hash and final archive.
It rejects unexpected source/staging files and symbolic links instead of removing
or silently packaging them. Source changes require an explicit review and pin
update. The seven C compilation units and nine headers are preserved byte for
byte from the recovered reader; `SOURCES.json` records those bytes.

## Host verification

```sh
python3 native/test.py
python3 native/test.py --ndk /absolute/path/to/android-ndk-r28c
```

Host tests require `clang` with AddressSanitizer/UndefinedBehaviorSanitizer.
The Python UI/package checks use only the standard library. The optional NDK run also rebuilds twice into separate directories and compares
complete archive bytes. These tests use synthetic fixtures and establish no
Android runtime, game-value or performance compatibility.

The retained font decoder is unchanged; its tests generate synthetic font
metrics. The current skin uses the separately licensed public MFNT font assets.
`vendor/UPSTREAM.json` pins the two GPL ABI headers and license. No old private
verifier, game dump, capture, save or ROM is an input to this public build.
