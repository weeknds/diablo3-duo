# Publication and release

The owner has authorized a public repository and eventual releases. Source may
describe ongoing development. An incomplete binary must be explicitly labelled
a development preview or prerelease; do not claim a working release yet.

## Source and preview checks

1. Run the synthetic tests, manifest check and build commands in the README.
2. Review `public-files.txt` before explicitly staging its paths. Exclude `private/`, local tooling
   configuration, generated files, game material, personal paths and secrets.
   Inspect all history to be published, not only the current tree.
3. Confirm vendored SHA-256 values match `vendor/UPSTREAM.json`; retain GPL-3.0
   and attribution. Review redistribution permissions for added assets.
4. Inspect the ZIP. The current stage contains exactly `package.json` and
   `dualscreen/manifest.json`. Check version, title, runtime, development labels
   and absence of live-support claims. Confirm reproducible output.
5. Review source and documentation before public push. CI artifacts must be
   marked as development previews and contain only the package and checksum.

Do not publish raw intake reports or research captures. NSP XML is advisory
metadata, not compatibility evidence. No game bytes are needed in CI.

## Working release gate

A working release requires observed game startup and package installation on
the target Thor, useful verified live fields for exact builds, correct invalid
states, lifecycle/input checks, measured acceptable performance, and a polished
device-checked UI. Keep compatibility and project flags consistent with evidence.
Follow [the hardware protocol](device-validation.md).

Once those requirements pass, choose a version matching the verified scope,
update the changelog and compatibility record, tag the reviewed source, build
from that tag, and publish the `.dsmod.zip`, `SHA256SUMS`, and release notes.
Notes must distinguish supported fields/classes/modes from limitations. Verify
the uploaded checksum and repository/tag/release agreement.

Normal users should need only the package and installation guide, not development
tools. Do not announce on other services or contact upstream maintainers.
