/* Proven discovery transport with application-selected signature and payload. */
#ifndef AMILAN_DISCOVERY_H
#define AMILAN_DISCOVERY_H
#include "socket.h"
#include "protocol.h"
#define AMILAN_DISC_REQUEST 16
#define AMILAN_DISC_REPLY_MAX 60
#define AMILAN_DISC_HOSTS 4
struct amilan_discovery {
 struct amilan_socket_api api;
 struct amilan_interface local[AMILAN_INTERFACES];
 uint32_t nonce,started,last_send,last_reply;
 int fd,local_count,searching,hosting,failed;
};
/* reply must populate exactly reply_bytes bytes or return 0 without sending.
 * found validates application payload and stores at most four results itself.
 * expire performs application result expiry. These callbacks never allocate. */
struct amilan_discovery_codec {
 uint8_t magic[8],version,protocol_version;
 uint16_t port,reply_bytes;
 int (*reply)(uint8_t *out,uint32_t nonce,void *context);
 void (*found)(const uint8_t *data,unsigned len,uint32_t nonce,const uint8_t ip[4],uint32_t now,void *context);
 void (*expire)(void *context,uint32_t now);
};
void amilan_discovery_request(uint8_t out[AMILAN_DISC_REQUEST],uint32_t nonce,const struct amilan_discovery_codec *codec,unsigned type);
int amilan_discovery_header_valid(const uint8_t *data,unsigned len,unsigned type,const struct amilan_discovery_codec *codec);
int amilan_discovery_open(struct amilan_discovery *d,int hosting,const struct amilan_discovery_codec *codec);
void amilan_discovery_close(struct amilan_discovery *d);
int amilan_discovery_search(struct amilan_discovery *d,uint32_t now);
/* Four datagrams/poll, rate-limited replies, five-second search, broadcast fallback. */
void amilan_discovery_poll(struct amilan_discovery *d,uint32_t now,const struct amilan_discovery_codec *codec,void *context);
#endif
