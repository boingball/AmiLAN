/* Original browser/responder scenarios, opaque application advertisement. */
#include "amilan/discovery.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
struct result {uint8_t payload[44],ip[4];uint32_t seen;};
struct browser {unsigned count;struct result host[AMILAN_DISC_HOSTS];};
static const struct amilan_discovery_codec codec;
static int reply(uint8_t *b,uint32_t nonce,void *context)
{
    amilan_discovery_request(b,nonce,&codec,2);memcpy(b+16,context,44);return 1;
}
static void found(const uint8_t *b,unsigned len,uint32_t nonce,const uint8_t ip[4],uint32_t now,void *context)
{
    struct browser *c=context;unsigned i;(void)nonce;assert(len==60);
    if(!amilan_get16(b+16))return; /* application validation */
    for(i=0;i<c->count;i++)if(!memcmp(c->host[i].ip,ip,4)&&!memcmp(c->host[i].payload,b+16,2))break;
    if(i==c->count){if(c->count==AMILAN_DISC_HOSTS)return;c->count++;}
    memcpy(c->host[i].payload,b+16,44);memcpy(c->host[i].ip,ip,4);c->host[i].seen=now;
}
static void expire(void *context,uint32_t now)
{
    struct browser *c=context;unsigned i=0,k;
    while(i<c->count){if((uint32_t)(now-c->host[i].seen)>=400){for(k=i;k+1<c->count;k++)c->host[k]=c->host[k+1];c->count--;}else i++;}
}
static const struct amilan_discovery_codec codec={{'T','E','S','T','G','A','M','E'},1,42,25568,60,reply,found,expire};
int main(void)
{
    struct amilan_discovery host,client;struct browser results={0};
    uint8_t b[92]={0},payload[44]={0},ip[4],loopback[4]={127,0,0,1};uint16_t port;unsigned i;int n;
    amilan_put16(payload,25567);memcpy(payload+2,"Example opaque application data",31);
    memset(b,0xa5,sizeof(b));amilan_discovery_request(b,7,&codec,1);
    for(i=16;i<sizeof(b);i++)assert(b[i]==0xa5);
    assert(amilan_discovery_header_valid(b,16,1,&codec));b[9]++;assert(!amilan_discovery_header_valid(b,16,1,&codec));b[9]--;
    assert(amilan_discovery_open(&host,1,&codec));assert(amilan_discovery_open(&client,0,&codec));
    assert(client.local_count<=AMILAN_INTERFACES&&amilan_discovery_search(&client,100));
    amilan_discovery_request(b,client.nonce,&codec,1);
    assert(amilan_socket_sendto(&client.api,client.fd,loopback,codec.port,b,16)==16);
    amilan_discovery_poll(&host,100,&codec,payload);amilan_discovery_poll(&client,100,&codec,&results);
    assert(results.count==1&&!memcmp(results.host[0].payload,payload,44));
    assert(amilan_socket_sendto(&client.api,client.fd,loopback,codec.port,b,16)==16);
    n=amilan_socket_recvfrom(&host.api,host.fd,ip,&port,b,sizeof(b));assert(n==16);
    for(i=0;i<6;i++){amilan_put16(payload,(uint16_t)(25567+i));reply(b,client.nonce,payload);assert(amilan_socket_sendto(&host.api,host.fd,ip,port,b,60)==60);}
    amilan_discovery_poll(&client,101,&codec,&results);assert(results.count==4);amilan_discovery_poll(&client,101,&codec,&results);assert(results.count==4);
    uint32_t seen[4];for(i=0;i<4;i++)seen[i]=results.host[i].seen;
    reply(b,client.nonce+1,payload);assert(amilan_socket_sendto(&host.api,host.fd,ip,port,b,60)==60);
    reply(b,client.nonce,payload);assert(amilan_socket_sendto(&host.api,host.fd,ip,port,b,sizeof(b))==(int)sizeof(b));
    amilan_discovery_poll(&client,102,&codec,&results);for(i=0;i<4;i++)assert(results.host[i].seen==seen[i]);
    amilan_discovery_request(b,client.nonce,&codec,1);for(i=0;i<4;i++)assert(amilan_socket_sendto(&client.api,client.fd,loopback,codec.port,b,16)==16);
    amilan_discovery_poll(&host,110,&codec,payload);assert(amilan_socket_recvfrom(&client.api,client.fd,ip,&port,b,sizeof(b))==AMILAN_AGAIN);
    amilan_discovery_poll(&client,350,&codec,&results);assert(!client.searching);amilan_discovery_poll(&client,501,&codec,&results);assert(!results.count);
    amilan_discovery_close(&client);assert(!amilan_discovery_search(&client,600));amilan_discovery_close(&host);amilan_discovery_close(&host);
    assert(amilan_discovery_open(&host,1,&codec));amilan_discovery_close(&host);
    printf("UDP opaque codec/nonce/truncation/four results/rate limit/expiry/reopen passed; transport=%zu bytes\n",sizeof(host));return 0;
}
