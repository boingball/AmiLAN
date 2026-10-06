# AmiLAN

A small, bounded C networking foundation extracted from AmiCraft's working LAN
implementation. The socket ABI, short-I/O stream pump, queue compaction, poll
budgets, timeouts and UDP discovery scheduling are preserved. This is an
extraction, not a new networking stack.

## Architecture

| Layer | Responsibility |
| --- | --- |
| `socket.h`, `host/socket.c`, `amiga/socket.c` | Lazy socket lifecycle, non-blocking TCP and UDP, listen/connect/accept, readiness, optional TCP_NODELAY, fixed socket buffers, IPv4 interfaces and broadcasts |
| `protocol.h` | Endian helpers, strict IPv4 parsing, FNV-1a, eight-byte framing with application signature/version and optional validator |
| `stream.h` | One bounded peer, fragmented frame reconstruction, partial I/O, backpressure, disconnect reasons and connect/idle/stall deadlines |
| `host.h` | Three remote connection slots, one accept attempt per call, excess-connection rejection, slot reuse and closed-ID bitmask |
| `link.h` | Wrap-safe staged-handshake deadlines and application-selected four-byte ping/pong controls |
| `discovery.h` | Fixed UDP request framing, nonce/version checks, broadcast fallback, receive/reply budgets and search expiry; application payload callbacks |
| `zlib.h` | Optional fixed-memory zlib chunk compression, independent of sockets and message framing |

Games own all message IDs and payload meanings. A receive callback must validate
direction, handshake phase and authority before applying a packet. AmiLAN does
not know about players, worlds, blocks, snapshots, inventory or any game rules.
The framing version check and pool expiry hook support application handshakes;
AmiLAN intentionally does not impose a HELLO/WELCOME payload or session protocol.

## Platforms and build

```
make all test example       # Linux/POSIX library, tests, echo example
make test-sanitize         # same suite with ASan and UBSan
make check-amiga            # freestanding m68k archive; requires cross compiler
```

The Linux backend uses POSIX sockets, select and ioctl. Tests include real
loopback TCP/UDP, deterministic injected short I/O and a mocked AmiTCP v4 socket
register ABI. `make check-amiga` compiles the actual Amiga backend with
`m68k-linux-gnu-gcc -m68020-60`; `M68K_CC` and `M68K_AR` can select another
compatible compiler. Consumers can also compile `src/*.c` and one socket backend
directly, as AmiCraft does. Do not link both socket backends.

The Amiga implementation opens `bsdsocket.library` version 4 lazily. It is
freestanding: no NDK, C runtime, DNS, threads or allocator dependency. Supply
`os_openlib`, `os_closelib` and `os_call` with the declarations and register
contract in `include/amilan/amiga.h`. AmiCraft's existing assembly bridge supplies
these unchanged. OS scheduling must be enabled; sockets cannot run under Forbid
or custom-chip hardware takeover. Close all sockets before shutting down a
backend. Keep each API instance on its owning OS task.

The interface parser retains both native AmiTCP-style 32-byte ifreq handling and
the Amiberry/Linux 40-byte record and broadcast-ioctl fallbacks. Mocked ABI tests
are not a substitute for running on real Amiga stacks.

## Memory and work bounds

There are no heap allocations anywhere in the library, including polling,
framing or discovery. Storage is caller-owned and reusable.

| Resource | Bound |
| --- | --- |
| Packet payload / frame | 512 / 520 bytes |
| Peer RX / TX buffers | 520 / 2080 bytes |
| `struct amilan_peer` | 2624 bytes on Linux; measure on target ABI |
| Remote host slots | 3, matching the proven four-participant implementation |
| Socket send/receive buffer request | 4096 bytes each, actual stack bookkeeping is external |
| Stream work per peer/poll | 2048 bytes each way and 8 received frames |
| Interface scratch / returned interfaces | 1024 bytes on Amiga / 4 interfaces |
| Discovery request / maximum reply | 16 / 60 bytes (44 application bytes) |
| Discovery work | 4 received datagrams/poll, at most one reply per 13 ticks |
| Discovery search / resend | 250 / 50 ticks |
| Compression raw chunk / encoded chunk | 4096 / 512 bytes maximum |
| Compression hash scratch | 256 uint16_t entries (512 bytes) on the encoder's stack; no persistent dictionary |

Time is a caller-supplied monotonic **50 Hz** tick count. Connect/stall deadlines
are 500 ticks and idle is 750 ticks. Unsigned subtraction handles wraparound.
Game/session handshake and world-transfer deadlines belong to the application;
use `amilan_deadline_expired` and the host expiry hook. Discovery result storage
is application-owned; bound it to `AMILAN_DISC_HOSTS` (4) and expire stale results
in the codec's `expire` callback. The example discovery test demonstrates this.
No internal discovery cache duplicates the application's decoded advertisements.

`amilan_peer_queue` returns 0 when full or invalid and leaves the peer unchanged.
Retry later or disconnect; do not silently drop authoritative events. Polling
returns 0 after closing a peer and retains its reason. Callback packets are
short-lived stack storage: copy any data needed beyond the callback.

## Basic use

