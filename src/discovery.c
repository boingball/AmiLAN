#include "amilan/discovery.h"
static void zero(void *p,unsigned n){uint8_t *b=p;unsigned i;for(i=0;i<n;i++)b[i]=0;}
static void copy(void *to,const void *from,unsigned n){uint8_t *a=to;const uint8_t *b=from;while(n--)*a++=*b++;}
void amilan_discovery_request(uint8_t b[AMILAN_DISC_REQUEST],uint32_t nonce,const struct amilan_discovery_codec *c,unsigned type)
{
    copy(b,c->magic,8);b[8]=c->version;b[9]=c->protocol_version;b[10]=(uint8_t)type;b[11]=0;amilan_put32(b+12,nonce);
}
int amilan_discovery_header_valid(const uint8_t *b,unsigned len,unsigned type,const struct amilan_discovery_codec *c)
{
    unsigned i;if(len<16)return 0;
    for(i=0;i<8;i++)if(b[i]!=c->magic[i])return 0;
    return b[8]==c->version&&b[9]==c->protocol_version&&b[10]==type&&!b[11];
}
int amilan_discovery_open(struct amilan_discovery *d,int hosting,const struct amilan_discovery_codec *codec)
{
    int probe;
    if(!codec||codec->reply_bytes<16||codec->reply_bytes>AMILAN_DISC_REPLY_MAX)return 0;
    zero(d,sizeof(*d));d->fd=-1;d->hosting=hosting;
    if(!amilan_socket_open(&d->api)){d->failed=1;return 0;}
    d->fd=amilan_socket_udp(&d->api,hosting?codec->port:0);
    probe=d->fd>=0?d->fd:amilan_socket_listen(&d->api,0);
    if(probe>=0){d->local_count=amilan_socket_interfaces(&d->api,probe,d->local);if(probe!=d->fd)amilan_socket_close(&d->api,probe);}
    if(d->fd<0)d->failed=1;
    return d->fd>=0;
}
void amilan_discovery_close(struct amilan_discovery *d){amilan_socket_close(&d->api,d->fd);amilan_socket_shutdown(&d->api);d->fd=-1;d->searching=d->hosting=0;}
int amilan_discovery_search(struct amilan_discovery *d,uint32_t now)
{
    if(d->hosting||d->fd<0)return 0;
    d->started=now;d->last_send=now-50;d->nonce+=0x9e3779b9u+now;if(!d->nonce)d->nonce=1;d->searching=1;d->failed=0;return 1;
}
static int unicast(const uint8_t ip[4]){return ip[0]&&ip[0]<224&&!(ip[0]==255&&ip[1]==255&&ip[2]==255&&ip[3]==255);}
void amilan_discovery_poll(struct amilan_discovery *d,uint32_t now,const struct amilan_discovery_codec *codec,void *context)
{
    uint8_t b[AMILAN_DISC_REPLY_MAX+1],ip[4];uint16_t port;int n,i,k;
    if(d->fd<0)return;
    if(d->searching&&(uint32_t)(now-d->started)>=250)d->searching=0;
    if(d->searching&&(uint32_t)(now-d->last_send)>=50){
        int sent=0;d->last_send=now;amilan_discovery_request(b,d->nonce,codec,1);
        for(i=0;i<d->local_count;i++)if(d->local[i].broadcast[0]){
            n=amilan_socket_sendto(&d->api,d->fd,d->local[i].broadcast,codec->port,b,AMILAN_DISC_REQUEST);if(n==AMILAN_DISC_REQUEST)sent=1;
        }
        if(!sent){static const uint8_t broadcast[4]={255,255,255,255};n=amilan_socket_sendto(&d->api,d->fd,broadcast,codec->port,b,AMILAN_DISC_REQUEST);if(n!=AMILAN_DISC_REQUEST&&n!=AMILAN_AGAIN)d->failed=1;}
    }
    for(k=0;k<4;k++){
        n=amilan_socket_recvfrom(&d->api,d->fd,ip,&port,b,sizeof(b));if(n==AMILAN_AGAIN)break;if(n<0){d->failed=1;break;}
        if(!port||!unicast(ip))continue;
        if(d->hosting){
            if(!codec->reply||n!=AMILAN_DISC_REQUEST||!amilan_discovery_header_valid(b,n,1,codec)||(uint32_t)(now-d->last_reply)<13)continue;
            if(codec->reply(b,amilan_get32(b+12),context)){
                d->last_reply=now;amilan_socket_sendto(&d->api,d->fd,ip,port,b,codec->reply_bytes);
            }
        }else if(d->searching&&port==codec->port){
            if(n==codec->reply_bytes&&amilan_discovery_header_valid(b,(unsigned)n,2,codec)&&amilan_get32(b+12)==d->nonce&&codec->found)
                codec->found(b,(unsigned)n,d->nonce,ip,now,context);
        }
    }
    if(!d->hosting&&codec->expire)codec->expire(context,now);
}
