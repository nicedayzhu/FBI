# FBI

FBI is an open source title manager for the 3DS.

This fork keeps the faster HTTP transfer changes from
[colbyshores/FBI](https://github.com/colbyshores/FBI) and adds local discovery
and LayeredFS patch reception for
[CIA Bridge](https://github.com/nicedayzhu/cia-bridge).

## CIA Bridge additions

- Discovers the active network receiver automatically while **Remote Install →
  Receive URLs over the network** is open.
- Receives `CBPATCH1` LayeredFS bundles containing `romfs` files and title-root
  files such as `code.ips`.
- Supports raw and per-file zlib payloads, integrity checks, safe repeat
  transfers, and faster pipelined network writes.
- Includes English and Simplified Chinese in one build, with automatic system
  language selection and a persistent manual override.

The CIA uses title ID `000400000F800200` and can be installed beside official
FBI. See [Installation and usage](docs/INSTALLATION.md) for artifact choices,
updating, and receiver troubleshooting.

Fork-specific implementation and maintenance details are kept in dedicated
documents:

- [Transfer architecture](docs/TRANSFER_ARCHITECTURE.md)
- [CIA Bridge discovery protocol](docs/CIA_BRIDGE_DISCOVERY.md)
- [Development and release builds](docs/DEVELOPMENT.md)
- [English and Simplified Chinese maintenance](docs/LOCALIZATION_ZH_CN.md)
- [Release history](CHANGELOG.md)

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
