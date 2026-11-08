# FBI

FBI is an open source title manager for the 3DS.

This fork keeps the faster HTTP transfer changes from
[colbyshores/FBI](https://github.com/colbyshores/FBI) and adds opt-in local
discovery for [CIA Bridge](https://github.com/nicedayzhu/cia-bridge).

URL reception and CIA installation are pipelined through a dedicated writer
and an adaptive 32/16/8/4/2-slot queue of 128 KiB blocks. New 3DS download work
prefers core 2 with a core 1 fallback. The queue uses at most 4 MiB, retries
partial service writes, and improved peak LAN throughput from about 1.4 MB/s
to 2.2 MB/s on the test system.

LayeredFS patch reception is exposed through the existing **Remote Install →
Receive URLs over the network** screen as normal installs, with automatic protocol
selection between TCP 5000 and TCP 5002. It consumes the sequential, raw-only
`CBPATCH1` format. FBI first requests only the fixed 72-byte header. A
deterministic content-set ID and a power-loss-safe completion marker allow an
already completed bundle to finish without transferring its payload or opening
any target file. Hold X while choosing Yes to force a complete per-file check.

On a cache miss, a bounded 4/2-slot queue of 512 KiB blocks feeds direct SD
writes. Each destination is compared by size and CRC32; identical files skip
all writes. Changed files grow sequentially under
`sdmc:/luma/titles/<Title ID>/romfs`, and an old longer tail is truncated only
after the payload passes CRC32. Exact write counts, accumulated length, and
CRC32 avoid a redundant final size query. Files absent from the incoming patch
are preserved.

The direct path avoids staging trees, rollback renames, per-record forced
flushes, and whole-file preallocation. Its completion marker is invalidated and
flushed before updates, then committed only after every record verifies, so an
interrupted transfer cannot produce a false cache hit.

Progress and completion screens report network, processing, and SD throughput;
file counts; comparison, setup, write, and close time; write-call count; and
cache status. The layouts are line-broken to fit the bottom screen.

When **Remote Install → Receive URLs over the network** is open, CIA Bridge
can discover the console automatically. Older FBI builds remain compatible:
CIA Bridge falls back to neighbor scanning or manual IP entry when no discovery
reply is available. The existing TCP port 5000 transfer protocol is unchanged.

The CIA retains the upstream **FBI-curl** HOME title and unique ID `0xF8002`
(`000400000F800200`), with product code `CTR-P-CFB2`. It can therefore be
installed beside the official FBI (`0xF8001`) instead of replacing it. This
fork intentionally updates/replaces an existing FBI-curl installation.

Build artifacts are named `FBI-cia-bridge.cia` and
`FBI-cia-bridge.3dsx`, so they remain easy to distinguish from official FBI
files on an SD card. This filename change does not alter the HOME Menu title,
title ID, or installed-title upgrade behavior described above.

The installed build uses a teal HOME Menu icon, banner logo, and runtime theme
while retaining FBI's layout and navigation. Maintainers can regenerate the
HOME Menu assets with `scripts/recolor-brand-assets.ps1`, recolor the runtime
textures with `scripts/recolor-runtime-theme.ps1`, and use
[txobtool](https://github.com/dnasdw/txobtool).

The same build includes English and Simplified Chinese. It follows the 3DS
system language automatically (Chinese selects zh-CN; all other system
languages fall back to English), and **Language** on the main menu provides a
persistent manual override. Localization catalogs are maintained separately in
`source/core/i18n/lang_en.c` and `source/core/i18n/lang_zh_cn.c`. See
[the runtime localization notes](docs/LOCALIZATION_ZH_CN.md) for maintenance
and validation details.

The discovery wire format is documented in
[docs/CIA_BRIDGE_DISCOVERY.md](docs/CIA_BRIDGE_DISCOVERY.md).
Release history is maintained in [CHANGELOG.md](CHANGELOG.md), not in this
README.

Original project: https://github.com/Steveice10/FBI

Requires [devkitARM](http://sourceforge.net/projects/devkitpro/files/devkitARM/), along with 3ds-curl, 3ds-zlib, and 3ds-jansson from the devkitPro pacman repository, to build.

# Features

* Browse and modify the SD card, TWL photos, TWL sounds, save data, and ext save data.
* Export, import, and erase save data from DS cartridges.
* Export, import, and delete save data secure values.
* Install titles/tickets from a file system, over a local network, or over the Internet with a URL or QR code.
  * Automatically imports title seeds on installation, either from the Internet or the SD card.
  * Advertises the active network receiver to CIA Bridge on the local network.
* Browse and delete pending titles (downloaded updates, in-progress eShop titles, etc).
* Customize appearance by placing replacements for RomFS resources in "sdmc:/fbi/theme/".

* Only available when run from a CIA, 3DS, or a 3DSX under Luma3DS:
  * Browse and modify CTR NAND, TWL NAND, and system save data.
  * Dump the raw NAND image to the SD card.
  * Launch titles installed to the system.

# Credit

Banner: Originally created by [OctopusRift](http://gbatemp.net/members/octopusrift.356526/), touched up by [Apache Thunder](https://gbatemp.net/members/apache-thunder.105648/), updated for new logo by [PabloMK7](http://gbatemp.net/members/pablomk7.345712/).

Logo: [PabloMK7](http://gbatemp.net/members/pablomk7.345712/)

SPI Protocol Information: [TuxSH](https://github.com/TuxSH/) ([TWLSaveTool](https://github.com/TuxSH/TWLSaveTool))
