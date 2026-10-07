# Publication and release

The owner has authorized public source and eventual releases. An incomplete
binary must be labeled a development preview or prerelease. The separate
`0.1.2-rc.1` native package is a live test candidate; installation and a few
correct fields do not establish a finished or broadly compatible product.

## Source and candidate checks

1. Run the public Python tests, static manifest check and static build. For a
   native candidate, also run `native/test.py --ndk /absolute/path/to/ndk` to
   exercise sanitizers and two separate reproducible Android builds.
2. Review `public-files.txt` before explicitly staging its paths. The current
   reviewed inventory contains 101 files; update and review it whenever source
   membership changes. Exclude private research, local tooling configuration,
   generated files, game material, personal paths and secrets. Inspect all
   history to be published, not only the current tree.
3. Verify both upstream provenance manifests and `native/SOURCES.json`. Retain
   GPL-3.0, font licences and attribution; review added assets' redistribution
   permissions. Never silently update hashes to make a changed input pass.
4. Inspect the correct ZIP. Both outputs share the same filename:

   | Output | Current scope | Exact member count |
   | --- | --- | --- |
   | `dist/01001B300B9BE000.dsmod.zip` | Static `0.1.1-dev` preview | 16 |
   | `dist/live/01001B300B9BE000.dsmod.zip` | Native `0.1.2-rc.1` test candidate | 15 |

   The static package contains two metadata files and 14 allowlisted assets.
   The live package contains two metadata files, 11 UI/font/licence assets,
   the GPL licence and one Android arm64 module. The builders validate exact
   paths and hashes; counts alone are insufficient. Check version, title,
   runtime, module hash/full build gate, visible candidate labels and exact
   archive reproducibility. Do not package the whole `dist/live/` directory.
5. Review source and documentation before pushing. Label CI artifacts according
   to what they actually contain. Publish only the selected package, checksum
   and accurate release notes—not local verification logs or stage directories.

Do not publish raw intake reports, saves or research captures. No game bytes are
needed in CI. Retain raw device evidence privately; use redacted public records.

## Working release gate

A working release requires useful live fields checked against the game on the
exact target build, two fresh launches and another test character, correct
invalid states, lifecycle/input checks, measured acceptable overhead, and a
polished device-checked UI. See [validation](validation.md) and the
[hardware protocol](device-validation.md). Pending performance or lifecycle work
must remain pending in release notes; source/build checks cannot replace it.

When the required evidence passes, choose a version matching that verified
scope, update changelog/compatibility records and relevant support metadata,
tag reviewed source and build from the tag. The static preview's `project.json`
guards remain truthful while that separate output exists. Publish the chosen
`.dsmod.zip`, `SHA256SUMS` and release notes. Distinguish observed
fields/classes/modes from limitations and verify uploaded hashes against the
reviewed local artifact and tag.

Users should need only the package and installation guide. Do not announce on
other services or contact upstream maintainers.
