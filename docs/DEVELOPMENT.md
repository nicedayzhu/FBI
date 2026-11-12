# Development and release builds

## Prerequisites

Install devkitPro with devkitARM and the 3DS packages for curl, zlib, and
jansson. The build expects `DEVKITPRO` and `DEVKITARM` to identify that
toolchain.

## Build

From the repository root:

```sh
make -j2
```

For a release or after changing toolchains, use a clean build:

```sh
make clean
make -j2
python scripts/check-localization.py
```

The project-local `scripts/zip` wrapper uses the native `zip` utility when it
is available and falls back to PowerShell on Windows/MSYS2.

## Outputs

The build places target files beneath `output/3ds-arm/` and creates
`output/FBI-cia-bridge.zip`. Important artifacts include:

- `FBI-cia-bridge.cia` for installation;
- `FBI-cia-bridge.3dsx` and its application directory for Homebrew Launcher;
- `FBI-cia-bridge.3ds` for compatible development workflows;
- `FBI-cia-bridge.elf` for debugging and symbol inspection.

Release archives must not contain stale build intermediates from another
version.

## Version consistency

The version is defined by `VERSION_MAJOR`, `VERSION_MINOR`, and
`VERSION_MICRO` in the root `Makefile`. These macros feed the runtime UI, HTTP
user agent, package metadata, and CIA Bridge discovery response.

A version-specific build stamp makes a version-only Makefile change rebuild
all objects. Release builds should still start with `make clean`, then verify
that the version shown inside FBI matches the version displayed by CIA Bridge.
The current release version is `2.7.6`.

## Project-specific maintenance

- Runtime language catalogs live in `source/core/i18n/lang_en.c` and
  `source/core/i18n/lang_zh_cn.c`; see
  [runtime localization notes](LOCALIZATION_ZH_CN.md).
- `scripts/recolor-brand-assets.ps1` regenerates the teal HOME Menu branding.
- `scripts/recolor-runtime-theme.ps1` recolors runtime textures while
  retaining FBI's layout and navigation.
- [txobtool](https://github.com/dnasdw/txobtool) is used for relevant Nintendo
  texture assets.
- Discovery changes must remain compatible with the documented
  [wire protocol](CIA_BRIDGE_DISCOVERY.md).

## Release validation

Before publishing a build:

1. Run a clean build and the localization checker.
2. Confirm the CIA and archive were regenerated from that build.
3. Install the CIA and confirm the on-device UI reports the intended version.
4. Open the network-receive screen and confirm CIA Bridge reports the same
   receiver version and expected capabilities.
5. Exercise one normal CIA transfer and one LayeredFS bundle containing both a
   `romfs` file and a title-root `code.ips` file.
