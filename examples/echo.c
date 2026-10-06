/* Minimal two-process Linux example. All peer storage is fixed, no heap. */
#define _POSIX_C_SOURCE 200809L
#include "amilan/host.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <signal.h>
#include <stdlib.h>
static volatile sig_atomic_t running=1;
static int hosting,received;
static const struct amilan_codec codec={{'E','X'},1,255,0};
static uint32_t ticks(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return (uint32_t)((uint64_t)t.tv_sec*50+t.tv_nsec/20000000);}
static void stop(int signal_number){(void)signal_number;running=0;}
static int receive(void *ctx,struct amilan_peer *peer,const struct amilan_packet *p)
{
    (void)ctx;if(p->type!=1||p->len!=4)return 0;
    if(hosting)return amilan_peer_queue(peer,p,ticks(),&codec);
    if(memcmp(p->data,"echo",4))return 0;
    received=1;puts("echo received");return 1;
}
int main(int argc,char **argv)
{
    struct amilan_host host;struct amilan_peer client;struct amilan_socket_api api={0};uint8_t ip[4];
    struct timespec pause={0,2000000};uint32_t start=ticks();unsigned port=argc>2?(unsigned)strtoul(argv[2],0,10):25569;int result=0;
    if(argc<2||!port||port>65535){fprintf(stderr,"usage: echo host|IPv4 [port]\n");return 2;}
    hosting=!strcmp(argv[1],"host");signal(SIGINT,stop);signal(SIGTERM,stop);
    if(hosting){amilan_host_init(&host);if(!amilan_host_open(&host,(uint16_t)port))return 1;}
    else{
        if(!amilan_ipv4(argv[1],ip)||!amilan_socket_open(&api))return 1;
        amilan_peer_init(&client,-1,0,start);
        if(!amilan_connect(&api,&client,ip,(uint16_t)port,start)){amilan_socket_shutdown(&api);return 1;}
        struct amilan_packet p={4,1,{'e','c','h','o'}};
        if(!amilan_peer_queue(&client,&p,start,&codec))result=1;
    }
    while(running&&!result&&(uint32_t)(ticks()-start)<3000){
        uint32_t now=ticks();
        if(hosting){if(amilan_host_accept(&host,now)==-1){result=1;break;}amilan_host_poll(&host,now,receive,0,0,&codec);}
        else{if(!amilan_peer_poll(&api,&client,now,receive,0,&codec)){result=1;break;}if(received)break;}
        nanosleep(&pause,0);
    }
    if(hosting)amilan_host_stop(&host);
    else{amilan_peer_close(&api,&client,AMILAN_NORMAL);amilan_socket_shutdown(&api);if(!received)result=1;}
    return result;
}
