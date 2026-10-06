/* Small reusable pieces of the proven link/session policy, no game phases. */
#ifndef AMILAN_LINK_H
#define AMILAN_LINK_H
#include "stream.h"
/* Same wrap-safe deadline used for connect and staged handshake timeouts. */
int amilan_deadline_expired(uint32_t started,uint32_t now,uint32_t duration);
/* 1=handled, 0=queue full/invalid control payload, -1=application message.
 * Application chooses control IDs, framing version and ping schedule.
 * Does not consume outgoing queue capacity unless reply is accepted. */
int amilan_control_receive(struct amilan_peer *peer,const struct amilan_packet *packet,uint32_t now,uint8_t ping_type,uint8_t pong_type,const struct amilan_codec *codec);
#endif
