// Exercise the actual probe callbacks and Detours trampoline in a separate x86 process.
#define HOODIE_TESTING
#include "hoodie.cpp"
#include <cassert>
#include <cstdio>
struct Fixture {unsigned calls=0;uint32_t expected=0;void* value=nullptr;};
__declspec(noinline) static uintptr_t __fastcall originalAttach(void* actor,void*,void* handle,uint32_t code,uint32_t stamp,uint32_t mode){
    auto f=static_cast<Fixture*>(actor);assert(handle==f->value&&code==f->expected&&stamp==77&&mode==1);++f->calls;SetLastError(4321);return 0xabcdef12;
}
__declspec(noinline) static uintptr_t __fastcall originalRelease(void* context,void*,void* value){
    auto f=static_cast<Fixture*>(context);assert(value==f->value);++f->calls;SetLastError(5678);return 0x1234abcd;
}
struct ParserFixture {
    unsigned char root[0x38]={};uintptr_t slot=0;const hood::Lease* lease=nullptr;unsigned calls=0;
};
__declspec(noinline) static void* __fastcall originalParser(void* context,void*,void** output,const uint8_t* bytes,uintptr_t owner){
    auto f=static_cast<ParserFixture*>(context);assert(owner==0&&bytes==f->lease->blob->bytes.data());
    hood::Blob checked(std::vector<uint8_t>(bytes,bytes+f->lease->size()));
    ++f->calls;f->slot=(uintptr_t)f->root;*output=&f->slot;return output;
}
__declspec(noinline) static void* __fastcall originalFixture(void* context,void*,void** output,uint32_t key){
    auto f=static_cast<Fixture*>(context);assert(key==f->expected);++f->calls;*output=f->value;SetLastError(1234);return output;
}
__declspec(naked) static void* __stdcall invokeOwner(void* context,void** output,uint32_t key,RequestFn target,uintptr_t owner){
    __asm {
        push edi
        mov edi,[esp+24]
        mov ecx,[esp+8]
        mov edx,[esp+20]
        push [esp+16]
        push [esp+16]
        call edx
        pop edi
        ret 20
    }
}
struct HoodieFixtureAccess {
 static void hooked(Hoodie& p){p.hooked=true;}static bool settings(const Hoodie& p){return p.showSettings;}
 static std::string message(const Hoodie& p){return p.gestureMessage;}
 static void resetMessage(Hoodie& p){p.gestureMessage="untouched";}
 static void queuedRelease(Hoodie& p){p.gesturePass="/displayhead off";p.gesturePassUntil=GetTickCount64()+2000;}
 static bool releaseConsumed(const Hoodie& p){return p.gesturePass.empty();}
 static void animate(Hoodie& p,bool enabled){p.setAnimateChange(enabled);}
 static void tickGesture(Hoodie& p){p.tickGesture();}
 static unsigned actorQueries(const Hoodie& p){return p.gestureActorQueries;}
 static void preferences(Hoodie& p,const std::filesystem::path& path){p.configurePreferences(path);}
 static bool requestedAnimate(const Hoodie& p){return p.requestedAnimateChange;}
};
static LONG WINAPI failDetach(PVOID*,PVOID){return ERROR_INVALID_BLOCK;}
struct DisplayFixture {unsigned calls=0,option=0,value=0;bool defer=false,lastHidden=false;hood::gesture::Sequence sequence;uint64_t now=100;} displayFixture;
__declspec(noinline) static bool __cdecl originalDisplay(unsigned option,unsigned value){++displayFixture.calls;displayFixture.option=option;displayFixture.value=value;SetLastError(2468);return uint8_t(value)!=0;}
static void checkPreferencesInFreshProcess(const std::filesystem::path& file,bool enabled){
 wchar_t module[MAX_PATH]{};assert(GetModuleFileNameW(nullptr,module,MAX_PATH));
 std::wstring command=L"\""+std::wstring(module)+L"\" --check-animate \""+file.wstring()+L"\" "+(enabled?L"1":L"0");
 STARTUPINFOW startup{};startup.cb=sizeof(startup);PROCESS_INFORMATION child{};
 assert(CreateProcessW(module,command.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,nullptr,&startup,&child));
 assert(WaitForSingleObject(child.hProcess,5000)==WAIT_OBJECT_0);DWORD result=1;assert(GetExitCodeProcess(child.hProcess,&result)&&result==0);
 CloseHandle(child.hThread);CloseHandle(child.hProcess);
}
int main(int argc,char** argv){ captureEnabled=true;
    if(argc==4&&std::strcmp(argv[1],"--check-animate")==0){Hoodie child;HoodieFixtureAccess::preferences(child,argv[2]);return HoodieFixtureAccess::requestedAnimate(child)==(std::strcmp(argv[3],"1")==0)?0:7;}
    {
        auto config=std::filesystem::absolute("build")/("preferences-fixture-"+std::to_string(GetCurrentProcessId())+".ini");
        std::filesystem::remove(config);
        Hoodie first;HoodieFixtureAccess::preferences(first,config);assert(!HoodieFixtureAccess::requestedAnimate(first));
        assert(WritePrivateProfileStringW(L"Other",L"Keep",L"42",config.c_str()));
        auto package=std::make_shared<hood::gesture::Package>();package->race=1;gesturePackages.races[0]=package;
        HoodieFixtureAccess::animate(first,true);
        checkPreferencesInFreshProcess(config,true);
        Hoodie reloaded;HoodieFixtureAccess::preferences(reloaded,config);assert(HoodieFixtureAccess::requestedAnimate(reloaded));
        // Transient runtime disable/unload must not overwrite the saved choice.
        gestureEnabled=false;Hoodie afterShutdown;HoodieFixtureAccess::preferences(afterShutdown,config);assert(HoodieFixtureAccess::requestedAnimate(afterShutdown));
        HoodieFixtureAccess::animate(reloaded,false);
        checkPreferencesInFreshProcess(config,false);
        Hoodie nextSession;HoodieFixtureAccess::preferences(nextSession,config);assert(!HoodieFixtureAccess::requestedAnimate(nextSession));
        assert(GetPrivateProfileIntW(L"Other",L"Keep",0,config.c_str())==42);
        WritePrivateProfileStringW(nullptr,nullptr,nullptr,config.c_str());std::filesystem::remove(config);gesturePackages={};
        puts("Animate change persistence passed: missing-file default, on/off across new plugin instances, runtime shutdown preserves choice, unrelated settings retained.");
    }
    {
        // The real checkbox path must not clear readiness or rebuild the body.
        // No core is installed here: any attempted model refresh would fail.
        Hoodie p;auto package=std::make_shared<hood::gesture::Package>();package->race=1;
        gesturePackages.races[0]=package;gestureLoadedActor=0x12345;gestureLoadedRace=1;gestureLoadedBody=47;
        HoodieFixtureAccess::animate(p,true);assert(gestureEnabled.load());assert(gestureLoadedActor.load()==0x12345);
        HoodieFixtureAccess::animate(p,false);assert(!gestureEnabled.load());assert(gestureLoadedActor.load()==0x12345);
        HoodieFixtureAccess::animate(p,true);assert(gestureEnabled.load());assert(gesturePackages.forRace(1)==package);
        auto queries=HoodieFixtureAccess::actorQueries(p);
        for(unsigned i=0;i<1000;++i)HoodieFixtureAccess::tickGesture(p);
        assert(HoodieFixtureAccess::actorQueries(p)==queries);
        HoodieFixtureAccess::animate(p,false);queries=HoodieFixtureAccess::actorQueries(p);
        for(unsigned i=0;i<1000;++i)HoodieFixtureAccess::tickGesture(p);
        assert(HoodieFixtureAccess::actorQueries(p)==queries);
        assert(gestureLoadedRace.load()==1&&gestureLoadedBody.load()==47);
        gestureEnabled=false;gesturePackages={};gestureLoadedActor=0;
        puts("Animate change checkbox passed: uses primed library, no body rebuild, no readiness reset, off/on preserves prepared resources.");
    }
    {
        // Exercise the exact cdecl hook/trampoline, including a request held
        // for 400 ms before calling the unchanged native submission routine.
        displayRequest=originalDisplay;shuttingDown=false;
        nativeGestureOwner=&displayFixture;
        nativeGestureDefer=[](void* owner,bool hidden){auto& f=*static_cast<DisplayFixture*>(owner);f.lastHidden=hidden;if(!f.defer)return false;hood::gesture::Package p;p.race=2;p.duration=700;p.marker=351;hood::gesture::Actor actor{1,2,3,2,47,1,0,0,0,true};return f.sequence.begin(actor,f.now,p,hidden,400);};
        assert(DetourTransactionBegin()==NO_ERROR);assert(DetourUpdateThread(GetCurrentThread())==NO_ERROR);assert(DetourAttach(&(PVOID&)displayRequest,displayRequestHook)==NO_ERROR);assert(DetourTransactionCommit()==NO_ERROR);
        auto entry=&originalDisplay;
        assert(entry(10,1));assert(displayFixture.calls==1&&displayFixture.option==10&&displayFixture.value==1&&GetLastError()==2468);
        displayFixture.defer=true;assert(!entry(9,0));assert(displayFixture.calls==2&&displayFixture.option==9);
        assert(entry(10,1));assert(displayFixture.calls==2);
        hood::gesture::Actor actor{1,2,3,2,47,1,0,0,0,true};assert(displayFixture.sequence.tick(actor,499)==hood::gesture::Event::None);assert(displayFixture.calls==2);
        assert(displayFixture.sequence.tick(actor,500)==hood::gesture::Event::Contact);assert(displayRequest(10,1));assert(displayFixture.calls==3);
        assert(displayFixture.sequence.tick(actor,501)==hood::gesture::Event::None);assert(displayFixture.calls==3);
        shuttingDown=true;assert(entry(10,1));assert(displayFixture.calls==4);shuttingDown=false;
        // Bare /displayhead modifies CL and pushes ECX without clearing its
        // upper bytes. Exercise the actual native hook with that calling form.
        auto beforeToggle=displayFixture.calls;
        for(unsigned packed:{0xa5a5a500u,0xa5a5a501u,0x123400ffu}){
            displayFixture.sequence.cancel();assert(entry(10,packed));
            assert(displayFixture.calls==beforeToggle&&displayFixture.lastHidden==(uint8_t(packed)!=0));
        }
        displayFixture.defer=false;assert(!entry(10,0xa5a5a500u));assert(displayFixture.value==0xa5a5a500u);
        assert(entry(10,0xa5a5a501u));assert(displayFixture.value==0xa5a5a501u);
        assert(DetourTransactionBegin()==NO_ERROR);assert(DetourUpdateThread(GetCurrentThread())==NO_ERROR);assert(DetourDetach(&(PVOID&)displayRequest,displayRequestHook)==NO_ERROR);assert(DetourTransactionCommit()==NO_ERROR);
        nativeGestureOwner=nullptr;nativeGestureDefer=nullptr;displayRequest=nullptr;
        puts("Native display request cdecl hook passed: disabled passthrough, other options, 400ms hold, one trampoline release, return/arguments/last-error and shutdown passthrough.");
    }
    {
        hood::LoginGate gate;hood::WorldIdentity first{1030,123456,230,0x10000},second{1031,654321,231,0x20000};
        assert(!gate.update(false,first,0));assert(!gate.update(false,first,10000)); // Title/character select cannot start.
        assert(!gate.update(true,first,11000));assert(!gate.update(true,first,12499));assert(gate.update(true,first,12500));
        assert(gate.update(true,first,13000)); // Remain ready without restarting the timer.
        assert(!gate.update(false,first,14000)); // Logout or zoning suspends immediately.
        assert(!gate.update(true,second,15000));assert(!gate.update(true,second,16499));assert(gate.update(true,second,16500));
        auto missing=second;missing.actor=0;assert(!gate.update(true,missing,17000));
        assert(!gate.update(true,second,20000));assert(gate.update(true,second,21500));
        auto moved=second;moved.zone=232;assert(!gate.update(true,moved,22000));assert(gate.update(true,moved,23500));
        missing=moved;missing.server=0;assert(!gate.update(true,missing,24000));
        assert(!gate.update(true,moved,25000));gate.reset();assert(!gate.update(true,moved,27000));assert(gate.update(true,moved,28500));
        puts("Login gate passed title/select, stable first login, zoning/logout, character change, missing actor/server and zone-reset transitions.");
        gate.reset();auto optionalZone=first;optionalZone.zone=0;
        assert(!gate.update(true,optionalZone,30000));assert(gate.update(true,optionalZone,31500));
        puts("Regression passed: a loaded character with a valid identity/actor and unset zone can activate; optional zone changes still re-arm the gate.");
        auto rebuilt=optionalZone;rebuilt.actor=0x99999;
        assert(gate.update(true,rebuilt,31501,true)); // Apply preview changes race/skeleton/actor, not login.
        rebuilt.actor=0x88888;assert(gate.update(true,rebuilt,31502,true));
        auto different=rebuilt;++different.server;assert(!gate.update(true,different,31503,true));
        assert(!gate.update(false,different,31504,true));
        assert(!gate.update(true,different,31505,true));assert(gate.update(true,different,33005,true));
        puts("Regression passed: in-world actor rebuilds stay ready; different characters and logout still re-arm startup stabilization.");
    }
    {
        std::vector<uint8_t> settings(0x2cfdc);uintptr_t pointer=(uintptr_t)settings.data();bool hidden=true;
        assert(hood::readHeadHidden((uintptr_t)&pointer,hidden)&&!hidden);
        settings[0x2cfdb]=8;assert(hood::readHeadHidden((uintptr_t)&pointer,hidden)&&hidden);
        settings[0x2cfdb]=0xf7;assert(hood::readHeadHidden((uintptr_t)&pointer,hidden)&&!hidden);
        pointer=0;assert(!hood::readHeadHidden((uintptr_t)&pointer,hidden));assert(!hood::readHeadHidden(1,hidden));
        localSelectionKnown=false;localSelection=hood::Variant::Up;
        assert(adoptLocalDisplay(true));assert(localSelection.load()==hood::Variant::Down);
        assert(!adoptLocalDisplay(true)); // A blocked command leaves the native flag unchanged.
        assert(adoptLocalDisplay(false));assert(localSelection.load()==hood::Variant::Up);
        assert(!adoptLocalDisplay(false));assert(adoptLocalDisplay(true));
        localSelectionKnown=false;
        puts("Native hidden-head flag: initial sync, accepted changes, unchanged/menu-blocked state, unrelated bits and invalid pointer checks passed.");
    }
    {
        Hoodie p;shuttingDown=false;variantsEnabled=true;localSelection=hood::Variant::Up;localSelectionKnown=true;
        assert(!p.HandleCommand(0,"/displayhead off",false));assert(localSelection.load()==hood::Variant::Up);
        assert(!p.HandleCommand(0,"/displayhead off",true));assert(localSelection.load()==hood::Variant::Up);
        assert(p.HandleCommand(0,"/hoodie",true));assert(HoodieFixtureAccess::settings(p));
        assert(p.HandleCommand(0,"/hoodie",false));assert(!HoodieFixtureAccess::settings(p));
        // Injected means macro/addon origin, not blocked. Only the optional
        // animation may intercept; a single queued release must not recurse.
        observedHeadHidden=false;gestureEnabled=false;HoodieFixtureAccess::resetMessage(p);
        assert(!p.HandleCommand(0,"/displayhead off",true));assert(HoodieFixtureAccess::message(p)=="untouched");
        assert(!p.HandleCommand(0,"/displayhead off",false));assert(HoodieFixtureAccess::message(p)=="untouched");
        gestureEnabled=true;
        assert(!p.HandleCommand(0,"/displayhead off",true));assert(HoodieFixtureAccess::message(p).find("Last change skipped animation:")==0);
        HoodieFixtureAccess::resetMessage(p);HoodieFixtureAccess::queuedRelease(p);
        assert(!p.HandleCommand(0,"/displayhead off",true));assert(HoodieFixtureAccess::releaseConsumed(p));assert(HoodieFixtureAccess::message(p)=="untouched");
        assert(!p.HandleCommand(0,"/displayhead off",true));assert(HoodieFixtureAccess::message(p).find("Last change skipped animation:")==0);
        gestureEnabled=false;
        variantsEnabled=false;localSelectionKnown=false;
        puts("Typed/injected commands, optional-animation-off passthrough, gesture eligibility, single queued release bypass and no optimistic displayhead state change passed.");
    }
    struct ResourceFixture {unsigned char padding[0x34]={};uint32_t key=10547;} resource;
    uintptr_t slot=(uintptr_t)&resource;
    Fixture fixture;fixture.value=&slot;auto entry=reinterpret_cast<RequestFn>(&originalFixture);
    for(bool async:{true,false}){
        auto& trampoline=async?asyncRequest:syncRequest;trampoline=entry;
        assert(DetourTransactionBegin()==NO_ERROR);assert(DetourUpdateThread(GetCurrentThread())==NO_ERROR);
        assert(DetourAttach(&(PVOID&)trampoline,async?asyncHook:syncHook)==NO_ERROR);assert(DetourTransactionCommit()==NO_ERROR);
        for(uint32_t k=0;k<1000;++k){fixture.expected=k;void* out=nullptr;auto result=entry(&fixture,&out,k);assert(result==&out);assert(out==fixture.value);assert(GetLastError()==1234);}
        assert(DetourTransactionBegin()==NO_ERROR);assert(DetourUpdateThread(GetCurrentThread())==NO_ERROR);
        assert(DetourDetach(&(PVOID&)trampoline,async?asyncHook:syncHook)==NO_ERROR);assert(DetourTransactionCommit()==NO_ERROR);
    }
    assert(fixture.calls==2000);assert(reads.load()==2000);reads=0;
    cacheRequest=entry;actorBaseline=0x12345678;
    assert(DetourTransactionBegin()==NO_ERROR);assert(DetourUpdateThread(GetCurrentThread())==NO_ERROR);
    assert(DetourAttach(&(PVOID&)cacheRequest,cacheHook)==NO_ERROR);assert(DetourTransactionCommit()==NO_ERROR);
    for(uint32_t k=0;k<1000;++k){
        fixture.expected=k;void* out=nullptr;uintptr_t owner=k%2?0x12345678:0x87654321;
        auto result=invokeOwner(&fixture,&out,k,entry,owner);
        assert(result==&out&&out==fixture.value&&GetLastError()==1234);
        auto sample=samples[(reads.load()-1)%samples.size()];
        assert(sample.lookup&&sample.owner==owner&&sample.baselineOwner==0x12345678&&sample.resourceKey==k&&sample.context==(uintptr_t)&fixture);
        assert(sample.result==(uintptr_t)&out&&sample.handle==(uintptr_t)&slot&&sample.resourceObject==(uintptr_t)&resource&&sample.objectKey==10547&&sample.handleReadable&&sample.objectReadable);
    }
    assert(DetourTransactionBegin()==NO_ERROR);assert(DetourUpdateThread(GetCurrentThread())==NO_ERROR);
    assert(DetourDetach(&(PVOID&)cacheRequest,cacheHook)==NO_ERROR);assert(DetourTransactionCommit()==NO_ERROR);
    assert(fixture.calls==3000&&reads.load()==1000);reads=0;
    auto attachmentEntry=reinterpret_cast<AttachFn>(&originalAttach);attachResource=attachmentEntry;
    assert(DetourTransactionBegin()==NO_ERROR);assert(DetourUpdateThread(GetCurrentThread())==NO_ERROR);
    assert(DetourAttach(&(PVOID&)attachResource,attachHook)==NO_ERROR);assert(DetourTransactionCommit()==NO_ERROR);
    for(uint32_t k=0;k<1000;++k){
        fixture.expected=k;assert(attachmentEntry(&fixture,fixture.value,k,77,1)==0xabcdef12);assert(GetLastError()==4321);
        auto sample=samples[(reads.load()-1)%samples.size()];assert(sample.attachment&&sample.owner==(uintptr_t)&fixture&&sample.objectKey==10547&&sample.modelCode==k&&sample.stamp==77&&sample.objectReadable);
    }
    assert(DetourTransactionBegin()==NO_ERROR);assert(DetourUpdateThread(GetCurrentThread())==NO_ERROR);
    assert(DetourDetach(&(PVOID&)attachResource,attachHook)==NO_ERROR);assert(DetourTransactionCommit()==NO_ERROR);
    assert(fixture.calls==4000&&reads.load()==1000);
    assert(writeCapture("build/capture-fixture.txt"));
    {std::ifstream report("build/capture-fixture.txt");std::string text((std::istreambuf_iterator<char>(report)),{});assert(text.find("ATTACHMENT")!=std::string::npos&&text.find("Capture state: RECORDING")!=std::string::npos);}
    reads=0;
    for(bool slotCase:{false,true}){
        auto releaseEntry=reinterpret_cast<ReleaseFn>(&originalRelease);auto& trampoline=slotCase?releaseSlot:releaseResource;trampoline=releaseEntry;fixture.value=slotCase?(void*)&slot:(void*)&resource;
        assert(DetourTransactionBegin()==NO_ERROR);assert(DetourUpdateThread(GetCurrentThread())==NO_ERROR);
        assert(DetourAttach(&(PVOID&)trampoline,slotCase?slotReleaseHook:resourceReleaseHook)==NO_ERROR);assert(DetourTransactionCommit()==NO_ERROR);
        for(unsigned k=0;k<1000;++k){
            assert(releaseEntry(&fixture,fixture.value)==0x1234abcd&&GetLastError()==5678);
            auto sample=samples[reads.load()-1];assert(sample.objectKey==10547&&sample.objectReadable);assert(slotCase?sample.slotRelease:sample.released);
        }
        assert(DetourTransactionBegin()==NO_ERROR);assert(DetourUpdateThread(GetCurrentThread())==NO_ERROR);
        assert(DetourDetach(&(PVOID&)trampoline,slotCase?slotReleaseHook:resourceReleaseHook)==NO_ERROR);assert(DetourTransactionCommit()==NO_ERROR);
    }
    assert(fixture.calls==6000&&reads.load()==2000);
    assert(writeCapture("build/capture-fixture.txt"));
    {std::ifstream report("build/capture-fixture.txt");std::string text((std::istreambuf_iterator<char>(report)),{});assert(text.find("RESOURCE_RELEASE")!=std::string::npos&&text.find("SLOT_RELEASE")!=std::string::npos);}
    reads=2048;record(Sample{});assert(!captureEnabled.load()&&reads.load()==2048);record(Sample{});assert(reads.load()==2048);
    assert(writeCapture("build/capture-fixture.txt"));
    {std::ifstream report("build/capture-fixture.txt");std::string text((std::istreambuf_iterator<char>(report)),{});assert(text.find("Capture state: STOPPED")!=std::string::npos);}
    {captureEnabled=true;std::atomic<bool> stop{false};std::condition_variable wake;std::mutex mutex;
     std::thread writer([&]{runCaptureWriter("build/capture-timer.txt",stop,wake,mutex,25,5);});writer.join();assert(!captureEnabled.load());}
    {captureEnabled=true;std::atomic<bool> stop{false};std::condition_variable wake;std::mutex mutex;
     std::thread writer([&]{runCaptureWriter("build/capture-stop.txt",stop,wake,mutex);});stop=true;wake.notify_all();writer.join();assert(!captureEnabled.load());}
    for(unsigned r=1;r<=8;++r)assert(supportedRace(r));
    assert(!supportedRace(0)&&!supportedRace(9));
    {
        hood::Assets assets("package/resources/hoodie/hf-footman/up.dat","package/resources/hoodie/hf-footman/down.dat",1);assets.select(hood::Variant::Down);
        auto lease=assets.begin(0x1000,0x1000,10547,0xd3e1d);ParserFixture f;f.lease=lease.get();parseModel=reinterpret_cast<ParseFn>(&originalParser);
        for(unsigned i=0;i<100;++i){void* output=nullptr;assert(constructVariant(*lease,&f,&output,0xf0000000u+i));assert(output==&f.slot);uint32_t key;memcpy(&key,f.root+0x34,4);assert(key==0xf0000000u+i);}
        assert(f.calls==100&&lease->size()==std::filesystem::file_size("package/resources/hoodie/hf-footman/down.dat"));
    }
    {
        Hoodie plugin;uint8_t packet[0x60]={};uint32_t server=123456;uint16_t index=1030,head=0x1003,body=0x2003;
        memcpy(packet+4,&server,4);memcpy(packet+8,&index,2);packet[0xa]=0x10;packet[0x49]=2;
        memcpy(packet+0x4a,&head,2);memcpy(packet+0x4c,&body,2);othersEnabled=false;
        assert(!plugin.HandleIncomingPacket(0xd,sizeof(packet),packet,packet,0,nullptr,false,false));
        assert((uint32_t)(receivedLooks[index].load()>>32)==server&&(uint16_t)receivedLooks[index].load()==head);
        head=0;memcpy(packet+0x4a,&head,2);packet[0xa]=0;
        plugin.HandleIncomingPacket(0xd,sizeof(packet),packet,packet,0,nullptr,false,false);
        assert((uint16_t)receivedLooks[index].load()==0x1003); // Ignore absent model fields.
        packet[0xa]=0x10;plugin.HandleIncomingPacket(0xd,16,packet,packet,0,nullptr,false,false);
        assert((uint16_t)receivedLooks[index].load()==0x1003); // Bounds check.
        plugin.HandleIncomingPacket(0xd,sizeof(packet),packet,packet,0,nullptr,false,true);
        assert((uint16_t)receivedLooks[index].load()==0x1003); // Blocked update ignored.
        plugin.HandleIncomingPacket(0xd,sizeof(packet),packet,packet,0,nullptr,false,false);
        assert((uint16_t)receivedLooks[index].load()==0&&(receivedLooks[index].load()&0x10000));
        plugin.HandleIncomingPacket(0xa,0,nullptr,nullptr,0,nullptr,false,false);assert(receivedLooks[index].load()==0);
    }
    {
        unsigned char code[]={0x8b,0x44,0x24,0x08};
        assert(hood::matchesCode((uintptr_t)code,sizeof(code),0,code,sizeof(code)));
        assert(!hood::matchesCode(0,1,0x730e0,code,sizeof(code)));
        assert(!hood::matchesCode((uintptr_t)code,sizeof(code),3,code,sizeof(code)));
        hood::PendingLooks pending;pending.queue(17,101);assert(pending.get(17)==101);
        pending.queue(17,202);assert(!pending.complete(17,101));assert(pending.get(17)==202);
        assert(pending.complete(17,202)&&pending.get(17)==0);
        Hoodie plugin;uint8_t packet[0x60]={};uint32_t server=123456;uint16_t index=1030,head=0,body=0x2003;
        memcpy(packet+4,&server,4);memcpy(packet+8,&index,2);packet[0xa]=0x10;packet[0x49]=2;
        memcpy(packet+0x4a,&head,2);memcpy(packet+0x4c,&body,2);othersEnabled=true;liveCore=nullptr;pendingLooks.clear();
        std::thread incoming([&]{plugin.HandleIncomingPacket(0xd,sizeof(packet),packet,packet,0,nullptr,false,false);});incoming.join();
        auto queued=pendingLooks.get(index);assert(queued&&(uint16_t)queued==0);
        drainPeerRefreshes();assert(pendingLooks.get(index)==queued); // Actor/core not ready: retain.
        plugin.HandleIncomingPacket(0xd,sizeof(packet),packet,packet,0,nullptr,false,false);assert(pendingLooks.get(index)==queued);
        head=0x1003;memcpy(packet+0x4a,&head,2);plugin.HandleIncomingPacket(0xd,sizeof(packet),packet,packet,0,nullptr,false,false);
        auto visible=pendingLooks.get(index);auto changes=headChanges.load();assert(visible==hood::peerToken(server,head));
        head=0x1005;memcpy(packet+0x4a,&head,2);plugin.HandleIncomingPacket(0xd,sizeof(packet),packet,packet,0,nullptr,false,false);
        assert(headChanges.load()==changes&&pendingLooks.get(index)==visible); // Visible gear swaps do not queue another hood change.
        plugin.HandleIncomingPacket(0xa,0,nullptr,nullptr,0,nullptr,false,false);assert(pendingLooks.get(index)==0);
        auto fallback=std::make_unique<hood::Assets>("package/resources/hoodie/taru-footman/up.dat","package/resources/hoodie/taru-footman/down.dat",1,20075);
        variantAssets.reset();taruAssets=std::move(fallback);assert(anyAssets());taruAssets.reset();assert(!anyAssets());
        assert(hood::activeHooks.load()==0);{hood::HookCall active;assert(hood::activeHooks.load()==1);}assert(hood::activeHooks.load()==0);
    }
    {
        auto open=DetourCodeFromPointer((PVOID)realOpen,nullptr);auto find=DetourCodeFromPointer((PVOID)realFind,nullptr);requestHooks=attachmentHooked=releaseHooks=false;captureEnabled=false;
        auto attachFiles=[&]{assert(DetourTransactionBegin()==NO_ERROR);hood::HookThreads threads;assert(threads.enroll());assert(DetourAttach(&(PVOID&)realOpen,openHook)==NO_ERROR);assert(DetourAttach(&(PVOID&)realFind,findHook)==NO_ERROR);assert(DetourTransactionCommit()==NO_ERROR);};
        attachFiles();Hoodie normal;HoodieFixtureAccess::hooked(normal);normal.Release();assert((PVOID)realOpen==open&&(PVOID)realFind==find);
        attachFiles();Hoodie failure;HoodieFixtureAccess::hooked(failure);detachHookApi=failDetach;failure.Release();detachHookApi=DetourDetach;
        assert(reloadRequiresRestart.load()&&retainedModule&&(PVOID)realOpen!=open);
        assert(DetourTransactionBegin()==NO_ERROR);assert(DetourUpdateThread(GetCurrentThread())==NO_ERROR);assert(DetourDetach(&(PVOID&)realOpen,openHook)==NO_ERROR);assert(DetourDetach(&(PVOID&)realFind,findHook)==NO_ERROR);assert(DetourTransactionCommit()==NO_ERROR);
        assert((PVOID)realOpen==open&&(PVOID)realFind==find&&hood::activeHooks.load()==0);
    }
    puts("Actual file-hook shutdown passed; injected detach failure aborted safely and retained module code.");
    puts("Shutdown accounting, guarded signature bounds, deferred peer packet retry/coalescing and non-HumeF asset availability passed.");
    puts("Passed 2000 x86 resource probe calls through real attach/trampoline/detach; context, key, output, return and last-error preserved.");
    puts("Passed 1000 cache-entry probe calls with exact EDI owner capture and local/other actor distinction; arguments, return and error preserved.");
    puts("Passed 1000 attachment-probe calls preserving actor/handle/model/generation/mode, return and error; object key observed.");
    puts("Passed 2000 resource/slot release calls, saved combined report, and bounded-buffer stop checks.");
    puts("Automatic writer passed timed-stop and unload/notify-stop checks.");
    puts("Native parser bridge passed 100 full-DAT fixture calls, output handoff and private in-memory resource tags; no retail slot mapping.");
}


