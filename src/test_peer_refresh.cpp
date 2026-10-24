#include "peer_refresh.h"
#include <cassert>
#include <iostream>
#include <thread>
struct Entity {
    unsigned reads=0,writes=0;uint32_t server=42;uintptr_t raw=1,actor=2;unsigned type=0,race=2;uint16_t body=0x2003,flags=0x8002,locked=0;
    uintptr_t GetRawEntity(unsigned){++reads;return raw;}uintptr_t GetActorPointer(unsigned){++reads;return actor;}
    uint32_t GetServerId(unsigned){++reads;return server;}unsigned GetType(unsigned){++reads;return type;}
    unsigned GetRace(unsigned){++reads;return race;}uint16_t GetLookBody(unsigned){++reads;return body;}
    uint16_t GetActorLockFlag(unsigned){++reads;return locked;}uint16_t GetModelUpdateFlags(unsigned){++reads;return flags;}
    void SetModelUpdateFlags(unsigned,uint16_t value){++writes;flags=value;}
};
int main(){using namespace hood;PendingLooks pending;Entity e;auto up=peerToken(42,0x1003),down=peerToken(42,0);
    assert(up==peerToken(42,0x1009)&&up!=down);
    auto drain=[&](uint64_t now){auto batch=pending.take(now);for(unsigned i=0;i<batch.count;++i){auto index=batch.indices[i];auto token=pending.get(index);if(token&&requestPeerBody(&e,index,token)==PeerRefresh::Discard)pending.complete(index,token);}};
    for(unsigned i=0;i<10000;++i)drain(i);assert(!e.reads&&!e.writes&&pending.empty());
    pending.queue(17,down,100);drain(100);assert(e.flags==0x8006&&e.writes==1&&pending.get(17)==down);
    // Completion acknowledges a native build; merely requesting flags does not.
    for(unsigned i=101;i<200;++i)drain(i);assert(e.writes==1);
    drain(200);drain(300);assert(e.writes==3);drain(400);assert(pending.empty()&&pending.expired==1);
    pending.queue(17,down,500);pending.queue(17,down,550);drain(500);assert(e.writes==4);drain(600);assert(e.writes==5);
    pending.queue(17,up,601);assert(!pending.complete(17,down)&&pending.get(17)==up);assert(pending.complete(17,up)&&pending.empty());
    e.actor=0;pending.queue(17,down,1000);drain(1000);assert(e.writes==5);e.actor=2;drain(1100);assert(e.writes==6);pending.clear();
    e.locked=1;pending.queue(17,up,1200);drain(1200);assert(e.writes==6);e.locked=0;drain(1300);assert(e.writes==7);pending.clear();
    for(unsigned failure=0;failure<4;++failure){e.server=failure==0?43:42;e.type=failure==1?1:0;e.race=failure==2?9:2;e.body=failure==3?0x2004:0x2003;
        pending.queue(17,down,2000);drain(2000);assert(pending.empty()&&e.writes==7);}
    e.server=42;e.type=0;e.race=2;e.body=0x2003;
    pending.queue(17,down,3000);assert(pending.take(5000).count==0&&pending.empty());
    for(unsigned i=1;i<100;++i)pending.queue(i,up,6000);
    unsigned total=0;for(unsigned i=0;i<7;++i){auto batch=pending.take(6000);assert(batch.count<=16);total+=batch.count;for(unsigned j=0;j<batch.count;++j)assert(pending.complete(batch.indices[j],up));}
    assert(total==99&&pending.empty());
    std::thread incoming([&]{for(unsigned i=1;i<PendingLooks::capacity;++i)pending.queue(i,down,7000);});
    for(unsigned i=0;i<10000;++i){auto batch=pending.take(7000);for(unsigned j=0;j<batch.count;++j)pending.complete(batch.indices[j],down);}
    incoming.join();pending.clear();assert(pending.empty());
    e.server=42;e.type=0;e.race=2;e.body=0x202f;e.actor=2;e.locked=0;e.flags=0x8002;
    assert(requestPeerBody(&e,17,up)==PeerRefresh::Requested&&e.flags==0x8006);
    std::cout<<"PASS: native Vermillion body model 47 is eligible for the same event-driven peer refresh as Footman model 3\n";
    std::cout<<"PASS: body bit preserves flags; zero idle entity reads; packet-only refresh, bounded retry/expiry, native-build acknowledgement, stale identity/locked actor guards, coalescing and concurrent bounded batches\n";
}

