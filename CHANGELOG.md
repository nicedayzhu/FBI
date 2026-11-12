# Changelog

## 2.7.6

- Added title-root CBPATCH1 records so `code.ips` is installed alongside
  `romfs` instead of being silently omitted.
- Added negotiated per-record zlib streaming to reduce network payload without
  staging a ZIP on the SD card.
- Fixed the integrated Remote Install discovery response to advertise the
  `title-root,zlib` features, and added the receiver version for diagnostics.
- Added a version-specific build stamp so version-only changes recompile every
  object and cannot leave UI and discovery version constants out of sync.

## 2.7.5

- Replaced the Chinese-only build with one universal English and Simplified
  Chinese binary. The default follows the 3DS system language, while a new
  **Language** menu provides an immediate, persistent manual override.
- Split translations into independently maintained `lang_en.c/.h` and
  `lang_zh_cn.c/.h` catalogs, with automated catalog parity, format-string,
  UTF-8, and BCFNT glyph-coverage checks.
- Localized the bundled `servefiles` and `sendurls` tools according to the host
  locale, with `FBI_LANGUAGE` available as an explicit override.
- Pointed the updater at this fork's releases and retained one language-neutral
  asset name so every installation receives the same universal build.
- Added a project-local ZIP fallback for Windows/MSYS2 systems without `zip`,
  and excluded ELF and other build intermediates from release archives.
- Kept the product name `FBI CIA Bridge`, the original blue startup animation,
  and a neutral English HOME Menu description.

## 2.7.4

- Added raw-only `CBPATCH1` LayeredFS patch reception on TCP 5002, integrated
  into the existing **Receive URLs over the network** screen with automatic
  discovery and protocol selection. Normal FBI TCP 5000 installs are unchanged.
- Streamed sorted records directly to their final `romfs` paths. Existing files
  outside the patch are preserved, while strict path, length, write-count, and
  CRC32 checks reject incomplete or unsafe input.
- Added size-and-CRC skip checks and a transactional completion cache. A cache
  hit reads only the 72-byte header; holding X while confirming forces a full
  verification. Interrupted transfers cannot leave a false valid marker.
- Optimized first installs with aligned 512 KiB blocks, a bounded 4/2-slot
  receive-ahead queue, direct sequential writes, directory reuse, and no
  per-file forced flush, preallocation pass, or redundant final size query.
- Added compact progress and completion diagnostics for network, SD, file,
  setup, comparison, write, close, cache, and card state.
- Applied the teal/green identity to the complete runtime interface without
  changing FBI's established navigation or default menu strings.

## 2.7.3

- Expanded the adaptive 128 KiB transfer queue to 32/16/8/4/2 slots.

## 2.7.2

- Pipelined URL reception and CIA installation with a dedicated writer.

## 2.7.1

- Added opt-in CIA Bridge discovery while retaining FBI-curl compatibility.
