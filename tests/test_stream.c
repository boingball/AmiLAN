/* Deterministic short I/O, backpressure and error injection. */
#include "support.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static uint8_t input[16384], output[16384];
static unsigned in_len,in_pos,out_len,read_limit,write_limit,send_calls,recv_calls;
static int blocked, ended, failed, connect_result, closed, callbacks;
int amilan_socket_open(struct amilan_socket_api *a) { a->base=a; return 1; }
void amilan_socket_shutdown(struct amilan_socket_api *a) { a->base=0; }
int amilan_socket_listen(struct amilan_socket_api *a,uint16_t p) { (void)a;(void)p;return -1; }
int amilan_socket_accept(struct amilan_socket_api *a,int f) { (void)a;(void)f;return -1; }
int amilan_socket_connect(struct amilan_socket_api *a,const uint8_t i[4],uint16_t p) { (void)a;(void)i;(void)p;return -1; }
int amilan_socket_connect_ready(struct amilan_socket_api *a,int f) { (void)a;(void)f;return connect_result; }
void amilan_socket_close(struct amilan_socket_api *a,int f) { (void)a;(void)f;closed++; }
int amilan_socket_recv(struct amilan_socket_api *a,int f,uint8_t *b,unsigned n)
{
    (void)a;(void)f;recv_calls++;
    if(failed) return -1;
    if(in_pos==in_len) return ended?0:AMILAN_AGAIN;
    if(n>read_limit) n=read_limit;
    if(n>in_len-in_pos) n=in_len-in_pos;
    memcpy(b,input+in_pos,n);in_pos+=n;return (int)n;
}
int amilan_socket_send(struct amilan_socket_api *a,int f,const uint8_t *b,unsigned n)
{
    (void)a;(void)f;send_calls++;
    if(blocked) return AMILAN_AGAIN;
    if(n>write_limit) n=write_limit;
    assert(out_len+n<=sizeof(output));memcpy(output+out_len,b,n);out_len+=n;return (int)n;
}
static int receive(void *c,struct amilan_peer *p,const struct amilan_packet *n)
{ (void)p;(void)n;callbacks++;return c==0; }
static void reset(struct amilan_peer *p)
{
    in_len=in_pos=out_len=send_calls=recv_calls=0;
    read_limit=write_limit=1;blocked=ended=failed=closed=callbacks=0;connect_result=1;
    amilan_peer_init(p,1,0,0);
}
int main(void)
{
    struct amilan_socket_api api={0}; struct amilan_peer p; struct amilan_packet n={0};unsigned i,len;
    n.type=TEST_PING;n.len=4;amilan_put32(n.data,0x12345678);
    reset(&p);len=amilan_encode(input,sizeof(input),&n);in_len=len;
    assert(amilan_peer_queue(&p,&n,0));assert(amilan_peer_poll(&api,&p,0,receive,0));
    assert(callbacks==1 && out_len==len && !memcmp(input,output,len));
    assert(send_calls==len && recv_calls==len+1);
    /* Fragment across separate polls: never deliver incomplete packets. */
    reset(&p);amilan_encode(input,sizeof(input),&n);in_len=7;
    assert(amilan_peer_poll(&api,&p,0,receive,0)&&!callbacks&&p.rx_len==7);
    in_len=11;assert(amilan_peer_poll(&api,&p,1,receive,0)&&!callbacks);
    in_len=12;assert(amilan_peer_poll(&api,&p,2,receive,0)&&callbacks==1);
    reset(&p);for(i=0;i<20;i++) in_len+=amilan_encode(input+in_len,sizeof(input)-in_len,&n);
    assert(amilan_peer_poll(&api,&p,0,receive,0)&&callbacks==AMILAN_POLL_FRAMES);
    assert(amilan_peer_poll(&api,&p,0,receive,0)&&callbacks==16);
    assert(amilan_peer_poll(&api,&p,0,receive,0)&&callbacks==20);
    reset(&p);n.type=TEST_BLOB;n.len=512;memset(n.data,0,512);
    for(i=0;i<4;i++) assert(amilan_peer_queue(&p,&n,0));
    assert(p.tx_len==AMILAN_TX_BYTES&&!amilan_peer_queue(&p,&n,0));
    assert(amilan_peer_poll(&api,&p,0,receive,0)&&out_len==AMILAN_POLL_BYTES&&p.tx_len==32);
    assert(amilan_peer_queue(&p,&n,0)); /* compaction after partial write */
    assert(p.tx_start==0 && p.tx_len==552);
    assert(amilan_peer_poll(&api,&p,0,receive,0)&&out_len==2600&&!p.tx_len);
    reset(&p);n.type=TEST_PING;n.len=4;assert(amilan_peer_queue(&p,&n,0));blocked=1;
    assert(amilan_peer_poll(&api,&p,499,receive,0));assert(!amilan_peer_poll(&api,&p,500,receive,0));assert(p.reason==AMILAN_TIMEOUT&&closed==1);
    amilan_peer_close(&api,&p,AMILAN_NORMAL);assert(closed==1);
    reset(&p);amilan_encode(input,sizeof(input),&n);input[4]=3;in_len=8;
    assert(!amilan_peer_poll(&api,&p,0,receive,0)&&p.reason==AMILAN_BAD_PACKET&&!callbacks);
    reset(&p);amilan_encode(input,sizeof(input),&n);in_len=12;
    assert(!amilan_peer_poll(&api,&p,0,receive,(void *)1)&&p.reason==AMILAN_REJECTED);
    reset(&p);in_len=1;input[0]='A';
    assert(amilan_peer_poll(&api,&p,749,receive,0));assert(!amilan_peer_poll(&api,&p,750,receive,0)&&p.reason==AMILAN_TIMEOUT);
    reset(&p);input[0]='A';in_len=1;ended=1;
    assert(!amilan_peer_poll(&api,&p,0,receive,0)&&p.reason==AMILAN_BAD_PACKET);
    reset(&p);ended=1;assert(!amilan_peer_poll(&api,&p,0,receive,0)&&p.reason==AMILAN_NORMAL);
    reset(&p);failed=1;assert(!amilan_peer_poll(&api,&p,0,receive,0)&&p.reason==AMILAN_IO_ERROR);
    reset(&p);p.connecting=1;connect_result=0;
    assert(amilan_peer_poll(&api,&p,499,receive,0));assert(!amilan_peer_poll(&api,&p,500,receive,0)&&p.reason==AMILAN_TIMEOUT);
    reset(&p);p.connecting=1;connect_result=-1;assert(!amilan_peer_poll(&api,&p,0,receive,0));
    reset(&p);p.born=p.last_packet=0xfffffff0u;
    assert(amilan_peer_poll(&api,&p,0xfffffff0u+749u,receive,0));assert(!amilan_peer_poll(&api,&p,0xfffffff0u+750u,receive,0));
    puts("bounded stream/short I/O/timeout tests passed");return 0;
}
