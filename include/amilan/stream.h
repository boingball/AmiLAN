#ifndef AMILAN_STREAM_H
#define AMILAN_STREAM_H
#include "protocol.h"
#include "socket.h"
#define AMILAN_TX_BYTES (AMILAN_FRAME * 4)
#define AMILAN_POLL_BYTES 2048
#define AMILAN_POLL_FRAMES 8
struct amilan_peer {
    int fd;
    uint8_t connecting, reason;
    uint16_t rx_len, tx_start, tx_len;
    uint32_t born, last_packet, last_progress;
    uint8_t rx[AMILAN_FRAME], tx[AMILAN_TX_BYTES];
};
/* Called only for complete, structurally valid packets. Return 0 to reject.
 * Application must validate message direction, phase, application rules and authority.
 * Packet storage is temporary; copy bounded actions into the application queue. */
typedef int (*amilan_receive_fn)(void *context, struct amilan_peer *peer, const struct amilan_packet *packet);
void amilan_peer_init(struct amilan_peer *p, int fd, int connecting, uint32_t now);
void amilan_peer_close(struct amilan_socket_api *api, struct amilan_peer *p, int reason);
/* A full queue returns 0, changes nothing. Caller MUST retry or disconnect,
 * never silently discard an authoritative event. No unbounded packet queue. */
int amilan_peer_queue(struct amilan_peer *p, const struct amilan_packet *packet, uint32_t now, const struct amilan_codec *codec);
/* 1 alive, 0 closed. Each call has fixed byte/frame budgets. */
int amilan_peer_poll(struct amilan_socket_api *api, struct amilan_peer *p, uint32_t now,
                  amilan_receive_fn receive, void *context, const struct amilan_codec *codec);
#endif
