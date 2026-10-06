# AmiLAN

Bounded, non-blocking LAN transport extracted from AmiCraft. This first stage
contains its proven socket backends, stream pump and eight-byte packet framing.
Applications select the frame signature/version and validate their own payloads.
No application packet IDs or game state are defined here.

Build the Linux static library with `make`. Amiga applications compile the core
with `amiga/socket.c` and supply the three OS bridge functions in
`include/amilan/amiga.h`. Never call sockets during hardware takeover/Forbid.

RX is 520 bytes and TX is 2080 bytes per peer; each poll is limited to 2048
bytes each way and eight received frames. There are no heap allocations.
