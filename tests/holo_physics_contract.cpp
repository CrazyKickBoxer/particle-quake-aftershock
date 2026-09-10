#include "holo_physics_contract.h"
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <initializer_list>
#include <cmath>
#define CHECK(x) do { if(!(x)) { std::fprintf(stderr,"line %d: %s\n",__LINE__,#x); return 1; } } while(0)
int main() {
    hp_queue_t q={}; hp_impulse_t e={}; e.radius=128; e.strength=1; e.duration=1.2f;
    for(unsigned i=0;i<64;++i) { e.strength=float(i+1); CHECK(HP_QueuePush(&q,e)); }
    e.strength=.5f; CHECK(!HP_QueuePush(&q,e)); CHECK(q.count==64 && q.dropped==1);
    e.strength=100; CHECK(HP_QueuePush(&q,e)); CHECK(q.count==64 && q.dropped==2);
    CHECK(q.events[0].strength==100);
    e.origin[1]=std::numeric_limits<float>::quiet_NaN(); CHECK(!HP_QueuePush(&q,e));
    hp_impulse_t out[64]; CHECK(HP_QueueDrain(&q,out)==64 && q.count==0);
    CHECK(q.serial==66); // invalid events do not consume private randomness
    for(float dt: {.001f,.008f,.016f,.033f,.05f}) {
        float x=30,v=180;
        for(float t=0;t<3;t+=dt) HP_SpringReference(&x,&v,10,dt);
        CHECK(std::isfinite(x) && std::isfinite(v)); CHECK(std::fabs(x)<.001f && std::fabs(v)<.01f);
    }
    float x=30,v=180; HP_SpringReference(&x,&v,10,0); CHECK(x==30 && v==180);
    std::puts("Holo contracts: layouts, bounded priority queue, private seeds, pause, spring convergence passed");
    return 0;
}
