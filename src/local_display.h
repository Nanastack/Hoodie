#pragma once
#include <Windows.h>
#include <cstdint>
namespace hood {
// Verified /displayhead handler and native option getter use this flag.
// Read only: the client/server remains responsible for accepting changes.
inline bool readHeadHidden(uintptr_t pointerSlot,bool& hidden){
    __try{
        uintptr_t settings=*reinterpret_cast<const uintptr_t*>(pointerSlot);
        if(!settings)return false;
        hidden=(*reinterpret_cast<const uint8_t*>(settings+0x2cfdb)&8)!=0;
        return true;
    }__except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
}
