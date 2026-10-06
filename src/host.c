#include "amilan/host.h"
void amilan_host_init(struct amilan_host *h)
{
    unsigned i;
    h->api.base=0;h->listener=-1;
    for(i=0;i<AMILAN_CONNECTIONS;i++)amilan_peer_init(&h->peer[i],-1,0,0);
}
int amilan_host_open(struct amilan_host *h,uint16_t port)
{
    if(h->listener>=0||!amilan_socket_open(&h->api))return 0;
    h->listener=amilan_socket_listen(&h->api,port);
    if(h->listener<0){amilan_socket_shutdown(&h->api);return 0;}
    return 1;
}
int amilan_pool_accept(struct amilan_socket_api *api,int listener,struct amilan_peer *peers,unsigned count,uint32_t now)
{
    unsigned i;int fd;
    if(listener<0||count>AMILAN_CONNECTIONS)return -1;
    fd=amilan_socket_accept(api,listener);
    if(fd==AMILAN_AGAIN)return 0;
    if(fd<0)return -1;
    for(i=0;i<count;i++)if(peers[i].fd<0){amilan_peer_init(&peers[i],fd,0,now);return (int)i+1;}
    amilan_socket_close(api,fd);return AMILAN_HOST_FULL;
}
unsigned amilan_pool_poll(struct amilan_socket_api *api,struct amilan_peer *peers,unsigned count,uint32_t now,amilan_receive_fn receive,amilan_expired_fn expired,void *context,const struct amilan_codec *codec)
{
    unsigned i,closed=0;
    if(count>AMILAN_CONNECTIONS)return 0;
    for(i=0;i<count;i++)if(peers[i].fd>=0){
        if(expired&&expired(context,i+1,now))amilan_peer_close(api,&peers[i],AMILAN_TIMEOUT);
        else amilan_peer_poll(api,&peers[i],now,receive,context,codec);
        if(peers[i].fd<0)closed|=1U<<(i+1);
    }
    return closed;
}
int amilan_host_accept(struct amilan_host *h,uint32_t now){return amilan_pool_accept(&h->api,h->listener,h->peer,AMILAN_CONNECTIONS,now);}
unsigned amilan_host_poll(struct amilan_host *h,uint32_t now,amilan_receive_fn receive,amilan_expired_fn expired,void *context,const struct amilan_codec *codec){return amilan_pool_poll(&h->api,h->peer,AMILAN_CONNECTIONS,now,receive,expired,context,codec);}
void amilan_host_stop(struct amilan_host *h)
{
    unsigned i;for(i=0;i<AMILAN_CONNECTIONS;i++)amilan_peer_close(&h->api,&h->peer[i],AMILAN_NORMAL);
    amilan_socket_close(&h->api,h->listener);h->listener=-1;amilan_socket_shutdown(&h->api);
}
int amilan_connect(struct amilan_socket_api *api,struct amilan_peer *p,const uint8_t ip[4],uint16_t port,uint32_t now)
{
    int fd;if(p->fd>=0)return 0;
    fd=amilan_socket_connect(api,ip,port);if(fd<0)return 0;
    amilan_peer_init(p,fd,1,now);return 1;
}
