# CIA Bridge discovery protocol

This FBI fork exposes a small, stateless LAN discovery endpoint while the
**Receive URLs over the network** screen is active. It does not change FBI's
existing URL transfer protocol.

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
