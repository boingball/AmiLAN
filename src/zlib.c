#include "amilan/zlib.h"
static const uint16_t lengths[29]={3,4,5,6,7,8,9,10,11,13,15,17,19,23,27,31,35,43,51,59,67,83,99,115,131,163,195,227,258};
static const uint8_t length_bits[29]={0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3,4,4,4,4,5,5,5,5,0};
static const uint16_t distances[30]={1,2,3,4,5,7,9,13,17,25,33,49,65,97,129,193,257,385,513,769,1025,1537,2049,3073,4097,6145,8193,12289,16385,24577};
static const uint8_t distance_bits[30]={0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8,9,9,10,10,11,11,12,12,13,13};
static uint32_t adler(const uint8_t *p,unsigned n)
{
    uint32_t a=1,b=0;
    while(n--){a+=*p++;if(a>=65521)a-=65521;b+=a;if(b>=65521)b-=65521;}
    return (b<<16)|a;
}
static unsigned reverse(unsigned v,unsigned n)
{unsigned r=0;while(n--){r=(r<<1)|(v&1);v>>=1;}return r;}
struct writer {uint8_t *out;unsigned cap,at,bits,bad;uint32_t value;};
static void put(struct writer *w,unsigned v,unsigned n)
{
    w->value|=(uint32_t)v<<w->bits;w->bits+=n;
    while(w->bits>=8){
        if(w->at>=w->cap){w->bad=1;return;}
        w->out[w->at++]=(uint8_t)w->value;w->value>>=8;w->bits-=8;
    }
}
static void symbol(struct writer *w,unsigned s)
{
    unsigned v,n;
    if(s<144){v=48+s;n=8;}else if(s<256){v=400+s-144;n=9;}
    else if(s<280){v=s-256;n=7;}else{v=192+s-280;n=8;}
    put(w,reverse(v,n),n);
}
static unsigned key(const uint8_t *p) {return (p[0]*17u+p[1]*7u+p[2])&255;}
unsigned amilan_zlib_deflate(uint8_t *out,unsigned cap,const uint8_t *in,unsigned size)
{
    uint16_t latest[256];unsigned i,at=0,k,pos,len,dist,lc,dc,j;uint32_t sum;
    struct writer w;
    if(!out||!in||!size||size>AMILAN_ZLIB_RAW||cap<8)return 0;
    if(cap>AMILAN_ZLIB_PACKED)cap=AMILAN_ZLIB_PACKED;
    for(i=0;i<256;i++)latest[i]=0;
    out[0]=0x78;out[1]=0x01;
    w.out=out;w.cap=cap-4;w.at=2;w.bits=w.bad=0;w.value=0;
    put(&w,3,3); /* BFINAL=1, BTYPE=01. */
    while(at<size&&!w.bad){
        len=0;pos=0;
        if(at+2<size){
            k=key(in+at);pos=latest[k];latest[k]=(uint16_t)(at+1);
            if(pos){pos--;while(len<258&&at+len<size&&in[pos+len]==in[at+len])len++;}
        }
        if(len>=3){
            dist=at-pos;
            for(lc=0;lc<28&&lengths[lc+1]<=len;lc++);
            for(dc=0;dc<29&&distances[dc+1]<=dist;dc++);
            symbol(&w,257+lc);put(&w,len-lengths[lc],length_bits[lc]);
            put(&w,reverse(dc,5),5);put(&w,dist-distances[dc],distance_bits[dc]);
            /* One candidate per hash: bounded search, including overlap runs. */
            for(j=1;j<len&&at+j+2<size;j++)latest[key(in+at+j)]=(uint16_t)(at+j+1);
            at+=len;
        }else{symbol(&w,in[at]);at++;}
    }
    if(w.bad)return 0;
    symbol(&w,256);if(w.bad)return 0;if(w.bits)put(&w,0,8-w.bits);
    if(w.bad)return 0;
    sum=adler(in,size);for(i=0;i<4;i++)out[w.at+i]=(uint8_t)(sum>>(24-i*8));
    return w.at+4;
}
struct reader {const uint8_t *in;unsigned bits,at,bad;};
static unsigned get(struct reader *r,unsigned n)
{
    unsigned v=0,i;
    for(i=0;i<n;i++){
        if(r->at>=r->bits){r->bad=1;return 0;}
        v|=((r->in[r->at>>3]>>(r->at&7))&1u)<<i;r->at++;
    }
    return v;
}
static unsigned read_symbol(struct reader *r)
{
    unsigned code=0,n;
    for(n=1;n<=9;n++){
        code=(code<<1)|get(r,1);
        if(n==7&&code<24)return 256+code;
        if(n==8){if(code>=48&&code<=191)return code-48;if(code>=192&&code<=199)return 280+code-192;}
        if(n==9&&code>=400&&code<=511)return 144+code-400;
    }
    r->bad=1;return 0;
}
int amilan_zlib_inflate(uint8_t *out,unsigned size,const uint8_t *in,unsigned bytes)
{
    struct reader r;unsigned at=0,s,lc,dc,n,d,i;uint32_t sum=0;
    if(!out||!in||!size||size>AMILAN_ZLIB_RAW||bytes<8||bytes>AMILAN_ZLIB_PACKED||in[0]!=0x78||in[1]!=0x01)return 0;
    r.in=in+2;r.bits=(bytes-6)*8;r.at=r.bad=0;
    if(get(&r,3)!=3)return 0;
    for(;;){
        s=read_symbol(&r);if(r.bad)return 0;
        if(s<256){if(at==size)return 0;out[at++]=(uint8_t)s;}
        else if(s==256)break;
        else {
            if(s>285)return 0;
            lc=s-257;n=lengths[lc]+get(&r,length_bits[lc]);
            dc=reverse(get(&r,5),5);if(dc>=30)return 0;
            d=distances[dc]+get(&r,distance_bits[dc]);
            if(r.bad||!d||d>at||n>size-at)return 0;
            for(i=0;i<n;i++){out[at]=out[at-d];at++;}
        }
    }
    if(at!=size||(r.at+7)/8!=bytes-6)return 0;
    for(i=bytes-4;i<bytes;i++)sum=(sum<<8)|in[i];
    return adler(out,size)==sum;
}
