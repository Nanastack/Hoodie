// Per-actor tunic variants; private native resources, unchanged retail model IDs.
#include "Ashita.h"
#include "detours.h"
#include "routing.h"
#include "asset_store.h"
#include "hook_lifetime.h"
#include "pending_looks.h"
#include "peer_refresh.h"
#include "local_display.h"
#include "login_gate.h"


#include "normal_armors.h"
#include "hood_gesture.h"
#include "preferences.h"
#include <atomic>
#include <intrin.h>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <array>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <vector>
#include <deque>
using OpenA=decltype(&CreateFileA);
static OpenA realOpen=CreateFileA;
using FindA=decltype(&FindFirstFileA);static FindA realFind=FindFirstFileA;
static std::atomic<unsigned> reads{0};
static std::atomic<bool> captureEnabled{false};static std::mutex reportMutex;
static uintptr_t engineBase=0,engineEnd=0;
static thread_local bool inside=false;
static IAshitaCore* liveCore=nullptr;static DWORD coreThread=0;
static std::atomic<bool> shuttingDown{false},reloadRequiresRestart{false};
static std::atomic<bool> worldReady{false};
using DisplayRequestFn=bool(__cdecl*)(unsigned,unsigned);
static DisplayRequestFn displayRequest=nullptr;
static bool displayRequestHooked=false;
static std::atomic<unsigned> nativeHeadRequests{0};
static void* nativeGestureOwner=nullptr;
static bool (*nativeGestureDefer)(void*,bool)=nullptr;
static bool __cdecl displayRequestHook(unsigned option,unsigned value){
    hood::HookCall active;
    if(option==10&&!shuttingDown.load()){
        ++nativeHeadRequests;
        // Native displayhead toggling writes CL then pushes ECX. Upper bits
        // need not be cleared; the native callee tests only the low byte.
        if(nativeGestureOwner&&nativeGestureDefer&&nativeGestureDefer(nativeGestureOwner,uint8_t(value)!=0))return true;
    }
    return displayRequest(option,value);
}
static HMODULE retainedModule=nullptr;
static decltype(&DetourDetach) detachHookApi=DetourDetach;
struct Sample {DWORD thread=0;void* frames[12]={};USHORT count=0;ULONGLONG bytes=0;bool metadata=false,success=false;char actual[MAX_PATH]={};uintptr_t candidates[48]={};USHORT candidateCount=0;bool request=false;uint32_t resourceKey=0;uintptr_t context=0,caller=0,owner=0,baselineOwner=0;bool lookup=false,attachment=false,released=false,slotRelease=false;uint32_t modelCode=0,stamp=0;uintptr_t result=0,handle=0,resourceObject=0;uint32_t objectKey=0;bool handleReadable=false,objectReadable=false;};
static std::array<Sample,2048> samples;
static SRWLOCK sampleLock=SRWLOCK_INIT;
static void record(Sample sample){
    if(!captureEnabled.load())return;
    sample.thread=GetCurrentThreadId();sample.count=CaptureStackBackTrace(1,12,sample.frames,nullptr);
    // Bounded raw stack scan supplements x86 unwinding through frame-pointer-omitted code.
    // These are potential return addresses, not validated call frames.
    auto tib=reinterpret_cast<NT_TIB*>(NtCurrentTeb());
    auto begin=reinterpret_cast<uintptr_t*>(_AddressOfReturnAddress());
    auto top=reinterpret_cast<uintptr_t*>(tib->StackBase);
    size_t limit=top>begin?static_cast<size_t>(top-begin):0;if(limit>1024)limit=1024;
    for(size_t k=0;k<limit&&sample.candidateCount<48;++k){
        auto value=begin[k];if(value<engineBase||value>=engineEnd)continue;
        MEMORY_BASIC_INFORMATION info{};
        if(!VirtualQuery(reinterpret_cast<void*>(value),&info,sizeof(info)))continue;
        if(info.State!=MEM_COMMIT || !(info.Protect&(PAGE_EXECUTE|PAGE_EXECUTE_READ|PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY)))continue;
        bool duplicate=false;for(unsigned j=0;j<sample.candidateCount;++j)if(sample.candidates[j]==value)duplicate=true;
        if(!duplicate)sample.candidates[sample.candidateCount++]=value;
    }
    AcquireSRWLockExclusive(&sampleLock);auto n=reads.load();if(n<samples.size()){samples[n]=sample;reads=n+1;}else captureEnabled=false;ReleaseSRWLockExclusive(&sampleLock);
}
using RequestFn=void*(__thiscall*)(void*,void**,uint32_t);
static RequestFn asyncRequest=nullptr,syncRequest=nullptr;
static bool requestHooks=false;
static RequestFn cacheRequest=nullptr;
static std::atomic<uintptr_t> actorBaseline{0};
#pragma pack(push,4)
struct EntryFrame {uint32_t edi,esi,ebp,esp,ebx,edx,ecx,eax,flags,caller,output,key;};
#pragma pack(pop)
static bool readPointer(uintptr_t address,void* destination,size_t bytes){
    __try{memcpy(destination,(const void*)address,bytes);return true;}
    __except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
static uintptr_t currentLocalActor(){
    if(!liveCore)return actorBaseline.load();
    if(!worldReady.load())return 0;
    if(GetCurrentThreadId()!=coreThread)return 0;
    auto index=liveCore->GetMemoryManager()->GetParty()->GetMemberTargetIndex(0);
    if(!index||index>=0x900)return 0;
    return liveCore->GetMemoryManager()->GetEntity()->GetActorPointer(index);
}
using AttachFn=uintptr_t(__thiscall*)(void*,void*,uint32_t,uint32_t,uint32_t);
static AttachFn attachResource=nullptr;static bool attachmentHooked=false;
static uintptr_t __fastcall attachHook(void* actor,void*,void* handle,uint32_t modelCode,uint32_t stamp,uint32_t mode){
    hood::HookCall active;
    if(!captureEnabled.load())return attachResource(actor,handle,modelCode,stamp,mode);
    Sample sample;sample.attachment=true;sample.owner=(uintptr_t)actor;sample.baselineOwner=currentLocalActor();sample.caller=(uintptr_t)_ReturnAddress();sample.handle=(uintptr_t)handle;sample.modelCode=modelCode;sample.stamp=stamp;
    sample.handleReadable=handle&&readPointer((uintptr_t)handle,&sample.resourceObject,sizeof(sample.resourceObject));
    sample.objectReadable=sample.handleReadable&&sample.resourceObject&&readPointer(sample.resourceObject+0x34,&sample.objectKey,sizeof(sample.objectKey));
    auto result=attachResource(actor,handle,modelCode,stamp,mode);DWORD error=GetLastError();sample.result=result;sample.success=true;record(sample);SetLastError(error);return result;
}
using ParseFn=void*(__thiscall*)(void*,void**,const uint8_t*,uintptr_t);
static ParseFn parseModel=nullptr;static bool parserVerified=false;
static std::unique_ptr<hood::Assets> variantAssets,taruAssets,mithraAssets,humeMaleAssets,elvaanMaleAssets,elvaanFemaleAssets,galkaAssets;
static std::array<std::unique_ptr<hood::Assets>,8> vermillionAssets;
static std::array<std::array<std::unique_ptr<hood::Assets>,8>,3> cloakAssets;

static bool supportedRace(unsigned race){return race==1||race==2||race==3||race==4||race==5||race==6||race==7||race==8;}
static std::atomic<bool> variantsEnabled{false},othersEnabled{false};
static std::atomic<bool> gestureEnabled{false};
static std::mutex gesturePackageMutex;
static hood::gesture::Library gesturePackages;
static std::atomic<uintptr_t> gestureLoadedActor{0};
static std::atomic<unsigned> gestureLoadedBody{0},gestureLoadedRace{0};

static std::atomic<unsigned> peerUp{0},peerDown{0},peerIndex{0},peerHead{0};
static std::array<std::atomic<uint64_t>,0x900> receivedLooks{};
static std::atomic<unsigned> modelPackets{0},headChanges{0},packetHead{0},packetIndex{0},updateRequests{0};
static std::atomic<unsigned> refreshAcknowledged{0};
static hood::PendingLooks pendingLooks;
static std::atomic<hood::Variant> localSelection{hood::Variant::Up};
static std::atomic<bool> localSelectionKnown{false};
static bool observedHeadHidden=false,displaySettingVerified=false;
static bool adoptLocalDisplay(bool hidden){
    bool known=localSelectionKnown.load();
    if(known&&hidden==observedHeadHidden)return false;
    observedHeadHidden=hidden;auto wanted=hidden?hood::Variant::Down:hood::Variant::Up;
    bool changed=localSelection.exchange(wanted)!=wanted;localSelectionKnown=true;
    return !known||changed;
}
static bool anyAssets(){if(variantAssets||taruAssets||mithraAssets||humeMaleAssets||elvaanMaleAssets||elvaanFemaleAssets||galkaAssets)return true;for(const auto& a:vermillionAssets)if(a)return true;for(const auto& armor:cloakAssets)for(const auto& a:armor)if(a)return true;return false;}
static void syncLocalSelection(){
    if(!worldReady.load()||!liveCore||GetCurrentThreadId()!=coreThread)return;
    auto index=liveCore->GetMemoryManager()->GetParty()->GetMemberTargetIndex(0);
    if(!index||index>=0x900)return;
    auto e=liveCore->GetMemoryManager()->GetEntity();if(!e->GetRawEntity(index)||!e->GetActorPointer(index))return;
    bool hidden=false;
    if(!displaySettingVerified||!hood::readHeadHidden(engineBase+0x4dfd98,hidden))return;
    if(adoptLocalDisplay(hidden)&&variantsEnabled.load()&&supportedRace(e->GetRace(index))&&hood::normal::supportedBody(e->GetLookBody(index)))
        e->SetModelUpdateFlags(index,hood::refreshBody(e->GetModelUpdateFlags(index)));
}
static void drainPeerRefreshes(){
    if(pendingLooks.empty())return;
    if(shuttingDown.load()||!worldReady.load()||!variantsEnabled.load()||!othersEnabled.load()||!liveCore||GetCurrentThreadId()!=coreThread)return;
    auto local=liveCore->GetMemoryManager()->GetParty()->GetMemberTargetIndex(0);
    auto e=liveCore->GetMemoryManager()->GetEntity();auto batch=pendingLooks.take(GetTickCount64());
    for(unsigned item=0;item<batch.count;++item){unsigned index=batch.indices[item];
        auto look=pendingLooks.get(index);if(!look)continue;
        if(index==local){pendingLooks.complete(index,look);continue;}
        auto result=hood::requestPeerBody(e,index,look);
        if(result==hood::PeerRefresh::Discard)pendingLooks.complete(index,look);
        else if(result==hood::PeerRefresh::Requested)++updateRequests;
        // A request is acknowledged by a matching body build, not a flag write.
    }
}

static std::atomic<uint32_t> nextPrivateTag{0xf0000000u};
static std::atomic<unsigned> customBuilds{0},customFailures{0};
static std::atomic<unsigned> observedBody{0},observedRace{0},eligibleRequests{0};
static bool tagPrivateRoot(uintptr_t object,uint32_t tag){
    __try{*reinterpret_cast<uint32_t*>(object+0x34)=tag;return true;}
    __except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
static bool constructVariant(const hood::Lease& lease,void* manager,void** output,uint32_t tag){
    // Match the observed synchronous parser path: caller-owned output, full DAT
    // bytes, null optional owner. FFXI copies DAT sections and owns the result.
    void* handle=nullptr;
    parseModel(manager,&handle,lease.blob->bytes.data(),0);
    uintptr_t root=0;
    if(!handle||!readPointer((uintptr_t)handle,&root,sizeof(root))||!root)return false;
    if(!tagPrivateRoot(root,tag))return false;
    *output=handle;return true;
}
static bool tryVariant(EntryFrame* frame){
    if(shuttingDown.load()||!worldReady.load()||!variantsEnabled.load()||!parserVerified||GetCurrentThreadId()!=coreThread||!liveCore)return false;
    if(!hood::normal::bodyResource(frame->key)||frame->caller-engineBase!=0xd3e1d)return false;
    auto index=liveCore->GetMemoryManager()->GetParty()->GetMemberTargetIndex(0);
    if(!index||index>=0x900)return false;
    auto entity=liveCore->GetMemoryManager()->GetEntity();
    uintptr_t local=entity->GetActorPointer(index), verified=local;
    bool peer=frame->edi!=local;uint64_t peerLook=0;
    syncLocalSelection();hood::Variant wanted=localSelection.load();
    if(peer){
        if(!othersEnabled.load()||!frame->edi)return false;
        unsigned found=0;
        for(unsigned i=1;i<0x900;++i){
            if(i==index||!entity->GetRawEntity(i))continue;
            if(entity->GetActorPointer(i)==frame->edi&&entity->GetServerId(i)&&entity->GetType(i)==0){found=i;break;}
        }
        if(!found)return false;
        index=found;verified=entity->GetActorPointer(index);
        // Preserve the full head value: zero means hidden; 0x1000 is a visible empty head slot.
        auto head=entity->GetLookHead(index);
        auto received=receivedLooks[index].load();
        if((uint32_t)(received>>32)==entity->GetServerId(index)&&(received&0x10000))head=(uint16_t)received;
        wanted=head==0?hood::Variant::Down:hood::Variant::Up;
        peerLook=hood::peerToken(entity->GetServerId(index),head);
        peerIndex=index;peerHead=head;
    }
    observedBody=entity->GetLookBody(index);observedRace=entity->GetRace(index);
    auto race=observedRace.load();
    if(hood::normal::resource(race,uint16_t(observedBody.load()))!=frame->key)return false;
    hood::Assets* assets=race==2&&frame->key==10547?variantAssets.get():(race==5||race==6)&&frame->key==20075?taruAssets.get():race==7&&frame->key==23475?mithraAssets.get():race==1&&frame->key==7371?humeMaleAssets.get():race==3&&frame->key==13723?elvaanMaleAssets.get():race==4&&frame->key==16899?elvaanFemaleAssets.get():race==8&&frame->key==26651?galkaAssets.get():nullptr;
    if((observedBody.load()&0xfff)==hood::normal::vermillion)assets=vermillionAssets[hood::normal::raceIndex(race)].get();
    auto armor=hood::normal::armorIndex(uint16_t(observedBody.load()));
    if(armor>=2)assets=cloakAssets[armor-2][hood::normal::raceIndex(race)].get();

    if(!assets)return false;
    auto lease=assets->beginFor(frame->edi,verified,frame->key,(uint32_t)(frame->caller-engineBase),wanted);
    if(!lease)return false;
    std::shared_ptr<const hood::gesture::Package> motion;
    // Gesture resources are inert until played. Prepare both hood variants even
    // when Animate change is off, so its checkbox never needs a model rebuild.
    if(!peer){
        std::lock_guard<std::mutex> lock(gesturePackageMutex);motion=gesturePackages.forRace(race);
        if(motion&&motion->race==(race==6?5:race)){try{lease=motion->decorate(lease);}catch(const std::exception&){gestureEnabled=false;motion.reset();}}else motion.reset();
    }
    ++eligibleRequests;
    // Both variants use private resources so an existing down resource cannot satisfy an up request.
    uint32_t tag=nextPrivateTag.fetch_add(1);if(tag<0xf0000000u||tag==0xffffffffu)return false;
    if(!constructVariant(*lease,(void*)frame->ecx,(void**)frame->output,tag)){++customFailures;return false;}
    frame->eax=frame->output;++customBuilds;
    if(motion){gestureLoadedActor=frame->edi;gestureLoadedBody=observedBody.load();gestureLoadedRace=race==6?5:race;}
    if(peer){if(wanted==hood::Variant::Down)++peerDown;else ++peerUp;if(pendingLooks.complete(index,peerLook))++refreshAcknowledged;}
    if(captureEnabled.load()){
        Sample sample;sample.lookup=true;sample.request=true;sample.success=true;sample.owner=frame->edi;sample.baselineOwner=local;sample.resourceKey=frame->key;sample.caller=frame->caller;sample.bytes=lease->size();sample.objectKey=tag;record(sample);
    }
    return true;
}
static void __cdecl captureCache(EntryFrame* frame){
    hood::HookCall active;
    try{if(tryVariant(frame))return;}
    catch(const std::exception&){++customFailures;variantsEnabled=false;}
    if(!captureEnabled.load()){frame->eax=(uint32_t)cacheRequest((void*)frame->ecx,(void**)frame->output,frame->key);return;}
    Sample sample;sample.request=true;sample.lookup=true;sample.resourceKey=frame->key;
    sample.context=frame->ecx;sample.caller=frame->caller;sample.owner=frame->edi;sample.baselineOwner=currentLocalActor();
    auto result=cacheRequest((void*)frame->ecx,(void**)frame->output,frame->key);DWORD error=GetLastError();
    frame->eax=(uint32_t)result;sample.result=(uintptr_t)result;
    sample.handleReadable=readPointer(frame->output,&sample.handle,sizeof(sample.handle));
    if(sample.handleReadable&&sample.handle){
        sample.objectReadable=readPointer(sample.handle,&sample.resourceObject,sizeof(sample.resourceObject))&&sample.resourceObject!=0;
        if(sample.objectReadable&&sample.resourceObject)sample.objectReadable=readPointer(sample.resourceObject+0x34,&sample.objectKey,sizeof(sample.objectKey));
    }
    sample.success=true;record(sample);SetLastError(error);
}
// Preserve the original entry registers, flags, stack arguments and return path.
__declspec(naked) static void cacheHook(){
    __asm {
        pushfd
        pushad
        push esp
        call captureCache
        add esp,4
        popad
        popfd
        ret 8
    }
}
static void* __fastcall asyncHook(void* context,void*,void** output,uint32_t key){
    hood::HookCall active;
    if(!captureEnabled.load())return asyncRequest(context,output,key);
    Sample sample;sample.request=true;sample.resourceKey=key;sample.context=(uintptr_t)context;sample.caller=(uintptr_t)_ReturnAddress();
    auto result=asyncRequest(context,output,key);DWORD error=GetLastError();sample.success=true;record(sample);SetLastError(error);return result;
}
static void* __fastcall syncHook(void* context,void*,void** output,uint32_t key){
    hood::HookCall active;
    if(!captureEnabled.load())return syncRequest(context,output,key);
    Sample sample;sample.request=true;sample.resourceKey=key;sample.context=(uintptr_t)context;sample.caller=(uintptr_t)_ReturnAddress();
    auto result=syncRequest(context,output,key);DWORD error=GetLastError();sample.success=true;record(sample);SetLastError(error);return result;
}
static HANDLE WINAPI openHook(LPCSTR p,DWORD a,DWORD s,LPSECURITY_ATTRIBUTES sa,DWORD c,DWORD f,HANDLE t){
    hood::HookCall active;
    HANDLE h=realOpen(p,a,s,sa,c,f,t);DWORD error=GetLastError();
    if(captureEnabled.load() && !inside && p && footmanPath(p)){
        inside=true;Sample sample;sample.success=h!=INVALID_HANDLE_VALUE;
        if(sample.success){LARGE_INTEGER size{};if(GetFileSizeEx(h,&size))sample.bytes=size.QuadPart;GetFinalPathNameByHandleA(h,sample.actual,MAX_PATH,FILE_NAME_NORMALIZED);}
        record(sample);inside=false;
    }
    SetLastError(error);return h;
}
static HANDLE WINAPI findHook(LPCSTR p,LPWIN32_FIND_DATAA data){
    hood::HookCall active;
    HANDLE h=realFind(p,data);DWORD error=GetLastError();
    if(captureEnabled.load() && !inside && p && footmanPath(p)){
        inside=true;Sample sample;sample.metadata=true;sample.success=h!=INVALID_HANDLE_VALUE;
        if(sample.success&&data)sample.bytes=((ULONGLONG)data->nFileSizeHigh<<32)|data->nFileSizeLow;
        record(sample);inside=false;
    }
    SetLastError(error);return h;
}
using ReleaseFn=uintptr_t(__thiscall*)(void*,void*);
static ReleaseFn releaseResource=nullptr,releaseSlot=nullptr;static bool releaseHooks=false;
static uintptr_t __fastcall resourceReleaseHook(void* context,void*,void* object){
    hood::HookCall active;
    if(!captureEnabled.load())return releaseResource(context,object);
    Sample sample;sample.released=true;sample.context=(uintptr_t)context;sample.resourceObject=(uintptr_t)object;sample.caller=(uintptr_t)_ReturnAddress();
    sample.objectReadable=object&&readPointer((uintptr_t)object+0x34,&sample.objectKey,4);
    auto result=releaseResource(context,object);DWORD error=GetLastError();sample.result=result;record(sample);SetLastError(error);return result;
}
static uintptr_t __fastcall slotReleaseHook(void* context,void*,void* handle){
    hood::HookCall active;
    if(!captureEnabled.load())return releaseSlot(context,handle);
    Sample sample;sample.slotRelease=true;sample.context=(uintptr_t)context;sample.handle=(uintptr_t)handle;sample.caller=(uintptr_t)_ReturnAddress();
    sample.handleReadable=handle&&readPointer((uintptr_t)handle,&sample.resourceObject,sizeof(sample.resourceObject));
    sample.objectReadable=sample.handleReadable&&sample.resourceObject&&readPointer(sample.resourceObject+0x34,&sample.objectKey,4);
    auto result=releaseSlot(context,handle);DWORD error=GetLastError();sample.result=result;record(sample);SetLastError(error);return result;
}
static bool writeCapture(const std::string& logPath){
        std::lock_guard<std::mutex> guard(reportMutex);
        std::vector<Sample> snapshot(samples.size());unsigned n;
        AcquireSRWLockShared(&sampleLock);std::copy(samples.begin(),samples.end(),snapshot.begin());n=reads.load();ReleaseSRWLockShared(&sampleLock);
        std::ofstream file(logPath);file<<"Capture state: "<<(captureEnabled.load()?"RECORDING":"STOPPED")<<"; limit 60 seconds / 2048 events.\n";file<<"Hoodie 0.49 DIAGNOSTIC MODE\nNo file redirects or model-ID writes. Native model-update flag follows the accepted hidden-head setting. Variant mode defaults on when parser signatures and assets verify.\nTarget ROM/32/114.DAT file events: "<<n<<"\n";
        file<<"Variant enabled="<<variantsEnabled.load()<<" selected="<<(localSelection.load()==hood::Variant::Down?"down":"up")<<" parser="<<parserVerified<<" assets="<<(bool)variantAssets<<" builds="<<customBuilds.load()<<" failures="<<customFailures.load()<<" eligible="<<eligibleRequests.load()<<" race="<<observedRace.load()<<" body="<<observedBody.load()<<"\n";
        file<<"Client display setting signature="<<displaySettingVerified<<" known="<<localSelectionKnown.load()<<"\n";
        file<<"Normal equipped armor routes: Royal Footman model 3; Vermillion model 47. Temporary GUI preview is independent.\n";


        file<<"Other players enabled="<<othersEnabled.load()<<" up="<<peerUp.load()<<" down="<<peerDown.load()<<" lastIndex="<<peerIndex.load()<<" rawHead="<<peerHead.load()<<"\n";
        file<<"Peer model packets="<<modelPackets.load()<<" head changes="<<headChanges.load()<<" refresh requests="<<updateRequests.load()<<" body acknowledgements="<<refreshAcknowledged.load()<<" pending="<<pendingLooks.size()<<" expired="<<pendingLooks.expired.load()<<" packetIndex="<<packetIndex.load()<<" packetHead="<<packetHead.load()<<"\n";
        for(unsigned k=0;k<std::min(n,2048u);++k){const auto& sample=snapshot[k];file<<"\n"<<(sample.released?"RESOURCE_RELEASE":sample.slotRelease?"SLOT_RELEASE":sample.attachment?"ATTACHMENT":sample.lookup?"CACHE_LOOKUP":sample.request?"RESOURCE_REQUEST":sample.metadata?"METADATA":"OPEN")<<" success="<<sample.success<<" bytes="<<sample.bytes<<" thread="<<sample.thread<<" actual="<<sample.actual<<"\n";if(sample.request||sample.attachment||sample.released||sample.slotRelease)file<<"resourceKey="<<sample.resourceKey<<" context=0x"<<std::hex<<sample.context<<" ownerCandidate=0x"<<sample.owner<<" localBaseline=0x"<<sample.baselineOwner<<" callerRva=0x"<<(sample.caller-engineBase)<<std::dec<<"\n";
            if(sample.lookup||sample.attachment||sample.released||sample.slotRelease)file<<"return=0x"<<std::hex<<sample.result<<" handle=0x"<<sample.handle<<" object=0x"<<sample.resourceObject<<std::dec<<" objectKey="<<sample.objectKey<<" handleReadable="<<sample.handleReadable<<" objectReadable="<<sample.objectReadable<<" modelCode="<<sample.modelCode<<" generation="<<sample.stamp<<"\n";
            file<<"RAW STACK CANDIDATES (not validated frames):\n";for(unsigned j=0;j<sample.candidateCount;++j)file<<"ffximain.dll + 0x"<<std::hex<<(sample.candidates[j]-engineBase)<<std::dec<<"\n";
            file<<"UNWOUND FRAMES:\n";
            for(unsigned j=0;j<sample.count;++j){HMODULE module=nullptr;char name[MAX_PATH]={};auto address=(uintptr_t)sample.frames[j];
                if(GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,(LPCSTR)address,&module)){GetModuleFileNameA(module,name,MAX_PATH);file<<name<<" + 0x"<<std::hex<<(address-(uintptr_t)module)<<std::dec<<"\n";}
                else file<<"unmapped 0x"<<std::hex<<address<<std::dec<<"\n";
            }
        }
        file.flush();return file.good();
    }
static void runCaptureWriter(const std::string& path,std::atomic<bool>& stopped,std::condition_variable& wake,std::mutex& mutex,unsigned windowMs=60000,unsigned intervalMs=1000){
    auto end=std::chrono::steady_clock::now()+std::chrono::milliseconds(windowMs);
    for(;;){std::unique_lock<std::mutex> lock(mutex);wake.wait_for(lock,std::chrono::milliseconds(intervalMs),[&]{return stopped.load();});bool stop=stopped.load();lock.unlock();
        if(stop||std::chrono::steady_clock::now()>=end)captureEnabled=false;
        writeCapture(path);if(stop||!captureEnabled.load())break;
    }
}
// Traced from this client's own actor->vtable[0x298] call sites, including
// 0x8bd75 (tag, target actor, null job). Both implementations return bool and
// pop 12 argument bytes. 0x2a0 queries completion (tag, target), popping 8.
// Use only exact checked implementations; never edit the entity's state table.
struct NativeGestureApi {
 bool equippedLookupFound=false;bool usedEquippedRoute=false;bool upperClipFound=false;
 static bool matches(uintptr_t fn,const unsigned char*expected,size_t size){
  unsigned char actual[16]{};return size==sizeof(actual)&&readPointer(fn,actual,size)&&!memcmp(actual,expected,size);
 }
 static uintptr_t entry(uintptr_t actor,unsigned slot){
  uintptr_t table=0,fn=0;if(!actor||!readPointer(actor,&table,4)||!readPointer(table+slot,&fn,4))return 0;
  const unsigned char playDerived[]={0x51,0x56,0x57,0x8b,0x7c,0x24,0x10,0x6a,0x00,0x6a,0x00,0x6a,0xff,0x57,0x8d,0x44};
  const unsigned char playBase[]={0x83,0xec,0x08,0x56,0x8b,0xf1,0x8d,0x4c,0x24,0x08,0x8b,0x06,0x51,0x8b,0xce,0xff};
  const unsigned char done[]={0x51,0x56,0x8b,0xf1,0x57,0x8b,0x06,0xff,0x90,0xe4,0x01,0x00,0x00,0x84,0xc0,0x75};
  unsigned char stop[]={0x56,0x57,0x68,0xbc,0x0e,0x33,0x10,0x8b,0xf1,0xe8,0x22,0x7c,0xfa,0xff,0x8b,0x7c};
  uint32_t stopString=uint32_t(engineBase+0x330ebc);memcpy(stop+3,&stopString,4);
  const unsigned char*expected=nullptr;
  if(slot==0x298&&fn==engineBase+0xcee40)expected=playDerived;
  if(slot==0x298&&fn==engineBase+0x84c30)expected=playBase;
  if(slot==0x2a0&&fn==engineBase+0x84d40)expected=done;
  if(slot==0x29c&&fn==engineBase+0x84cc0)expected=stop;
  unsigned char actual[16]{};if(!expected||!readPointer(fn,actual,sizeof(actual))||memcmp(actual,expected,sizeof(actual)))return 0;
  return fn;
 }
 static uintptr_t equippedLookup(uintptr_t actor){
  uintptr_t table=0,fn=0;const unsigned char expected[]={0x83,0xec,0x24,0x53,0x55,0x56,0x8b,0xf1,0x57,0x89,0x74,0x24,0x14,0x8d,0x8e,0x74};
  if(!readPointer(actor,&table,4)||!readPointer(table+0x1ec,&fn,4)||fn!=engineBase+0xce9b0||!matches(fn,expected,sizeof(expected)))return 0;
  return fn;
 }
 static uintptr_t schedulerStart(){
  const unsigned char expected[]={0x56,0x57,0x8b,0xf1,0xe8,0x17,0xb2,0x01,0x00,0x8b,0x44,0x24,0x14,0x8b,0x7c,0x24};
  return matches(engineBase+0x56c70,expected,sizeof(expected))?engineBase+0x56c70:0;
 }
 static uintptr_t equippedResource(uintptr_t actor,unsigned type,uint32_t tag){
  auto lookup=equippedLookup(actor);uintptr_t handle=0,resource=0;if(!lookup)return 0;
  reinterpret_cast<void*(__thiscall*)(void*,uintptr_t*,unsigned,uint32_t)>(lookup)((void*)actor,&handle,type,tag);
  return handle&&readPointer(handle,&resource,4)?resource:0;
 }
 bool play(uintptr_t actor,uint32_t tag){
  equippedLookupFound=false;usedEquippedRoute=false;upperClipFound=false;
  auto fn=entry(actor,0x298);if(!fn||!entry(actor,0x2a0)||!entry(actor,0x29c))return false;
  if(tag==hood::gesture::upTag||tag==hood::gesture::downTag){upperClipFound=equippedResource(actor,0x2b,hood::gesture::upperClip(tag))!=0;if(!upperClipFound)return false;}
  if(reinterpret_cast<bool(__thiscall*)(void*,uint32_t,void*,void*)>(fn)((void*)actor,tag,(void*)actor,nullptr))return true;
  // Normal actor motion lookup omits the equipment tree. Its type-aware query
  // (also used by native completion checks) searches the equipped meshes' roots.
  if(tag!=hood::gesture::upTag&&tag!=hood::gesture::downTag)return false;
  auto start=schedulerStart();if(!start)return false;
  auto resource=equippedResource(actor,7,tag);if(!resource)return false;
  equippedLookupFound=true;
  auto result=reinterpret_cast<uintptr_t(__thiscall*)(void*,void*,void*,unsigned,void*)>(start)((void*)resource,(void*)actor,(void*)actor,0,nullptr);
  usedEquippedRoute=result!=0;return usedEquippedRoute;
 }
 bool finished(uintptr_t actor,uint32_t tag){
  auto fn=entry(actor,0x2a0);return !fn||reinterpret_cast<bool(__thiscall*)(void*,uint32_t,void*)>(fn)((void*)actor,tag,(void*)actor);
 }
 void stop(uintptr_t actor,uint32_t tag){
  auto fn=entry(actor,0x29c);if(fn)reinterpret_cast<void(__thiscall*)(void*,uint32_t,void*)>(fn)((void*)actor,tag,(void*)actor);
 }
};
class Hoodie final:public IPlugin {
#ifdef HOODIE_TESTING
    friend struct HoodieFixtureAccess;
    unsigned gestureActorQueries=0;
#endif
    IAshitaCore* core=nullptr;bool hooked=false,showSettings=false;std::string logPath;std::thread writer;std::atomic<bool> stopWriter{false};std::mutex writerMutex;std::condition_variable writerWake;
    hood::LoginGate loginGate;bool startupAttempted=false,startupFailed=false,requestedEnabled=true,requestedOthers=true;std::atomic<bool> resetLogin{false};
    const char* waitingReason="Waiting for login";
    unsigned lastLogin=0,lastZoning=0,lastIndex=0,lastZone=0;uintptr_t lastActor=0;unsigned lastServer=0;

    hood::gesture::Sequence gestureSequence;
    NativeGestureApi gestureApi;
    hood::gesture::Playback<NativeGestureApi> gesturePlayback;
    std::string gestureMessage="Hood animation ready.",gestureCommand,gesturePass;
    uint64_t gesturePassUntil=0;
    int gestureCommandMode=0;
    bool gestureNativeRequest=false;
    hood::Preferences preferences;
    bool requestedAnimateChange=false;
    bool gestureRequestReleased=false,gestureRequestHidden=false;
    std::deque<std::string> gestureHistory;
    hood::gesture::MotionWatch gestureMotion;
    hood::gesture::CastActivity gestureCasting;
    hood::gesture::Preparation gesturePreparation;
    const char* gestureIdleReason="character is unavailable";
    std::string gestureSlotReport;
    void logGestureOutcome(){
#ifndef HOODIE_TESTING
        return;
#endif
        if(!core)return;auto actor=gestureActor(false);
        std::ostringstream recent;recent<<GetTickCount64()<<" "<<gestureMessage<<" race="<<actor.race<<" baseIdle="<<actor.idle;
        if(gestureHistory.size()==16)gestureHistory.pop_front();gestureHistory.push_back(recent.str());
        std::ofstream out(std::filesystem::path(core->GetInstallPath())/"logs"/"hoodie-gesture-state.txt");
        out<<"Hoodie 0.49: "<<gestureMessage<<"\nAnimate="<<gestureEnabled.load()<<" native requests="<<nativeHeadRequests.load()<<" actor="<<std::hex<<actor.pointer<<std::dec<<" race="<<actor.race<<" body="<<actor.body<<" idle="<<actor.idle<<" preparedActor="<<std::hex<<gestureLoadedActor.load()<<std::dec<<" preparedRace="<<gestureLoadedRace.load()<<" preparedBody="<<gestureLoadedBody.load()<<"\n";
        if(actor.pointer){auto m=core->GetMemoryManager();auto e=m->GetEntity();auto cast=m->GetCastBar();out<<"Idle rejection: "<<gestureIdleReason<<"\nstatus="<<e->GetStatus(actor.index)<<" server="<<e->GetStatusServer(actor.index)<<" event="<<e->GetStatusEvent(actor.index)<<" rawDelta="<<e->GetMoveDeltaX(actor.index)<<","<<e->GetMoveDeltaY(actor.index)<<","<<e->GetMoveDeltaZ(actor.index)<<" observedStationary="<<gestureMotion.stationary(actor,GetTickCount64())<<" castCount="<<(cast?cast->GetCount():0)<<" castMax="<<(cast?cast->GetMax():0)<<"\n";}
        out<<"Recent outcomes (oldest first):\n";for(const auto& line:gestureHistory)out<<line<<"\n";
    }
    void inspectGestureSlots(){
#ifndef HOODIE_TESTING
        return;
#endif
        auto actor=gestureActor();
        if(!actor.pointer){gestureSlotReport="No logged-in local actor.";return;}
        gestureSlotReport=hood::gesture::describeAnimation(core->GetMemoryManager()->GetEntity(),actor.index);
        uintptr_t table=0,play=0,done=0;
        if(readPointer(actor.pointer,&table,4)){readPointer(table+0x298,&play,4);readPointer(table+0x2a0,&done,4);}
        std::ostringstream route;route<<"\nNative motion RVA: play=0x"<<std::hex<<(play>=engineBase?play-engineBase:0)<<" done=0x"<<(done>=engineBase?done-engineBase:0)<<std::dec;
        route<<" verified="<<(NativeGestureApi::entry(actor.pointer,0x298)&&NativeGestureApi::entry(actor.pointer,0x2a0));gestureSlotReport+=route.str();
        std::ostringstream equipment;equipment<<"\nEquipment lookup verified="<<bool(NativeGestureApi::equippedLookup(actor.pointer))<<" scheduler start verified="<<bool(NativeGestureApi::schedulerStart())<<" found="<<gestureApi.equippedLookupFound<<" started="<<gestureApi.usedEquippedRoute;
        equipment<<"\nExact upper-body clip found="<<gestureApi.upperClipFound;
        equipment<<"\nPrivate body parsed="<<(gestureLoadedActor.load()==actor.pointer&&gestureLoadedBody.load()==actor.body&&gestureLoadedRace.load()==actor.race);gestureSlotReport+=equipment.str();
        auto path=std::filesystem::path(core->GetInstallPath())/"logs"/"hoodie-gesture-slots.txt";
        std::ofstream report(path);report<<"Hoodie 0.49 gesture playback inspection\nLocal actor race="<<actor.race<<" body="<<actor.body<<" idle="<<actor.idle<<"\n"<<gestureSlotReport<<"\n";report.flush();
    }
    hood::gesture::Actor gestureActor(bool checkMotion=true){
#ifdef HOODIE_TESTING
        ++gestureActorQueries;
#endif
        hood::gesture::Actor a;gestureIdleReason="character is unavailable";if(!core||!worldReady.load())return a;
        auto m=core->GetMemoryManager();auto e=m->GetEntity();a.index=m->GetParty()->GetMemberTargetIndex(0);
        if(!a.index||a.index>=0x900||!e->GetRawEntity(a.index))return a;
        a.pointer=e->GetActorPointer(a.index);a.server=e->GetServerId(a.index);a.race=e->GetRace(a.index);if(a.race==6)a.race=5;a.body=e->GetLookBody(a.index);a.zone=m->GetParty()->GetMemberZone(0);
        a.x=e->GetLocalPositionX(a.index);a.y=e->GetLocalPositionY(a.index);a.z=e->GetLocalPositionZ(a.index);
        auto cast=m->GetCastBar();
        // Closed/interrupted cast bars retain Count and Max. Observe actual
        // progress during this request, rather than treating residue as busy.
        bool casting=gestureCasting.observe(cast?cast->GetCount():0,cast?cast->GetMax():0,GetTickCount64());
        gestureIdleReason=hood::gesture::idleBlock(e->GetStatus(a.index),e->GetStatusServer(a.index),e->GetStatusEvent(a.index),casting,!checkMotion||gestureMotion.stationary(a,GetTickCount64()));
        a.idle=a.pointer&&!*gestureIdleReason;
        return a;
    }
    void passGestureCommand(){
        if(gestureCommand.empty()||!core)return;
        if(gestureNativeRequest){
            auto hidden=displayHeadCommand(gestureCommand)==0;gestureCommand.clear();gestureNativeRequest=false;
            bool accepted=displayRequest&&displayRequest(10,hidden?1u:0u);
            gestureRequestReleased=accepted;
            gestureMessage=accepted?"Delay reached; native displayhead request submitted. Waiting for client acceptance.":"Delay reached; native client could not submit the displayhead change.";
            return;
        }
        gesturePass=gestureCommand;gesturePassUntil=GetTickCount64()+2000;gestureCommand.clear();core->GetChatManager()->QueueCommand(gestureCommandMode,gesturePass.c_str());
    }
    void finishGesture(bool allowCommand){
        if(!gesturePreparation.active()&&!gestureSequence.active()&&!gesturePlayback.active){gestureCommand.clear();gestureNativeRequest=false;return;}
        auto current=gestureActor();bool same=hood::gesture::same(current,gesturePreparation.active()?gesturePreparation.identity():gestureSequence.identity());
        if(core)gesturePlayback.release(gestureApi,current.pointer,same);
        gesturePreparation.cancel();gestureSequence.cancel();if(allowCommand&&same)passGestureCommand();else{gestureCommand.clear();gestureNativeRequest=false;}
        gestureRequestReleased=false;
    }
    bool deferNativeDisplay(bool hidden){
        if(!gestureEnabled.load()||!variantsEnabled.load()||!worldReady.load()||!localSelectionKnown.load())return false;
        if((gesturePreparation.active()||gestureSequence.active())&&(gestureNativeRequest||gestureRequestReleased)){
            if(gestureRequestHidden==hidden)return true;
            finishGesture(false); // Latest opposite request cancels the pending one.
        }
        bool current=false;
        if(!displaySettingVerified||!hood::readHeadHidden(engineBase+0x4dfd98,current)||hidden==current)return false;
        return prepareGesture(hidden,hidden?"/displayhead off":"/displayhead on",true);
    }
    bool prepareGesture(bool hidden,const std::string& command={},bool native=false,int mode=0){
        if(!gestureEnabled.load()||!variantsEnabled.load())return false;
        gestureCasting={}; // First sample starts a new on-demand cast probe.
        auto actor=gestureActor(false);
        if(!actor.idle){gestureMessage=std::string("Last change skipped animation: ")+gestureIdleReason+".";logGestureOutcome();return false;}
        if(gesturePreparation.active()||gestureSequence.active())finishGesture(false);
        gestureMotion={};gestureMotion.observe(actor,GetTickCount64());
        gesturePreparation.begin(actor,GetTickCount64());
        gestureCommand=command;gestureCommandMode=mode;gestureRequestHidden=hidden;gestureRequestReleased=false;
        gestureNativeRequest=native;
        gestureMessage="Checking stationary position for this hood change.";
        logGestureOutcome();
        return true;
    }
    bool startGesture(const std::string& command={},int mode=0){
        auto a=gestureActor();
        if(!gestureEnabled.load()||!variantsEnabled.load()){gestureMessage="Gesture skipped: enable Hoodie and Animate change first.";return false;}
        if(!a.idle){gestureMessage=std::string("Last change skipped animation: ")+gestureIdleReason+".";return false;}
        std::shared_ptr<const hood::gesture::Package> p;{std::lock_guard<std::mutex> lock(gesturePackageMutex);p=gesturePackages.forRace(a.race);}
        if(!p||p->race!=a.race){gestureMessage="Animation unavailable for this race.";return false;}
        // Playback verifies the exact private clip and scheduler in this actor's
        // equipped resources. A stale preparation marker is not that proof.
        if(gestureSequence.active())finishGesture(false);
        inspectGestureSlots();
        if(!NativeGestureApi::entry(a.pointer,0x298)||!NativeGestureApi::entry(a.pointer,0x2a0)){gestureMessage="Native motion call signature differs; playback skipped. State table unchanged.";return false;}
        auto idle=core->GetMemoryManager()->GetEntity()->GetAnimation(a.index,0);
        if(!gesturePlayback.beginChange(gestureApi,a.pointer,idle)){gestureMessage="Native actor and equipment motion lookup did not accept the gesture. See diagnostics; state table unchanged.";inspectGestureSlots();return false;}
        if(!gestureSequence.begin(a,GetTickCount64(),*p,false,hood::gesture::switchDelayMs)){gesturePlayback.release(gestureApi,a.pointer,true);return false;}
        gestureCommand=command;gestureCommandMode=mode;gestureMessage=gestureApi.usedEquippedRoute?"Client accepted the private equipment scheduler. Check arms/hands visually; state table unchanged.":"Client accepted the custom motion. Check arms/hands visually; native state table unchanged.";inspectGestureSlots();return true;
    }
    void tickGesture(){
        if(!gesturePreparation.active()&&!gestureSequence.active())return;
        auto now=GetTickCount64();auto observed=gestureActor(false);gestureMotion.observe(observed,now);
        if(gesturePreparation.active()){
            auto prepared=gesturePreparation.tick(observed,now);
            if(prepared==hood::gesture::PrepareEvent::Waiting)return;
            if(prepared==hood::gesture::PrepareEvent::Changed){gestureCommand.clear();gestureNativeRequest=false;gestureMessage="Hood change cancelled: character changed during preparation.";logGestureOutcome();return;}
            if(prepared==hood::gesture::PrepareEvent::Interrupted){passGestureCommand();gestureMessage="Last change skipped animation: movement, casting, event or delayed frame during preparation.";logGestureOutcome();return;}
            auto command=gestureCommand;bool native=gestureNativeRequest;auto mode=gestureCommandMode;
            if(!startGesture(command,mode)){auto reason=gestureMessage;passGestureCommand();gestureMessage=reason;logGestureOutcome();return;}
            gestureNativeRequest=native;gestureMessage=native?"Gesture started; native hood request held for 230 ms.":"Gesture test started.";logGestureOutcome();
        }
        // An active gesture needs current eligibility and displacement, not a
        // fresh 60ms settling window. A slow render frame is not movement.
        auto a=gestureActor(false);auto start=gestureSequence.identity();
        if(hood::gesture::positionMoved(start,a))a.idle=false;
        auto event=gestureSequence.tick(a,GetTickCount64());
        if(event==hood::gesture::Event::Contact&&!gestureCommand.empty()){bool native=gestureNativeRequest;passGestureCommand();if(!native)gestureMessage="Contact reached; native /displayhead command queued. Client restrictions still apply.";}
        else if(event==hood::gesture::Event::Finished){finishGesture(false);gestureMessage="Gesture completed without restarting native idle.";}
        else if(event==hood::gesture::Event::Cancelled){bool released=gestureRequestReleased;finishGesture(true);gestureMessage=released?"Model/state changed after hood request submission; gesture released.":"Gesture interrupted before switch; native command kept only for the same character.";}
        if(event!=hood::gesture::Event::None)logGestureOutcome();
    }
    void loadGesture(bool enable=true,bool refresh=true){
        if(gesturePreparation.active()||gestureSequence.active())finishGesture(true);auto actor=refresh?gestureActor():hood::gesture::Actor{};gestureLoadedActor=0;
        try{
            if(refresh&&!actor.pointer)throw std::runtime_error("Log in before enabling Animate change.");
            auto root=std::filesystem::path(core->GetInstallPath())/"resources"/"hoodie-gesture";
            std::ifstream config(root/"editor-exports-path.txt");std::string path;std::getline(config,path);
            while(!path.empty()&&(path.back()=='\r'||path.back()=='\n'))path.pop_back();
            std::filesystem::path exports(path);hood::gesture::Library loaded;
            for(unsigned race:{1u,2u,3u,4u,5u,7u,8u}){
                auto selected=root/hood::normal::families[race-1]/"gesture.hga";std::error_code ec;
                auto latest=std::filesystem::last_write_time(selected,ec);if(ec)latest=std::filesystem::file_time_type::min();
                if(!path.empty()&&std::filesystem::is_directory(exports,ec))for(const auto& item:std::filesystem::directory_iterator(exports)){
                    auto candidate=item.path()/"gesture.hga";
                    if(!item.is_directory()||item.path().filename().string().rfind(std::string("gesture-")+hood::normal::families[race-1]+"-",0)!=0||!std::filesystem::is_regular_file(candidate,ec))continue;
                    auto time=std::filesystem::last_write_time(candidate,ec);if(!ec&&time>latest){selected=candidate;latest=time;}
                }
                auto package=std::make_shared<const hood::gesture::Package>(hood::gesture::Package::read(selected));
                if(package->race!=race)throw std::runtime_error("Gesture export belongs to another race.");
                loaded.races[race-1]=std::move(package);
            }
            {std::lock_guard<std::mutex> lock(gesturePackageMutex);gesturePackages=std::move(loaded);}
            gestureEnabled=enable;
            if(refresh&&actor.pointer)core->GetMemoryManager()->GetEntity()->SetModelUpdateFlags(actor.index,hood::refreshBody(core->GetMemoryManager()->GetEntity()->GetModelUpdateFlags(actor.index)));
            gestureMessage="Animate change ready for all prepared races and armors.";
        }catch(const std::exception&e){gestureEnabled=false;gestureMessage=e.what();say(e.what());}
    }
    void setAnimateChange(bool enabled){
        requestedAnimateChange=enabled;
        if(!preferences.saveAnimateChange(enabled)&&core)say("Could not save Animate change; the selection applies for this session only.");
        if(!enabled){finishGesture(true);gestureEnabled=false;return;}
        bool available=false;{std::lock_guard<std::mutex> lock(gesturePackageMutex);available=bool(gesturePackages.forRace(1));}
        if(available)gestureEnabled=true;else loadGesture();
    }
    void configurePreferences(const std::filesystem::path& file){preferences.file=file;requestedAnimateChange=preferences.animateChange();}



    void say(const char* text){core->GetChatManager()->Writef(207,false,"[hoodie] %s",text);}
    void report(){say(writeCapture(logPath)?"Saved logs/hoodie-observe.txt":"Could not save diagnostic capture.");}

    unsigned refreshMatching(bool locals,bool peers){
        if(!worldReady.load())return 0;
        auto e=core->GetMemoryManager()->GetEntity();auto local=core->GetMemoryManager()->GetParty()->GetMemberTargetIndex(0);unsigned count=0;
        for(unsigned i=1;i<0x900;++i){
            if((i==local?!locals:!peers)||!e->GetRawEntity(i)||!e->GetServerId(i)||!e->GetActorPointer(i)||e->GetType(i)!=0||!supportedRace(e->GetRace(i))||!hood::normal::supportedBody(e->GetLookBody(i)))continue;
            e->SetModelUpdateFlags(i,hood::refreshBody(e->GetModelUpdateFlags(i)));++count;
        }return count;
    }
public:
    bool Direct3DInitialize(IDirect3DDevice8*) override{return true;}
    void Direct3DPresent(const RECT*,const RECT*,HWND,const RGNDATA*)override{
        if(shuttingDown.load())return;
        pollLogin();

        tickGesture();syncLocalSelection();drainPeerRefreshes();
        if(!showSettings||!core)return;auto gui=core->GetGuiManager();if(!gui||!gui->GetCurrentContext())return;
        gui->SetNextWindowSizeConstraints(ImVec2(340,0),ImVec2(600,1000));
        gui->SetNextWindowSize(ImVec2(360,0),ImGuiCond_FirstUseEver);
        if(gui->Begin("Hoodie",&showSettings,ImGuiWindowFlags_AlwaysAutoResize)){
            bool enabled=requestedEnabled,others=requestedOthers;
            if(gui->Checkbox("Enable Hoodie",&enabled))HandleCommand(0,enabled?"/hoodie enable":"/hoodie disable",false);
            if(gui->Checkbox("Other players",&others))HandleCommand(0,others?"/hoodie others on":"/hoodie others off",false);
            if(!worldReady.load())gui->TextWrapped("%s",startupFailed?"Hoodie could not start. Reload the plugin to retry.":"Waiting for your character to finish loading.");
            if(displaySettingVerified&&localSelectionKnown.load())gui->TextWrapped("Hood: %s. Follows /displayhead on or off.",localSelection.load()==hood::Variant::Down?"down":"up");
            else gui->TextWrapped("Hood: waiting for the client's display setting.");
            gui->TextWrapped("Supports all races: Royal Footman Tunic, Vermillion Cloak, Cloak, Silk Cloak and Black Cloak.");
            gui->Separator();
            bool gesture=requestedAnimateChange;if(gui->Checkbox("Animate change",&gesture))setAnimateChange(gesture);
            gui->TextWrapped("Play the hood-change animation while standing idle. When off, the hood changes immediately.");


        }gui->End();
    }
    const char* GetName()const override{return "Hoodie";}
    const char* GetAuthor()const override{return "Zarianna";}
    const char* GetDescription()const override{return "Hood up/down for supported cloaks and tunics";}
    double GetVersion()const override{return 1.0;}
    uint32_t GetFlags()const override{return (uint32_t)Ashita::PluginFlags::UseCommands|(uint32_t)Ashita::PluginFlags::UsePackets|(uint32_t)Ashita::PluginFlags::UseDirect3D;}
    bool Initialize(IAshitaCore* c,ILogManager*,uint32_t)override{
        if(reloadRequiresRestart.load())return false;
        core=c;liveCore=c;shuttingDown=false;worldReady=false;localSelectionKnown=false;pendingLooks.clear();for(auto& v:receivedLooks)v=0;othersEnabled=false;variantsEnabled=false;reads=0;captureEnabled=false;coreThread=GetCurrentThreadId();
        parserVerified=false;displaySettingVerified=false;loginGate.reset();resetLogin=false;startupAttempted=false;startupFailed=false;requestedEnabled=requestedOthers=true;
        gestureEnabled=false;
gestureLoadedActor=0;gestureSequence.cancel();gestureCommand.clear();gesturePass.clear();
        gestureNativeRequest=false;displayRequestHooked=false;nativeHeadRequests=0;
        gesturePreparation.cancel();gestureMotion={};gestureCasting={};
        configurePreferences(std::filesystem::path(c->GetInstallPath())/"config"/"hoodie"/"settings.ini");
        nativeGestureOwner=this;nativeGestureDefer=[](void* owner,bool hidden){return static_cast<Hoodie*>(owner)->deferNativeDisplay(hidden);};
        logPath=(std::filesystem::path(c->GetInstallPath())/"logs"/"hoodie-observe.txt").string();
        return true; // Standby must not touch native chat/model paths before login.
    }
    void pollLogin(){
        if(!core)return;
        if(resetLogin.exchange(false))loginGate.reset();
        auto m=core->GetMemoryManager();auto p=m->GetPlayer();auto e=m->GetEntity();auto party=m->GetParty();auto index=party->GetMemberTargetIndex(0);
        lastLogin=p->GetLoginStatus();lastZoning=p->GetIsZoning();lastIndex=index;lastZone=party->GetMemberZone(0);lastServer=0;lastActor=0;
        

        bool loaded=lastLogin==2&&!lastZoning&&index>0&&index<0x900;
        waitingReason=lastLogin!=2?"Waiting for login":lastZoning?"Waiting for zoning to finish":!(index>0&&index<0x900)?"Waiting for the local character index":"Waiting for character data";
        hood::WorldIdentity identity;
        if(loaded&&e->GetRawEntity(index)&&(startupAttempted||e->GetStatus(index)!=4)&&supportedRace(e->GetRace(index))){
            lastServer=e->GetServerId(index);lastActor=e->GetActorPointer(index);identity={index,lastServer,lastZone,lastActor};
            waitingReason=!lastServer?"Waiting for character identity":!lastActor?"Waiting for the character model":"Waiting for the character to settle (1.5 seconds)";
        }
        bool ready=loginGate.update(loaded,identity,GetTickCount64(),startupAttempted&&!startupFailed);
        if(!ready){if(worldReady.exchange(false))localSelectionKnown=false;return;}
        if(!startupAttempted){
            if(!GetModuleHandleA("FFXiMain.dll")){waitingReason="Waiting for FFXiMain.dll";return;}
            startupAttempted=true;worldReady=true;
            try{startupFailed=!activateNative();}catch(const std::exception&){startupFailed=true;}
            if(startupFailed){worldReady=false;variantsEnabled=false;say("Native startup failed; Hoodie remains inactive. Reload to retry.");return;}

        }
        if(!startupFailed&&!resetLogin.load())worldReady=true;
    }
    bool activateNative(){
        // Ashita's boot loader can initialize a plugin on a different thread
        // from in-game commands. Bind native model work to the present thread,
        // after the first character is ready, rather than that loader thread.
        coreThread=GetCurrentThreadId();
        auto c=core;captureEnabled=true;auto localIndex=core->GetMemoryManager()->GetParty()->GetMemberTargetIndex(0);
        actorBaseline=localIndex>0&&localIndex<0x900?core->GetMemoryManager()->GetEntity()->GetActorPointer(localIndex):0;
        auto engine=GetModuleHandleA("FFXiMain.dll");if(!engine){say("The game client is not ready yet.");return false;}
        engineBase=reinterpret_cast<uintptr_t>(engine);
        auto dos=reinterpret_cast<IMAGE_DOS_HEADER*>(engineBase);
        auto nt=reinterpret_cast<IMAGE_NT_HEADERS*>(engineBase+dos->e_lfanew);
        if(dos->e_magic!=IMAGE_DOS_SIGNATURE||nt->Signature!=IMAGE_NT_SIGNATURE)return false;
        engineEnd=engineBase+nt->OptionalHeader.SizeOfImage;
        logPath=(std::filesystem::path(c->GetInstallPath())/"logs"/"hoodie-observe.txt").string();
        const unsigned char asyncPrefix[]={0x8b,0x44,0x24,0x08,0x56,0x8b,0xf1,0x33,0xc9,0x51,0x88,0x4e,0x3c,0x89,0x4e,0x41};
        const unsigned char syncPrefix[]={0x83,0xec,0x08,0x53,0x8b,0x5c,0x24,0x14,0x55,0x56,0x57,0x53,0x8b,0xe9};
        unsigned char cachePrefix[]={0xa1,0,0,0,0,0x85,0xc0,0x74,0x04,0x8b,0x10,0xeb,0x02,0x33,0xd2,0x57,0x8b,0x7c,0x24,0x0c,0x85,0xd2,0x74,0x21};
        uint32_t global=(uint32_t)(engineBase+0x47cf84);memcpy(cachePrefix+1,&global,4);
        auto match=[&](size_t rva,const void* bytes,size_t length){return hood::matchesCode(engineBase,engineEnd-engineBase,rva,bytes,length);};
        unsigned char displayPrefix[]={0xa1,0,0,0,0,0x8a,0x88,0xdb,0xcf,0x02,0x00,0xf6,0xd1,0xc0,0xe9,0x03,0x80,0xe1,0x01};
        uint32_t settingsSlot=(uint32_t)(engineBase+0x4dfd98);memcpy(displayPrefix+1,&settingsSlot,4);
        const unsigned char displayGetter[]={0x8a,0x80,0xdb,0xcf,0x02,0x00,0xc0,0xe8,0x03,0x24,0x01,0xc3};
        const unsigned char raceUpdate[]={0x88,0x88,0xef,0x00,0x00,0x00,0x8b,0x88,0x20,0x01,0x00,0x00,0x83,0xc9,0x01,0x89,0x88,0x20,0x01,0x00,0x00};


        displaySettingVerified=match(0x7ddcc,displayPrefix,sizeof(displayPrefix))&&match(0xe4843,displayGetter,sizeof(displayGetter))&&engineEnd-engineBase>=0x4dfd98+sizeof(uintptr_t);
        unsigned char requestPrefix[]={0xa1,0,0,0,0,0x85,0xc0,0x0f,0x84,0xb2,0x00,0x00,0x00,0x6a,0x01,0x6a,0x01,0x68,0xdc,0x00,0x00,0x00};
        memcpy(requestPrefix+1,&settingsSlot,4);
        displayRequestHooked=displaySettingVerified&&match(0xe46a0,requestPrefix,sizeof(requestPrefix));
        if(displayRequestHooked)displayRequest=(DisplayRequestFn)(engineBase+0xe46a0);
        if(!displaySettingVerified)say("Client hidden-head setting signature unavailable; automatic local hood sync is inactive.");
        requestHooks=match(0x730e0,cachePrefix,sizeof(cachePrefix))&&match(0x72c30,asyncPrefix,sizeof(asyncPrefix))&&match(0x72fd0,syncPrefix,sizeof(syncPrefix));
        const unsigned char attachPrefix[]={0x8a,0x44,0x24,0x10,0x53,0x8b,0x5c,0x24,0x0c,0x55,0xc1,0xfb,0x0c,0x83,0xe3,0x0f,0x56,0x84};
        attachmentHooked=match(0xd0b50,attachPrefix,sizeof(attachPrefix));
        if(attachmentHooked)attachResource=(AttachFn)(engineBase+0xd0b50);
        if(requestHooks){cacheRequest=(RequestFn)(engineBase+0x730e0);asyncRequest=(RequestFn)(engineBase+0x72c30);syncRequest=(RequestFn)(engineBase+0x72fd0);}
        const unsigned char releasePrefix[]={0x56,0x8b,0x74,0x24,0x08,0x8b,0xce,0xe8};
        const unsigned char slotPrefix[]={0x8b,0x44,0x24,0x04,0x85,0xc0,0x74,0x04,0x8b,0x10,0xeb,0x02,0x33,0xd2,0x56};
        releaseHooks=match(0x73b80,releasePrefix,sizeof(releasePrefix))&&match(0x74180,slotPrefix,sizeof(slotPrefix));
        if(releaseHooks){releaseResource=(ReleaseFn)(engineBase+0x73b80);releaseSlot=(ReleaseFn)(engineBase+0x74180);}
        const unsigned char parserPrefix[]={0x81,0xec,0x18,0x04,0x00,0x00,0x53,0x55,0x56,0x57,0x8b,0xbc,0x24,0x30,0x04,0x00};
        parserVerified=requestHooks&&attachmentHooked&&releaseHooks&&match(0x73430,parserPrefix,sizeof(parserPrefix));
        if(parserVerified)parseModel=(ParseFn)(engineBase+0x73430);
        try{auto root=std::filesystem::path(c->GetInstallPath())/"resources"/"hoodie"/"hf-footman";
            variantAssets=std::make_unique<hood::Assets>((root/"up.dat").string().c_str(),(root/"down.dat").string().c_str(),1);
        }catch(const std::exception&){variantAssets.reset();say("Variant assets unavailable or invalid; Reinstall the resource files.");}
        try{auto root=std::filesystem::path(c->GetInstallPath())/"resources"/"hoodie"/"taru-footman";
            taruAssets=std::make_unique<hood::Assets>((root/"up.dat").string().c_str(),(root/"down.dat").string().c_str(),1,20075);
        }catch(const std::exception&){taruAssets.reset();say("Tarutaru assets unavailable; Hume female support remains available.");}
        try{auto root=std::filesystem::path(c->GetInstallPath())/"resources"/"hoodie"/"mithra-footman";
            mithraAssets=std::make_unique<hood::Assets>((root/"up.dat").string().c_str(),(root/"down.dat").string().c_str(),1,23475);
        }catch(const std::exception&){mithraAssets.reset();say("Mithra assets unavailable; other races remain available.");}
        try{auto root=std::filesystem::path(c->GetInstallPath())/"resources"/"hoodie"/"humemale-footman";
            humeMaleAssets=std::make_unique<hood::Assets>((root/"up.dat").string().c_str(),(root/"down.dat").string().c_str(),1,7371);
        }catch(const std::exception&){humeMaleAssets.reset();say("Hume male assets unavailable; other races remain available.");}
        try{auto root=std::filesystem::path(c->GetInstallPath())/"resources"/"hoodie"/"elvaanmale-footman";
            elvaanMaleAssets=std::make_unique<hood::Assets>((root/"up.dat").string().c_str(),(root/"down.dat").string().c_str(),1,13723);
        }catch(const std::exception&){elvaanMaleAssets.reset();say("Elvaan male assets unavailable; other races remain available.");}
        try{auto root=std::filesystem::path(c->GetInstallPath())/"resources"/"hoodie"/"elvaanfemale-footman";
            elvaanFemaleAssets=std::make_unique<hood::Assets>((root/"up.dat").string().c_str(),(root/"down.dat").string().c_str(),1,16899);
        }catch(const std::exception&){elvaanFemaleAssets.reset();say("Elvaan female assets unavailable; other races remain available.");}
        try{auto root=std::filesystem::path(c->GetInstallPath())/"resources"/"hoodie"/"galka-footman";
            galkaAssets=std::make_unique<hood::Assets>((root/"up.dat").string().c_str(),(root/"down.dat").string().c_str(),1,26651);
        }catch(const std::exception&){galkaAssets.reset();say("Galka assets unavailable; other races remain available.");}
        for(unsigned race=1;race<=8;++race){
            if(race==6)continue; // Native-compatible Tarutaru male/female body data is shared.
            auto index=hood::normal::raceIndex(race);
            try{auto root=std::filesystem::path(c->GetInstallPath())/"resources"/"hoodie"/hood::normal::vermillionFolders[race-1];
                vermillionAssets[index]=std::make_unique<hood::Assets>((root/"up.dat").string().c_str(),(root/"down.dat").string().c_str(),1,hood::normal::resource(race,hood::normal::vermillion));
            }catch(const std::exception&){vermillionAssets[index].reset();core->GetChatManager()->Writef(207,false,"[hoodie] Vermillion assets unavailable for race %u; other armor remains available.",race);}
        }
        for(unsigned armor=2;armor<5;++armor)for(unsigned race:{1u,2u,3u,4u,5u,7u,8u}){
            auto index=hood::normal::raceIndex(race);
            try{auto folder=std::string(hood::normal::prefixes[armor])+"-"+hood::normal::families[race-1];
                auto root=std::filesystem::path(c->GetInstallPath())/"resources"/"hoodie"/folder;
                cloakAssets[armor-2][index]=std::make_unique<hood::Assets>((root/"up.dat").string().c_str(),(root/"down.dat").string().c_str(),1,hood::normal::resource(race,hood::normal::models[armor]));
            }catch(const std::exception&){cloakAssets[armor-2][index].reset();say("An armor model is missing or invalid. Reinstall Hoodie resources to restore it.");}
        }
        loadGesture(requestedAnimateChange,false); // Restore preference and prime every race before private body builds.
        if(DetourTransactionBegin()!=NO_ERROR)return false;
        hood::HookThreads threads;
        LONG status=threads.enroll()?NO_ERROR:ERROR_ACCESS_DENIED;
        if(status==NO_ERROR)status=DetourAttach(&(PVOID&)realOpen,openHook);
        if(status==NO_ERROR)status=DetourAttach(&(PVOID&)realFind,findHook);
        if(status==NO_ERROR&&displayRequestHooked)status=DetourAttach(&(PVOID&)displayRequest,displayRequestHook);
        if(status==NO_ERROR&&requestHooks)status=DetourAttach(&(PVOID&)cacheRequest,cacheHook);
        if(status==NO_ERROR&&requestHooks)status=DetourAttach(&(PVOID&)asyncRequest,asyncHook);
        if(status==NO_ERROR&&requestHooks)status=DetourAttach(&(PVOID&)syncRequest,syncHook);
        if(status==NO_ERROR&&attachmentHooked)status=DetourAttach(&(PVOID&)attachResource,attachHook);
        if(status==NO_ERROR&&releaseHooks)status=DetourAttach(&(PVOID&)releaseResource,resourceReleaseHook);
        if(status==NO_ERROR&&releaseHooks)status=DetourAttach(&(PVOID&)releaseSlot,slotReleaseHook);
        if(status!=NO_ERROR){DetourTransactionAbort();return false;}
        hooked=DetourTransactionCommit()==NO_ERROR;
        if(hooked){
            // Enable only after all native parser signatures and the base assets are verified.
            variantsEnabled=requestedEnabled&&parserVerified&&anyAssets();
            othersEnabled=requestedOthers;
            if(variantsEnabled.load()){
                localSelectionKnown=false;syncLocalSelection();
                say("Ready. /hoodie opens settings.");
            }

#ifdef HOODIE_TESTING
            writeCapture(logPath);stopWriter=false;
#endif


#ifdef HOODIE_TESTING
            writer=std::thread([this]{runCaptureWriter(logPath,stopWriter,writerWake,writerMutex);});
#endif


        }

        if(hooked&&!parserVerified)say("Hoodie could not initialize for this client. Check the plugin version and resource installation.");return hooked;
    }
    void Release()override{
        if(gesturePreparation.active()||gestureSequence.active()||gesturePlayback.active)finishGesture(false);gestureEnabled=false;gestureLoadedActor=0;

#ifdef HOODIE_TESTING
        
#endif

        shuttingDown=true;worldReady=false;variantsEnabled=false;othersEnabled=false;captureEnabled=false;
        stopWriter=true;writerWake.notify_all();if(writer.joinable())writer.join();
#ifdef HOODIE_TESTING
if(!logPath.empty())writeCapture(logPath);
#endif

        if(hooked){
            HMODULE hold=nullptr;
            bool held=GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,reinterpret_cast<LPCSTR>(&cacheHook),&hold)!=FALSE;
            uintptr_t base=(uintptr_t)hold;size_t bytes=0;
            if(held){auto dos=(IMAGE_DOS_HEADER*)base;auto nt=(IMAGE_NT_HEADERS*)(base+dos->e_lfanew);bytes=nt->OptionalHeader.SizeOfImage;}
            for(unsigned wait=0;hood::activeHooks.load()&&wait<1000;++wait)Sleep(1);
            LONG status=DetourTransactionBegin();bool inFlight=true;
            if(status==NO_ERROR){
                hood::HookThreads threads;
                status=threads.enroll(base,bytes)?NO_ERROR:ERROR_ACCESS_DENIED;
                inFlight=threads.insideModule||hood::activeHooks.load()!=0;
                auto detach=[&](PVOID* pointer,PVOID hook){if(status==NO_ERROR)status=detachHookApi(pointer,hook);};
                detach(&(PVOID&)realOpen,(PVOID)openHook);detach(&(PVOID&)realFind,(PVOID)findHook);
                if(displayRequestHooked)detach(&(PVOID&)displayRequest,(PVOID)displayRequestHook);
                if(requestHooks){detach(&(PVOID&)cacheRequest,(PVOID)cacheHook);detach(&(PVOID&)asyncRequest,(PVOID)asyncHook);detach(&(PVOID&)syncRequest,(PVOID)syncHook);}
                if(attachmentHooked)detach(&(PVOID&)attachResource,(PVOID)attachHook);
                if(releaseHooks){detach(&(PVOID&)releaseResource,(PVOID)resourceReleaseHook);detach(&(PVOID&)releaseSlot,(PVOID)slotReleaseHook);}
                if(status==NO_ERROR)status=DetourTransactionCommit();else DetourTransactionAbort();
            }
            if(status!=NO_ERROR||inFlight){
                // Retain code/trampolines for failed detaches or callbacks returning
                // through this DLL. Passthrough hooks remain valid until exit.
                retainedModule=hold;reloadRequiresRestart=true;
                if(core)say(held?"Unload fallback: code retained for safety. Restart the game before loading a new DLL.":"Could not retain module during unload; hook shutdown failed.");
            }else if(held)FreeLibrary(hold);
            hooked=false;
        }
        liveCore=nullptr;
        nativeGestureOwner=nullptr;nativeGestureDefer=nullptr;
    }
    bool HandleIncomingPacket(uint16_t id,uint32_t size,const uint8_t* data,uint8_t* modified,uint32_t,const uint8_t*,bool,bool blocked)override{
        if(blocked)return false;
        if(id==0x00a||id==0x00b){
worldReady=false;resetLogin=true;for(auto& entry:receivedLooks)entry=0;pendingLooks.clear();localSelectionKnown=false;return false;}


        if(id!=0x00d||size<0x4e||!data)return false;
        const uint8_t* packet=modified?modified:data;
        if(!(packet[0x0a]&0x10))return false; // Model fields are valid only with model-update mask.
        uint32_t server=0;uint16_t index=0,head=0,body=0;
        memcpy(&server,packet+4,4);memcpy(&index,packet+8,2);
        memcpy(&head,packet+0x4a,2);memcpy(&body,packet+0x4c,2);
        if(!server||index==0||index>=receivedLooks.size())return false;
        auto old=receivedLooks[index].exchange((uint64_t(server)<<32)|0x10000u|head);
        if(!othersEnabled.load()||!supportedRace(packet[0x49])||!hood::normal::supportedBody(body))return false;
        ++modelPackets;packetHead=head;packetIndex=index;
        bool changed=(uint32_t)(old>>32)!=server||!(old&0x10000)||(((uint16_t)old==0)!=(head==0));
        if(!changed)return false;
        ++headChanges;
        pendingLooks.queue(index,hood::peerToken(server,head),GetTickCount64());
        return false;
    }
    bool HandleCommand(int32_t mode,const char* command,bool injected)override{
        // Ashita's third argument indicates origin, not prior blocking. Macros
        // and addon-issued displayhead commands need the same delay as typing.
        // Only our single queued release below bypasses gesture interception.
        (void)injected;
        if(!command||shuttingDown.load())return false;
        std::string cmd=normalize(command);
        int display=displayHeadCommand(command);
        if(display>=0){
            // The native option-request hook handles changes before submission,
            // including commands that bypass Ashita's text handler entirely.
            if(displayRequestHooked)return false;
            if(!gesturePass.empty()&&GetTickCount64()<=gesturePassUntil&&cmd==gesturePass){gesturePass.clear();return false;}
            if(!gesturePass.empty()&&GetTickCount64()>gesturePassUntil)gesturePass.clear();
            if(gestureEnabled.load()&&localSelectionKnown.load()){
                bool hidden=display==2?!observedHeadHidden:display==0;
                if(hidden!=observedHeadHidden){
                    if(prepareGesture(hidden,hidden?"/displayhead off":"/displayhead on",false,mode))return true;
                }else gestureMessage="Displayhead already matches that setting; native command passed through.";
            }else if(gestureEnabled.load())gestureMessage="Gesture skipped: waiting for the client's displayhead setting.";
            return false;
        }
        if(cmd!="/hoodie"&&cmd.rfind("/hoodie ",0)!=0)return false;
        if(cmd=="/hoodie enable")requestedEnabled=true;
        if(cmd=="/hoodie disable"||cmd=="/hoodie stop")requestedEnabled=false;
        if(cmd=="/hoodie others on")requestedOthers=true;
        if(cmd=="/hoodie others off")requestedOthers=false;
        if(!startupAttempted&&(cmd=="/hoodie enable"||cmd=="/hoodie disable"||cmd=="/hoodie stop"||cmd=="/hoodie others on"||cmd=="/hoodie others off")){say("Preference saved for this session; waiting for a fully logged-in character.");return true;}
        if(cmd=="/hoodie others on"||cmd=="/hoodie others off"){
            bool enable=cmd=="/hoodie others on";
            if(enable&&(!parserVerified||!anyAssets())){say("Hoodie is unavailable.");return true;}
            othersEnabled=enable;
            unsigned refreshed=refreshMatching(false,true);
            say(enable?"Other players enabled.":"Other players disabled.");return true;
        }
        if(cmd=="/hoodie down"){
            if(!parserVerified||!anyAssets()){say("Hoodie is unavailable: check the plugin version and resources.");return true;}
            localSelection=hood::Variant::Down;localSelectionKnown=true;variantsEnabled=true;
            say("Lowered hood selected for next normal tunic equip. No forced refresh.");return true;
        }
        if(cmd=="/hoodie enable"){
            if(!parserVerified||!anyAssets()){say("Hoodie is unavailable: check the plugin version and resource installation.");return true;}
            localSelectionKnown=false;syncLocalSelection();variantsEnabled=true;
            refreshMatching(true,othersEnabled.load());
            say("Enabled. Use /displayhead on or off to raise or lower your hood.");return true;
        }
        if(cmd=="/hoodie disable"||cmd=="/hoodie stop"){
#ifdef HOODIE_TESTING

#endif
variantsEnabled=false;refreshMatching(true,true);say("Disabled. Original appearance restored.");return true;}
        if(cmd=="/hoodie gesture reload"){loadGesture(requestedAnimateChange);return true;}

#ifdef HOODIE_TESTING
        if(cmd=="/hoodie gesture test"){prepareGesture(false);return true;}
#endif


#ifdef HOODIE_TESTING
        if(cmd=="/hoodie gesture reverse"){prepareGesture(true);return true;}
#endif

        if(cmd=="/hoodie gesture off"){setAnimateChange(false);return true;}
        if(cmd=="/hoodie"){showSettings=!showSettings;return true;}
        if(cmd=="/hoodie status"){
            if(!worldReady.load()){say(startupFailed?"Could not start. Reload Hoodie to retry.":"Waiting for your character to finish loading.");return true;}
            core->GetChatManager()->Writef(207,false,"[hoodie] %s | Other players: %s | Animate change: %s",variantsEnabled.load()?"Enabled":"Disabled",othersEnabled.load()?"On":"Off",requestedAnimateChange?"On":"Off");return true;
        }
        say("Use /hoodie for settings, or /hoodie enable, disable, status.");
        return true;
    }
};
extern "C" __declspec(dllexport) double __stdcall expGetInterfaceVersion(){return ASHITA_INTERFACE_VERSION;}
extern "C" __declspec(dllexport) IPlugin* __stdcall expCreatePlugin(const char*){return new Hoodie;}
extern "C" __declspec(dllexport) void __stdcall expDestroyPlugin(IPlugin* p){delete p;}