```c
#include "amilan/host.h"

/* Choose your own message IDs and validate payloads here. */
static int valid(const struct amilan_packet *p) {
    return p->type == 1 && p->len == 4;
}
static const struct amilan_codec wire = {{'E','X'}, 1, 255, valid};
static int receive(void *context, struct amilan_peer *peer,
                   const struct amilan_packet *p) {
    return amilan_peer_queue(peer, p, *(uint32_t *)context, &wire);
}

/* Initialize once. Call with OS scheduling enabled. */
struct amilan_host host;
amilan_host_init(&host);
if (!amilan_host_open(&host, 25569)) { /* report unavailable stack/port */ }

/* Each frame, with now measured at 50 Hz: */
int slot = amilan_host_accept(&host, now); /* 1..3 new, 0 none, -3 full */
unsigned closed = amilan_host_poll(&host, now, receive, 0, &now, &wire);
/* Initialize your handshake on new slots; clean game state for closed bits. */

amilan_host_stop(&host);
```

A client initializes `amilan_socket_api api = {0}`, calls `amilan_socket_open`,
initializes a peer with fd=-1, then uses `amilan_connect`. Queue messages with
`amilan_peer_queue`; call `amilan_peer_poll` until it closes or you stop.
`amilan_peer_close` and `amilan_socket_shutdown` complete cleanup.
`amilan_control_receive` can echo a four-byte ping into a chosen pong type; your
application schedules keepalives and handles a full reply queue.

Run `./build/echo host 25569` in one terminal and
`./build/echo 127.0.0.1 25569` in another for a working two-process example.

Discovery callers select an eight-byte signature, discovery and application
versions, port and reply size in `amilan_discovery_codec`. `reply` writes the
application advertisement, `found` validates/stores results, and `expire` removes
stale results. Call `amilan_discovery_open`, `amilan_discovery_search` for a
browser, `amilan_discovery_poll`, and `amilan_discovery_close`. Reuse the same
codec through a lifecycle. Close before reopening an initialized instance.

## Optional chunk compression

`amilan/zlib.h` exposes `amilan_zlib_deflate` and `amilan_zlib_inflate` for
snapshots or other application data. It has no allocator, floating point, C
runtime or external zlib dependency. Compile `src/zlib.c` alone if only the
codec is needed. Calls are independent and reentrant with separate buffers;
input and output must not overlap. The encoder uses a single candidate per
256-entry hash bucket, rather than an unbounded match search.

This is a **bounded zlib wire profile**, not a general-purpose zlib decoder:
header `78 01`, one final fixed-Huffman DEFLATE block, no preset dictionary,
and an Adler-32 trailer. Standard zlib can decompress the output. Stored,
dynamic-Huffman, multiple-block and other-header streams are unsupported.
Raw sizes are 1..4096; encoded sizes are 8..512 bytes. The decoder requires
the expected exact raw size, checks the checksum and rejects trailing bytes,
invalid distances, truncation and output overflow. Output can be modified on
failure; decode into scratch and commit only after a successful return.

The application selects compressed/raw message IDs and carries the raw size,
offset and any transfer checksum. Reserve message metadata space in the
encoder's capacity; AmiLAN framing is unchanged. An encoded chunk need not
be smaller, and incompressible input may not fit. Try smaller raw chunks or
send raw data using the available payload space. For example:

```c
#include "amilan/zlib.h"

uint8_t packed[AMILAN_ZLIB_PACKED], scratch[AMILAN_ZLIB_RAW];
unsigned capacity = AMILAN_ZLIB_PACKED - metadata_bytes;
unsigned bytes = amilan_zlib_deflate(packed, capacity, raw, raw_size);
if (bytes && bytes < raw_size) {
    /* Queue metadata + packed[0..bytes), retrying if the TX queue is full. */
} else {
    /* Split into raw payload-sized chunks, or retry compression smaller. */
}
/* After validating message metadata and exact expected raw size: */
if (amilan_zlib_inflate(scratch, raw_size, packed, bytes)) {
    /* Commit scratch[0..raw_size) at the validated destination offset. */
}
```

Compression is opt-in; transport polling does not compress packets or change
timeouts. Snapshot generation, join progress, pausing and raw fallback belong
to the application. The tests check interoperability against Python's standard
zlib, guarded buffer capacities, malformed input and sanitizer coverage of the
C codec. The Python ctypes shared library is unsanitized; the standalone C
test runs with ASan/UBSan under `make test-sanitize`.

## Compatibility and provenance

Extracted from `boingball/AmiCraft` commit
`473d0025e54489aa38004ad1362ec3dbaae1aa50`: `src/net/socket.h`,
`stream.c/.h`, generic framing/helpers in `protocol.c`, host pool/lifecycle and
link timeout/control pieces, discovery transport and both socket backends.
Useful stream, pool, TCP/UDP and Amiga ABI scenarios were ported; game tests stay
with AmiCraft. No license was added during extraction; the repository owner can
choose one separately.

The compression codec and independent zlib interoperability fixtures were
extracted from AmiCraft commit `288f1e582f6eaa8aa909e2f59ff3d376f825ae4e`
(`src/net/map_codec.c/.h`, `tests/test_map_codec.py`). Names are generalized to
`amilan_zlib_*`; the upstream encoder caps capacity at 512 bytes to match the
decoder's existing limit and both APIs reject null buffers. AmiCraft's existing
compressed map bytes remain compatible. No automatic compression or new wire
version is introduced.

The eight-byte frame stays: signature[2], version[1], type[1], big-endian
length[2], zero[2], then payload. Types 1..255 are application-defined.
AmiCraft supplies its existing `AC` signature, version 6 and exact packet
validator. Its discovery codec supplies `AMICRAFT`, discovery version 1,
application version 6 and the same 60-byte advert. No wire version bump is needed.
