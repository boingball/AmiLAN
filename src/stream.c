#include "amilan/stream.h"
void amilan_peer_init(struct amilan_peer *p, int fd, int connecting, uint32_t now)
{
    p->fd = fd; p->connecting = (uint8_t)!!connecting; p->reason = AMILAN_NORMAL;
    p->rx_len = p->tx_start = p->tx_len = 0;
    p->born = p->last_packet = p->last_progress = now;
}
void amilan_peer_close(struct amilan_socket_api *api, struct amilan_peer *p, int reason)
{
    if (p->fd >= 0) amilan_socket_close(api, p->fd);
    p->fd = -1; p->reason = (uint8_t)reason;
    p->connecting = 0; p->rx_len = p->tx_start = p->tx_len = 0;
}
int amilan_peer_queue(struct amilan_peer *p, const struct amilan_packet *packet, uint32_t now, const struct amilan_codec *codec)
{
    unsigned i, len;
    if (p->fd < 0 || !amilan_packet_valid(packet, codec)) return 0;
    len = AMILAN_HEADER + packet->len;
    if (len > AMILAN_TX_BYTES - (unsigned)p->tx_len) return 0;
    if ((unsigned)p->tx_start + p->tx_len + len > AMILAN_TX_BYTES) {
        for (i = 0; i < p->tx_len; i++) p->tx[i] = p->tx[p->tx_start + i];
        p->tx_start = 0;
    }
    if (!p->tx_len) p->last_progress = now;
    amilan_encode(p->tx + p->tx_start + p->tx_len, AMILAN_TX_BYTES - p->tx_start - p->tx_len, packet, codec);
    p->tx_len = (uint16_t)(p->tx_len + len);
    return 1;
}
int amilan_peer_poll(struct amilan_socket_api *api, struct amilan_peer *p, uint32_t now,
                  amilan_receive_fn receive, void *context, const struct amilan_codec *codec)
{
    unsigned bytes = 0, frames = 0, need, used;
    int n, valid, reason = AMILAN_IO_ERROR;
    struct amilan_packet packet;
    if (p->fd < 0) return 0;
    if (p->connecting) {
        if ((uint32_t)(now - p->born) >= AMILAN_CONNECT_TICKS) { reason = AMILAN_TIMEOUT; goto close; }
        n = amilan_socket_connect_ready(api, p->fd);
        if (n < 0) goto close;
        if (!n) return 1;
        p->connecting = 0; p->last_packet = p->last_progress = now;
    }
    if ((uint32_t)(now - p->last_packet) >= AMILAN_IDLE_TICKS ||
        (p->tx_len && (uint32_t)(now - p->last_progress) >= AMILAN_STALL_TICKS)) {
        reason = AMILAN_TIMEOUT; goto close;
    }
    while (p->tx_len && bytes < AMILAN_POLL_BYTES) {
        need = p->tx_len;
        if (need > AMILAN_POLL_BYTES - bytes) need = AMILAN_POLL_BYTES - bytes;
        n = amilan_socket_send(api, p->fd, p->tx + p->tx_start, need);
        if (n == AMILAN_AGAIN) break;
        if (n <= 0 || (unsigned)n > need) goto close;
        p->tx_start = (uint16_t)(p->tx_start + n); p->tx_len = (uint16_t)(p->tx_len - n);
        if (!p->tx_len) p->tx_start = 0;
        p->last_progress = now; bytes += (unsigned)n;
    }
    bytes = 0;
    while (frames < AMILAN_POLL_FRAMES && bytes < AMILAN_POLL_BYTES) {
        need = AMILAN_HEADER;
        if (p->rx_len >= AMILAN_HEADER) {
            /* decode checks header before trusting a declared length. */
            valid = amilan_decode(&packet, p->rx, p->rx_len, &used, codec);
            if (valid < 0) { reason = AMILAN_BAD_PACKET; goto close; }
            if (valid) {
                p->rx_len = 0; p->last_packet = now; frames++;
                if (!receive || !receive(context, p, &packet)) { reason = AMILAN_REJECTED; goto close; }
                if (p->fd < 0) return 0; /* callback may close deliberately */
                continue;
            }
            need += amilan_get16(p->rx + 4);
        }
        need -= p->rx_len;
        if (need > AMILAN_POLL_BYTES - bytes) need = AMILAN_POLL_BYTES - bytes;
        n = amilan_socket_recv(api, p->fd, p->rx + p->rx_len, need);
        if (n == AMILAN_AGAIN) break;
        if (n < 0 || (unsigned)n > need) goto close;
        if (!n) { reason = p->rx_len ? AMILAN_BAD_PACKET : AMILAN_NORMAL; goto close; }
        p->rx_len = (uint16_t)(p->rx_len + n); bytes += (unsigned)n;
    }
    return 1;
close:
    amilan_peer_close(api, p, reason);
    return 0;
}
