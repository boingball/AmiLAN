#include "amilan/zlib.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static uint32_t state=932;
static uint32_t random32(void)
{state^=state<<13;state^=state>>17;state^=state<<5;return state;}
static void fixtures(void)
{
    uint8_t raw[AMILAN_ZLIB_RAW],packed[AMILAN_ZLIB_PACKED+2],out[AMILAN_ZLIB_RAW+2];
    unsigned n,k,i,cap,pattern;
    for(pattern=0;pattern<5;pattern++)for(n=1;n<=AMILAN_ZLIB_RAW;n+=37){
        for(i=0;i<n;i++)raw[i]=pattern==0?0:pattern==1?255:
            pattern==2?(uint8_t)i:pattern==3?(uint8_t)(i%7):(uint8_t)random32();
        memset(packed,0xa5,sizeof(packed));
        k=amilan_zlib_deflate(packed+1,AMILAN_ZLIB_PACKED,raw,n);
        assert(packed[0]==0xa5&&packed[sizeof(packed)-1]==0xa5);
        if(k){
            memset(out,0xa5,sizeof(out));
            assert(amilan_zlib_inflate(out+1,n,packed+1,k));
            assert(!memcmp(raw,out+1,n)&&out[0]==0xa5&&out[n+1]==0xa5);
            for(i=0;i<k;i++)assert(!amilan_zlib_inflate(out+1,n,packed+1,i));
            packed[k]^=1;
            assert(!amilan_zlib_inflate(out+1,n,packed+1,k));
        }
        for(cap=0;cap<32;cap++){
            memset(packed,0xa5,sizeof(packed));
            k=amilan_zlib_deflate(packed+1,cap,raw,n);
            assert(k<=cap&&packed[0]==0xa5&&packed[cap+1]==0xa5);
        }
    }
    memset(raw,0,sizeof(raw));
    k=amilan_zlib_deflate(packed+1,AMILAN_ZLIB_PACKED,raw,sizeof(raw));
    assert(k&&amilan_zlib_inflate(out+1,sizeof(raw),packed+1,k));
    /* An oversized advertised capacity cannot make an undecodable packet. */
    for(i=0;i<sizeof(raw);i++)raw[i]=(uint8_t)random32();
    assert(!amilan_zlib_deflate(packed+1,100000,raw,sizeof(raw)));
    assert(packed[sizeof(packed)-1]==0xa5);
    assert(!amilan_zlib_deflate(0,512,raw,1));
    assert(!amilan_zlib_deflate(packed,512,0,1));
    assert(!amilan_zlib_deflate(packed,512,raw,0));
    assert(!amilan_zlib_deflate(packed,512,raw,4097));
    assert(!amilan_zlib_inflate(0,1,packed,8));
    assert(!amilan_zlib_inflate(out,1,0,8));
    assert(!amilan_zlib_inflate(out,0,packed,8));
    assert(!amilan_zlib_inflate(out,4097,packed,8));
    assert(!amilan_zlib_inflate(out,1,packed,513));
}
static void malformed(void)
{
    unsigned t,i,n,size;
    for(t=0;t<10000;t++){
        uint8_t in[512],out[4098];
        n=random32()%513;size=1+random32()%4096;
        for(i=0;i<n;i++)in[i]=(uint8_t)random32();
        if(n>=3){in[0]=0x78;in[1]=0x01;in[2]=3;}
        memset(out,0xa5,sizeof(out));
        (void)amilan_zlib_inflate(out+1,size,in,n);
        assert(out[0]==0xa5&&out[size+1]==0xa5);
    }
}
int main(void)
{fixtures();malformed();puts("bounded zlib: guarded capacity, roundtrip and 10000 malformed streams passed");return 0;}
