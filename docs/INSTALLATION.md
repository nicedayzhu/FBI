# Installation and usage

FBI CIA Bridge is distributed as both an installed CIA application and a
Homebrew Launcher 3DSX application. Use the CIA for the normal HOME Menu
experience; use the 3DSX when an installed title is not desired.

## Installed-title identity

The CIA intentionally retains the identity of the earlier FBI-curl fork:

| Field | Value |
| --- | --- |
| HOME Menu title | `FBI CIA Bridge` |
| Unique ID | `0xF8002` |
| Title ID | `000400000F800200` |
| Product code | `CTR-P-CFB2` |

This title can be installed beside official FBI, which uses unique ID
`0xF8001`. Installing a new FBI CIA Bridge CIA updates or replaces an existing
FBI-curl installation with the same title ID.

The release filenames are fork-specific (`FBI-cia-bridge.cia` and
`FBI-cia-bridge.3dsx`), but filenames do not affect installed-title identity.

## Install or update

1. Obtain `FBI-cia-bridge.cia` from a release archive or a verified local
   build.
2. Install the CIA with a trusted title manager, or copy the 3DSX application
   directory to the SD card for Homebrew Launcher use.
3. Launch **FBI CIA Bridge**. The application and CIA Bridge discovery entry
   should both report version `2.7.6`.

If the application still shows an older version after installing, confirm
that the selected CIA has title ID `000400000F800200` and reinstall the current
artifact. A sender showing `2.7.6` while the application itself shows `2.7.5`
indicates a stale mixed-object development build, not two independently
installed receiver versions; rebuild from a clean tree before reinstalling.

## Receive from CIA Bridge

1. Connect the computer and 3DS to the same local network.
2. In FBI CIA Bridge, open **Remote Install → Receive URLs over the network**.
3. Select the discovered console in CIA Bridge, or enter its IPv4 address
   manually when discovery is unavailable.
4. Select installation files or a supported LayeredFS patch bundle and start
   the transfer.

Normal FBI URL/CIA installation uses the existing receiver. LayeredFS bundles
use the patch receiver and may contain both `romfs` content and title-root
files such as `code.ips`; CIA Bridge selects the correct receiver
automatically.

Title-root and zlib patch transfers require the receiver capabilities
advertised by FBI CIA Bridge 2.7.6. Older receivers remain usable for ordinary
installation, but CIA Bridge rejects title-root patch records when it cannot
confirm that they can be written safely.

Protocol details are documented in
[CIA Bridge discovery protocol](CIA_BRIDGE_DISCOVERY.md).
