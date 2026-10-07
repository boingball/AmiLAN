#include "amilan/host.h"
#include "amilan/link.h"
#include "support.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sched.h>
static int expired(void *ctx,unsigned id,uint32_t now){(void)ctx;(void)id;return amilan_deadline_expired(0,now,500);}
static int got;
static int receive(void *context,struct amilan_peer *peer,const struct amilan_packet *p)
{ (void)context; (void)peer; assert(p->type==TEST_PING); got++; return 1; }
static void sockets(void)
{
    struct amilan_socket_api api={0}; struct amilan_peer s,c; struct amilan_packet p={4,TEST_PING,{0}};
    struct sockaddr_in address; socklen_t size=sizeof(address);
    uint8_t ip[4]={127,0,0,1}; unsigned i; int listen, accepted=-1, fd;
    assert(amilan_socket_listen(&api,0)==-1); assert(amilan_socket_open(&api)); listen=amilan_socket_listen(&api,0); assert(listen>=0);
    assert(getsockname(listen,(struct sockaddr *)&address,&size)==0);
    assert(amilan_socket_accept(&api,listen)==AMILAN_AGAIN);
    amilan_peer_init(&c,-1,0,0);assert(amilan_connect(&api,&c,ip,ntohs(address.sin_port),0));fd=c.fd;assert(fd>=0);assert(!amilan_connect(&api,&c,ip,ntohs(address.sin_port),0));
    for(i=0;i<10000 && accepted<0;i++) { accepted=amilan_socket_accept(&api,listen); sched_yield(); }
    assert(accepted>=0); amilan_peer_init(&s,accepted,0,0);
    assert(amilan_peer_queue(&c,&p,0));
    for(i=0;i<10000 && !got;i++) { assert(amilan_peer_poll(&api,&c,0,receive,0)); assert(amilan_peer_poll(&api,&s,0,receive,0)); sched_yield(); }
    assert(got==1); amilan_peer_close(&api,&c,AMILAN_NORMAL);
    for(i=0;i<10000 && s.fd>=0;i++) { amilan_peer_poll(&api,&s,1,receive,0); sched_yield(); }
    assert(s.fd==-1 && s.reason==AMILAN_NORMAL);
    amilan_peer_close(&api,&s,AMILAN_NORMAL); amilan_socket_close(&api,listen); amilan_socket_shutdown(&api);
}
static void slots(void)
{
    struct amilan_host h; struct sockaddr_in address; socklen_t size=sizeof(address);
    uint8_t ip[4]={127,0,0,1}; int fd[AMILAN_CONNECTIONS+1], accepted, i; unsigned attempts;
    amilan_host_init(&h); assert(!h.api.base && h.listener==-1);
    assert(amilan_host_open(&h,0)); assert(!amilan_host_open(&h,0));
    assert(!getsockname(h.listener,(struct sockaddr *)&address,&size));
    /* every slot, then one more: full */
    for(i=0;i<=AMILAN_CONNECTIONS;i++) {
        fd[i]=amilan_socket_connect(&h.api,ip,ntohs(address.sin_port));assert(fd[i]>=0);
        accepted=0;
        for(attempts=0;attempts<10000&&!accepted;attempts++) { accepted=amilan_host_accept(&h,0);sched_yield(); }
        assert(accepted==(i<AMILAN_CONNECTIONS?i+1:AMILAN_HOST_FULL));

    }
    assert(amilan_host_poll(&h,500,receive,expired,0,&test_codec)==((1U<<AMILAN_CONNECTIONS)-1)<<1); /* all handshake deadlines */
    for(i=0;i<=AMILAN_CONNECTIONS;i++)amilan_socket_close(&h.api,fd[i]);
    /* A disconnected slot can be reused without retaining protocol state. */
    fd[0]=amilan_socket_connect(&h.api,ip,ntohs(address.sin_port));assert(fd[0]>=0);
    accepted=0;for(attempts=0;attempts<10000&&!accepted;attempts++) { accepted=amilan_host_accept(&h,501);sched_yield(); }
    assert(accepted==1 && h.peer[0].born==501 && !h.peer[0].rx_len && !h.peer[0].tx_len);
    amilan_socket_close(&h.api,fd[0]);amilan_host_stop(&h);amilan_host_stop(&h);assert(!h.api.base);
}
int main(void){sockets();slots();
 struct amilan_peer p;struct amilan_packet ping={4,TEST_PING,{1,2,3,4}};
 amilan_peer_init(&p,100,0,0);
 assert(amilan_control_receive(&p,&ping,1,TEST_PING,TEST_PONG,&test_codec)==1); /* application-selected pong */
 assert(p.tx_len==12&&p.tx[3]==TEST_PONG);p.tx_len=AMILAN_TX_BYTES;struct amilan_peer before=p;
 assert(!amilan_control_receive(&p,&ping,1,TEST_PING,TEST_PONG,&test_codec)&&!memcmp(&p,&before,sizeof(p)));
 ping.type=TEST_PONG;assert(amilan_control_receive(&p,&ping,1,TEST_PING,TEST_PONG,&test_codec)==1);
 ping.type=TEST_BLOB;assert(amilan_control_receive(&p,&ping,1,TEST_PING,TEST_PONG,&test_codec)==-1);assert(!amilan_deadline_expired(0xfffffff0u,0xfffffff0u+499u,500));
 assert(amilan_deadline_expired(0xfffffff0u,0xfffffff0u+500u,500));
 puts("real TCP, fixed slots, handshake deadline, slot reuse and control validation passed");return 0;}
