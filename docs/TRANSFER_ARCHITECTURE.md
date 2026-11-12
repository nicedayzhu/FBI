# Transfer architecture

This document records the fork-specific transfer implementation that is kept
out of the project README.

## Normal URL and CIA installation

URL reception and CIA installation use a dedicated writer and an adaptive
32/16/8/4/2-slot queue of 128 KiB blocks. New 3DS download work prefers core 2
with a core 1 fallback. The queue consumes at most 4 MiB and retries partial
service writes.

On the original test system, these changes increased peak LAN throughput from
about 1.4 MB/s to 2.2 MB/s. These figures describe one test environment and
are not a guaranteed transfer rate.

The existing FBI TCP 5000 transfer protocol remains unchanged.

## LayeredFS patch reception

The existing **Remote Install → Receive URLs over the network** screen also
starts an independent patch receiver on TCP 5002. CIA Bridge selects the
receiver through discovery; no second FBI menu item is required.

The receiver consumes the sequential `CBPATCH1` format and supports raw or
per-file zlib payloads. Records may address:

- `romfs` content beneath the title's `romfs` directory;
- title-root content, including `code.ips` beside `romfs`.

Targets are written beneath `sdmc:/luma/titles/<Title ID>`. Paths are
validated before use, and files absent from the incoming patch are preserved.

## Streaming, comparison, and integrity

FBI first requests the fixed 72-byte bundle header. A deterministic content-set
ID and a power-loss-safe completion marker let a completed bundle finish
without transferring its payload or opening destination files. Holding X while
confirming forces a complete per-file check.

On a cache miss, a bounded 4/2-slot queue of 512 KiB blocks feeds direct SD
writes. Existing destinations are compared by size and CRC32; identical files
skip all writes. Changed files grow sequentially, and an old longer tail is
truncated only after the received payload passes CRC32 validation.

Exact write counts, accumulated length, and CRC32 state avoid a redundant final
size query. The direct path also avoids staging trees, rollback renames,
whole-file preallocation, and per-record forced flushes.

The completion marker is invalidated and flushed before an update and is
committed only after every record verifies. An interrupted transfer therefore
cannot create a false cache hit.

## Diagnostics

Progress and completion screens report network, processing, and SD throughput;
file counts; comparison, setup, write, and close time; write-call count; and
cache state. Layouts are line-broken for the 3DS bottom screen.

Capability negotiation and fallback behavior are documented in
[CIA Bridge discovery protocol](CIA_BRIDGE_DISCOVERY.md).
