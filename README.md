# FBI

FBI is an open source title manager for the 3DS.

This fork keeps the faster HTTP transfer changes from
[colbyshores/FBI](https://github.com/colbyshores/FBI) and adds opt-in local
discovery for [CIA Bridge](https://github.com/nicedayzhu/cia-bridge). Its first
CIA Bridge-compatible build is version `2.7.1`, based on FBI-curl `2.7.0`.

Version `2.7.2` pipelines URL reception and CIA installation instead of pausing
the network for every AM write. New 3DS download work prefers core 2 with a
core 1 fallback, while a dedicated writer drains an adaptive 16/8/4/2-slot
queue. The queue uses at most 2 MiB with the default 128 KiB blocks and absorbs
occasional long-latency AM/SD writes without collapsing the TCP receive window.
Successful writes are also checked and retried when the service reports a
partial write. Hardware A/B testing improved peak LAN throughput from about
1.4 MB/s to 2.2 MB/s on the test system.

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

The installed build also uses a teal HOME Menu icon and a matching teal FBI
logo on the rotating top-screen 3D banner. The model, animation, lighting, and
gray side texture remain unchanged. Maintainers can regenerate both branded
assets with `scripts/recolor-brand-assets.ps1` and
[txobtool](https://github.com/dnasdw/txobtool). The script updates both the
front texture and the `BOX_EDGE` material color used by the raised logo and
border on real hardware.

The discovery wire format is documented in
[docs/CIA_BRIDGE_DISCOVERY.md](docs/CIA_BRIDGE_DISCOVERY.md).

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
