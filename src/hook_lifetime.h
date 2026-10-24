#pragma once
#include <Windows.h>
#include <TlHelp32.h>
#include <atomic>
#include <vector>

namespace hood {
inline std::atomic<unsigned> activeHooks{0};
struct HookCall {HookCall(){++activeHooks;}~HookCall(){--activeHooks;}};
// Detours owns suspension/resumption until commit/abort. Keep the handles alive.
struct HookThreads {
    std::vector<HANDLE> handles;
    bool insideModule=false;
    ~HookThreads(){for(auto h:handles)CloseHandle(h);}
    bool enroll(uintptr_t moduleBase=0,size_t moduleSize=0){
        HANDLE snapshot=CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD,0);
        if(snapshot==INVALID_HANDLE_VALUE)return false;
        THREADENTRY32 item{};item.dwSize=sizeof(item);bool ok=Thread32First(snapshot,&item)!=FALSE;
        while(ok){
            if(item.th32OwnerProcessID==GetCurrentProcessId()&&item.th32ThreadID!=GetCurrentThreadId()){
                HANDLE thread=OpenThread(THREAD_SUSPEND_RESUME|THREAD_GET_CONTEXT|THREAD_QUERY_INFORMATION,FALSE,item.th32ThreadID);
                if(!thread){if(GetLastError()!=ERROR_INVALID_PARAMETER){CloseHandle(snapshot);return false;}}
                else {
                    handles.push_back(thread);
                    if(DetourUpdateThread(thread)!=NO_ERROR){CloseHandle(snapshot);return false;}
                    CONTEXT context{};context.ContextFlags=CONTEXT_CONTROL;
                    if(!GetThreadContext(thread,&context)){CloseHandle(snapshot);return false;}
                    if(moduleBase&&context.Eip>=moduleBase&&context.Eip-moduleBase<moduleSize)insideModule=true;
                }
            }
            item.dwSize=sizeof(item);if(!Thread32Next(snapshot,&item))break;
        }
        CloseHandle(snapshot);return ok;
    }
};
inline bool matchesCode(uintptr_t base,size_t imageSize,size_t rva,const void* expected,size_t length){
    if(rva>imageSize||length>imageSize-rva)return false;
    __try{return memcmp(reinterpret_cast<void*>(base+rva),expected,length)==0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
}
