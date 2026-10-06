#include "amilan/protocol.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
static int valid(const struct amilan_packet *p){return p->type==250&&p->len==4;}
int main(void)
{
    const struct amilan_codec c={{'X','Y'},42,0},strict={{'X','Y'},42,valid};
    struct amilan_packet p={512,255,{0}},v;uint8_t b[AMILAN_FRAME+1],ip[4]={1,2,3,4};unsigned used,i,j,n;uint32_t random=1234567;
    for(i=0;i<512;i++)p.data[i]=(uint8_t)i;
    assert(amilan_encode(b,sizeof(b),&p,&c)==520&&b[0]=='X'&&b[1]=='Y'&&b[2]==42&&b[3]==255);
    for(i=0;i<520;i++)assert(!amilan_decode(&v,b,i,&used,&c)&&!used);
    assert(amilan_decode(&v,b,521,&used,&c)==1&&used==520&&v.type==255&&!memcmp(v.data,p.data,512));
    assert(amilan_decode(&v,b,520,&used,&strict)==-1&&!used);
    b[2]++;assert(amilan_decode(&v,b,520,&used,&c)==-1);b[2]--;
    b[6]=1;assert(amilan_decode(&v,b,520,&used,&c)==-1);b[6]=0;
    amilan_put16(b+4,513);assert(amilan_decode(&v,b,8,&used,&c)==-1&&!used);
    p.len=513;memset(b,0xa5,sizeof(b));assert(!amilan_encode(b,sizeof(b),&p,&c));for(i=0;i<sizeof(b);i++)assert(b[i]==0xa5);
    p.len=0;p.type=1;assert(amilan_encode(b,sizeof(b),&p,&c)==8&&amilan_decode(&v,b,8,&used,&c)==1);
    p.type=0;assert(!amilan_encode(b,sizeof(b),&p,&c));assert(!amilan_packet_valid(&p,0));
    assert(amilan_get16((const uint8_t *)"\x12\x34")==0x1234);
    assert(amilan_get32((const uint8_t *)"\x89\xab\xcd\xef")==0x89abcdefu);
    assert(amilan_checksum(2166136261u,(const uint8_t *)"hello",5)==0x4f9f2cabu);
    assert(amilan_ipv4("192.168.1.23",ip)&&ip[3]==23);uint8_t saved[4];memcpy(saved,ip,4);
    const char *bad[]={"256.1.2.3","1.2.3","1.2.3.4:80","1..3.4","1.2.3.4 ","0000.2.3.4",0};
    for(i=0;i<sizeof(bad)/sizeof(bad[0]);i++)assert(!amilan_ipv4(bad[i],ip)&&!memcmp(saved,ip,4));
    char text[16];amilan_ipv4_text(text,(const uint8_t[]){255,255,255,255});assert(!strcmp(text,"255.255.255.255"));
    for(i=0;i<20000;i++){
        for(j=0;j<sizeof(b);j++){random=random*1664525u+1013904223u;b[j]=(uint8_t)(random>>24);}
        b[0]='X';b[1]='Y';b[2]=42;b[3]=(uint8_t)i;b[6]=b[7]=0;amilan_put16(b+4,(uint16_t)(i%1024));
        n=i%sizeof(b);int r=amilan_decode(&v,b,n,&used,&c);assert(r>=-1&&r<=1);
        if(r==1)assert(used<=n&&amilan_packet_valid(&v,&c));else assert(!used);
    }
    puts("application framing, version, bounds, endian, IPv4 and 20000 malformed frames passed");return 0;
}
