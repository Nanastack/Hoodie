#pragma once
#include "pending_looks.h"
#include "normal_armors.h"
namespace hood {
inline uint64_t peerToken(uint32_t server,uint16_t head){return (uint64_t(server)<<32)|0x10000u|(head?1u:0u);}
enum class PeerRefresh {Retry,Discard,Requested};
template<class Entities> PeerRefresh requestPeerBody(Entities* e,unsigned index,uint64_t token){
    if(!e->GetRawEntity(index)||!e->GetActorPointer(index))return PeerRefresh::Retry;
    auto race=e->GetRace(index);
    if(e->GetServerId(index)!=(uint32_t)(token>>32)||e->GetType(index)!=0||race<1||race>8||!normal::supportedBody(e->GetLookBody(index)))return PeerRefresh::Discard;
    if(e->GetActorLockFlag(index))return PeerRefresh::Retry;
    e->SetModelUpdateFlags(index,refreshBody(e->GetModelUpdateFlags(index)));
    return PeerRefresh::Requested;
}
}
