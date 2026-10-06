# Upstream investigation — 2026-10-06

The reviewed emulator commit is `5410366938b8aa0aca2c9c61f4e93038b71db90c`.
The companions repository remains at the vendored commit
`015d083e8859c86480360f44b65c4104bbf3c839`; no vendor update was made.

The latest reviewed release is [Eden Duo 1.1.0](https://github.com/igawa6/eden-duo/releases/tag/v1.1.0),
published 2026-10-03. The source declares
[runtime 18](https://github.com/igawa6/eden-duo/blob/5410366938b8aa0aca2c9c61f4e93038b71db90c/src/core/mods/mod_runtime.h#L157).
The owner's installed Eden Duo 1.1.0 was separately observed on the Thor.

## Environment and build identity

The native-module loader recognizes Android and Linux targets but rejects
macOS. Generic experimental Apple Silicon emulator support does not provide
Mac native-module support. The current Mac can build packages, inspect metadata
and perform static analysis. The documented desktop research route is a Linux
headless emulator. For this project, verified Android NDK r28c was installed in
the ignored private toolchain directory to compile small Android modules on the
Mac. A reviewed diagnostic module loaded and completed a bounded read on the
Thor; this does not establish a verified game reader. No paid service was used.
[Loader source](https://github.com/igawa6/eden-duo/blob/5410366938b8aa0aca2c9c61f4e93038b71db90c/src/core/mods/mod_module.cpp#L39).

The NSO header contains a 32-byte build ID at offset `0x40`. Data filenames use
the first eight bytes as 16 uppercase hex characters. Discovery also tries a
lowercase name and generic `data.json`; this lookup alone is not full build
validation. Do not ship generic fallback offsets. A native reader should compare
the full build ID and reject unverified builds.
[Porting guide](https://github.com/igawa6/eden-duo-companions/blob/015d083e8859c86480360f44b65c4104bbf3c839/docs/PORTING_A_GAME.md),
[discovery source](https://github.com/igawa6/eden-duo/blob/5410366938b8aa0aca2c9c61f4e93038b71db90c/src/core/mods/mod_manifest.cpp#L2381),
[module build checks](https://github.com/igawa6/eden-duo-companions/blob/015d083e8859c86480360f44b65c4104bbf3c839/docs/MODULE_GUIDE.md#3-build-ids-and-supports_build).

## First reader

Upstream uses `eden-cli --aux-virtual --screenshot-prefix ...`, developer tools
and `EDEN_DSMOD_CMD` for searches and reads. Use normal gameplay changes to
correlate candidates; optional upstream sentinel writes are outside this
project's read-only milestone. Fresh launches and another copied character
are required to test relocation and ownership.

A declarative point is preferable when its identity and lifecycle checks
suffice. A native module is justified only by the observed structure or required
checks. The host read API validates mapped ranges and overflow; readers must
still validate types, ownership and object lifetime, bound work, and clear stale
values on failure.
[Host reads](https://github.com/igawa6/eden-duo/blob/5410366938b8aa0aca2c9c61f4e93038b71db90c/src/core/mods/mod_module_host.cpp#L195),
[module rules](https://github.com/igawa6/eden-duo-companions/blob/015d083e8859c86480360f44b65c4104bbf3c839/docs/MODULE_GUIDE.md#6-rules-the-project-follows).

Android normally uses NCE. Documented `call`, `sequence` and spy mechanisms
depend on the desktop Dynarmic bridge; an NCE equivalent is unavailable and
unverified. Desktop `EDEN_DSMOD_NO_GUEST_BRIDGE=1` can expose accidental
dependencies but does not establish Android compatibility. No emulator change
is presently justified by a demonstrated Diablo III reader requirement.
[Native-menu constraints](https://github.com/igawa6/eden-duo-companions/blob/015d083e8859c86480360f44b65c4104bbf3c839/docs/PORTING_A_GAME.md#5-driving-the-native-menu).

Additional reviewed references:
[contribution workflow](https://github.com/igawa6/eden-duo-companions/blob/015d083e8859c86480360f44b65c4104bbf3c839/docs/CONTRIBUTE.md),
[package format](https://github.com/igawa6/eden-duo-companions/blob/015d083e8859c86480360f44b65c4104bbf3c839/docs/PACKAGE_FORMAT.md),
[architecture](https://github.com/igawa6/eden-duo-companions/blob/015d083e8859c86480360f44b65c4104bbf3c839/docs/ARCHITECTURE.md).
