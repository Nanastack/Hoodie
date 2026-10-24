#pragma once
#include <cstdint>
namespace hood {
struct WorldIdentity {
    uint32_t index=0,server=0,zone=0;uintptr_t actor=0;
    bool operator==(const WorldIdentity&)const=default;
};
// Require the same fully loaded character for 1.5 seconds. Reset for every
// logout/zone/character transition; native startup itself runs only once.
struct LoginGate {
    WorldIdentity identity{};uint64_t since=0;bool tracking=false;
    void reset(){tracking=false;identity={};since=0;}
    bool update(bool loaded,WorldIdentity next,uint64_t now,bool allowActorRebuild=false){
        // Zone is useful for detecting transitions, but can be unset in a
        // valid world state. Login/zoning checks and the actor establish readiness.
        if(!loaded||!next.index||next.index>=0x900||!next.server||!next.actor){reset();return false;}
        // Once in-world, race/equipment rebuilds replace the actor without
        // changing character/zone identity. They are not another login.
        if(allowActorRebuild&&tracking&&identity.index==next.index&&identity.server==next.server&&identity.zone==next.zone)identity.actor=next.actor;
        if(!tracking||identity!=next){identity=next;since=now;tracking=true;return false;}
        return now-since>=1500;
    }
};
}
