#include "amilan/stream.h"
#define TEST_PING 201
#define TEST_BLOB 202
#define TEST_PONG 203
static int test_valid(const struct amilan_packet *p) { return ((p->type==TEST_PING || p->type==TEST_PONG) && p->len==4) || (p->type==TEST_BLOB && p->len==512); }
static const struct amilan_codec test_codec={{'A','L'},1,test_valid};
#define amilan_encode(d,c,p) amilan_encode(d,c,p,&test_codec)
#define amilan_decode(p,s,n,u) amilan_decode(p,s,n,u,&test_codec)
#define amilan_peer_queue(p,n,t) amilan_peer_queue(p,n,t,&test_codec)
#define amilan_peer_poll(a,p,t,r,c) amilan_peer_poll(a,p,t,r,c,&test_codec)
