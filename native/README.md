# Native live preview

This separate build produces **0.2.1-dev**, an exact-build Android companion
preview. It does not change the older static preview builder. Device observations
and remaining limits are recorded in `docs/validation.md`.

The native UI contains Character, Equipment, Skills and Map views. Its bounded
readers cover class, level/Paragon, ten menu statistics, six equipped skill slots,
equipment occupancy and base item names, exploration coverage and resident terrain.
Touch controls inspect slots or change the companion map and its local pin.
They never equip items or write game memory.

## Build

Use Python 3.10 or later and an explicitly installed **macOS Android NDK r28c
(28.2.13676358)**. The ordinary build uses only Python's standard library, with
no network calls, automatic downloads or device access.

```sh
python3 native/build.py --ndk /absolute/path/to/android-ndk-r28c --output dist/native-0.2.0
```

Output: `dist/native-0.2.0/01001B300B9BE000.dsmod.zip`, `SHA256SUMS` and a local
verification receipt. Use a fresh output directory when replacing an older skin;
the builder refuses to remove unknown files from existing staging directories.

The builder checks the explicit source inventory, source hashes, native asset
hashes, exact module hash and every archive member. It rejects unexpected files,
path traversal and symbolic links. Review changes before updating pins.

## Verification

```sh
python3 native/test.py
python3 native/test.py --ndk /absolute/path/to/android-ndk-r28c --output dist/verify-0.2.0
```

Host tests require Clang with AddressSanitizer/UndefinedBehaviorSanitizer.
The optional NDK run builds twice into separate directories and compares complete
archives. Synthetic fixtures exercise failed reads, ownership transitions, bounds,
cache lifetime, navigation rendering, UI bindings and package integrity. They do
not establish game-value or Android performance compatibility.

Equipment/exploration acquisition runs at most four times per second; terrain
acquisition runs once per second. Small current-world/player checks prevent those
caches from retaining a previous character's or world's data. Numeric image
snapshots are copied under a mutex before the separate image worker renders them.
Unsupported scene transforms and parent-scene overrides remain unavailable.

## Assets

`artwork.py` regenerates the original line icons, Cinzel lettering and renamed
Duo Sans bitmap font from the included OFL fonts. Regeneration alone requires
Pillow; normal builds use pinned files in `assets/`. `ASSETS.json` records them.
Every raster includes its source metadata. No character imagery or game art is
included. ABI headers and GPL notices remain pinned in `vendor/UPSTREAM.json`.
