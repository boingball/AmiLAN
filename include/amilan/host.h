/* Extracted fixed connection pool. Application owns handshake/session policy. */
#ifndef AMILAN_HOST_H
#define AMILAN_HOST_H
#include "stream.h"
#define AMILAN_HOST_FULL (-3)
struct amilan_host { struct amilan_socket_api api; int listener; struct amilan_peer peer[AMILAN_CONNECTIONS]; };
typedef int (*amilan_expired_fn)(void *context,unsigned slot,uint32_t now);
void amilan_host_init(struct amilan_host *h);
int amilan_host_open(struct amilan_host *h,uint16_t port);
int amilan_pool_accept(struct amilan_socket_api *api,int listener,struct amilan_peer *peers,unsigned count,uint32_t now);
unsigned amilan_pool_poll(struct amilan_socket_api *api,struct amilan_peer *peers,unsigned count,uint32_t now,amilan_receive_fn receive,amilan_expired_fn expired,void *context,const struct amilan_codec *codec);
int amilan_host_accept(struct amilan_host *h,uint32_t now);
unsigned amilan_host_poll(struct amilan_host *h,uint32_t now,amilan_receive_fn receive,amilan_expired_fn expired,void *context,const struct amilan_codec *codec);
void amilan_host_stop(struct amilan_host *h);
/* Caller owns API lifecycle and peer storage. Queue only after success. */
int amilan_connect(struct amilan_socket_api *api,struct amilan_peer *peer,const uint8_t ip[4],uint16_t port,uint32_t now);
#endif
