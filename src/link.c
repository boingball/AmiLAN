#include "amilan/link.h"
int amilan_deadline_expired(uint32_t started,uint32_t now,uint32_t duration){return (uint32_t)(now-started)>=duration;}
int amilan_control_receive(struct amilan_peer *peer,const struct amilan_packet *p,uint32_t now,uint8_t ping,uint8_t pong,const struct amilan_codec *codec)
{
    struct amilan_packet reply;unsigned i;
    if(!ping||!pong||ping==pong)return 0;
    if(p->type!=ping&&p->type!=pong)return -1;
    if(p->len!=4||!amilan_packet_valid(p,codec))return 0;
    if(p->type==pong)return 1;
    reply.type=pong;reply.len=4;for(i=0;i<4;i++)reply.data[i]=p->data[i];
    return amilan_peer_queue(peer,&reply,now,codec);
}
