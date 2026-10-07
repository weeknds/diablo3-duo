# Publication and release

The owner has authorized public source and experimental releases. `0.2.0-dev`
is a live preview for one executable build. Pending field, lifecycle or
performance coverage must remain explicit.

## Source and artifact checks

1. Run the public Python suite, static manifest check/build and `native/test.py`.
   For a native release use `--ndk /absolute/path/to/ndk --output dist/verify-VERSION`
   to check two reproducible Android builds in a fresh output directory.
2. Review every addition to `public-files.txt`, then stage those explicit paths.
   Compare the inventory with tracked files. Exclude private research, game
   material, device helpers, local configuration, secrets and generated stages.
   Review the history being published as well as the working tree.
3. Check upstream pins, `native/SOURCES.json` and `native/ASSETS.json`. Review
   source changes before updating hashes. Retain GPL, OFL and asset notices.
4. Inspect the selected ZIP's complete membership, not just its file count.
   The native package contains metadata, the exact-build Android arm64 module,
   original UI/font assets and licences. The separate root output is a static
   preview with the same archive filename. Do not package an entire build directory.
5. Check version, title, runtime, binary hash, full build gate, preview label and
   archive reproducibility. Confirm which exact binary was tested on the device
   and retain observations in [validation](validation.md).
6. Commit reviewed public source, tag it, and verify the selected package against
   that source. Publish only `.dsmod.zip`, `SHA256SUMS` and accurate release notes.
   Read back the uploaded asset and compare its checksum.

Public UI captures must show only the companion. No saves, raw memory traces,
game-display captures, extracted artwork or private verification receipts belong
in the public tree or release assets. CI needs no private game files.

## Verified scope

A working-support claim requires matching values on the target build, fresh
launches and another character, correct unavailable states, relevant lifecycle
and input checks, measured acceptable overhead and the actual device UI. Record
tested classes, modes, values, scenes, driver and emulator versions. Synthetic
tests and old measurements do not validate new readers.

Keep the root static package's support flags truthful while that separate output
exists. Experimental releases may ship useful verified portions while declaring
remaining limitations. Do not contact upstream maintainers or announce on
unrelated services.
