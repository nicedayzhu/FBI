# CIA Bridge discovery protocol

This FBI fork exposes a small, stateless LAN discovery endpoint while the
**Receive URLs over the network** screen is active. It does not change FBI's
existing URL transfer protocol and also advertises the patch listener when it
was started successfully.

## Version 1

| Field | Value |
| --- | --- |
| Transport | UDP over IPv4 |
| Receiver port | `5001` |
| Discovery request | `CIA_BRIDGE_DISCOVER_V1` |
| Discovery response | `CIA_BRIDGE_FBI_V1;tcp=5000` |
| Install transport | Existing FBI TCP protocol on the advertised port |

Requests and responses are ASCII bytes with no trailing NUL, newline, or
length prefix. FBI sends the response as a unicast datagram to the source
address and source port of a valid request. CIA Bridge uses the response
datagram's source IPv4 address as the console address.

FBI only binds the discovery socket while its network-receive screen is open.
Unknown or malformed datagrams are ignored, and failure to bind UDP port 5001
does not prevent the standard TCP receiver from working.

## Compatibility and fallback

The TCP port 5000 payload format is untouched, so Boop-compatible senders and
older CIA Bridge versions continue to work. Old FBI versions do not understand
the UDP request and simply do not respond. CIA Bridge then falls back to its
existing Nintendo-MAC/candidate scan, followed by manual IP entry.

Future incompatible discovery formats must use a new request and response
version token instead of changing `V1` in place.

## Version 2 patch capability

The existing **Receive URLs over the network** screen listens for the same
discovery request on UDP 5001 and replies:

`CIA_BRIDGE_FBI_V2;cia=5000;patch=5002;codecs=raw;features=title-root,zlib;version=2.7.6`

`cia` preserves the original Remote Install control port, `patch` advertises
the independent CBPATCH1 control port, and the required `codecs=raw` field
keeps older senders compatible. Optional `features` declare support for paths
relative to the Luma title root and for per-record zlib payloads. `version`
identifies the running receiver for UI display and troubleshooting. The screen
selects the protocol by the TCP port that CIA Bridge connects to; users do not
choose a second FBI menu item. If TCP 5002 cannot be opened, discovery falls
back to the Version 1 response and TCP 5000 remains available.
