#include "asset_store.h"
#include <cassert>
#include <thread>
#include <filesystem>
#include <cstdio>
int main(){
    using namespace hood;
    const char* up="package/resources/hoodie/hf-footman/up.dat";
    const char* down="package/resources/hoodie/hf-footman/down.dat";
    Assets taru("package/resources/hoodie/taru-footman/up.dat","package/resources/hoodie/taru-footman/down.dat",1,20075);
    assert(taru.beginFor(0x2000,0x2000,20075,0xd3e1d,Variant::Up));
    assert(taru.beginFor(0x2000,0x2000,20075,0xd3e1d,Variant::Down));
    assert(!taru.beginFor(0x2000,0x2000,10547,0xd3e1d,Variant::Down));
    assert(!taru.beginFor(0x2000,0x3000,20075,0xd3e1d,Variant::Down));
    Assets mithra("package/resources/hoodie/mithra-footman/up.dat","package/resources/hoodie/mithra-footman/down.dat",1,23475);
    assert(mithra.beginFor(0x3000,0x3000,23475,0xd3e1d,Variant::Up));
    assert(mithra.beginFor(0x3000,0x3000,23475,0xd3e1d,Variant::Down));
    assert(!mithra.beginFor(0x3000,0x3000,20075,0xd3e1d,Variant::Down));
    assert(!mithra.beginFor(0x3000,0x3001,23475,0xd3e1d,Variant::Down));
    assert(!taru.beginFor(0x3000,0x3000,23475,0xd3e1d,Variant::Down));
    Assets gl("package/resources/hoodie/galka-footman/up.dat","package/resources/hoodie/galka-footman/down.dat",1,26651);
    assert(gl.beginFor(0x7000,0x7000,26651,0xd3e1d,Variant::Up));
    assert(gl.beginFor(0x7000,0x7000,26651,0xd3e1d,Variant::Down));
    assert(!gl.beginFor(0x7000,0x7001,26651,0xd3e1d,Variant::Down));
    assert(!gl.beginFor(0x7000,0x7000,16899,0xd3e1d,Variant::Down));
    Assets ef("package/resources/hoodie/elvaanfemale-footman/up.dat","package/resources/hoodie/elvaanfemale-footman/down.dat",1,16899);
    assert(ef.beginFor(0x6000,0x6000,16899,0xd3e1d,Variant::Up));
    assert(ef.beginFor(0x6000,0x6000,16899,0xd3e1d,Variant::Down));
    assert(!ef.beginFor(0x6000,0x6001,16899,0xd3e1d,Variant::Down));
    assert(!ef.beginFor(0x6000,0x6000,13723,0xd3e1d,Variant::Down));
    Assets em("package/resources/hoodie/elvaanmale-footman/up.dat","package/resources/hoodie/elvaanmale-footman/down.dat",1,13723);
    assert(em.beginFor(0x5000,0x5000,13723,0xd3e1d,Variant::Up));
    assert(em.beginFor(0x5000,0x5000,13723,0xd3e1d,Variant::Down));
    assert(!em.beginFor(0x5000,0x5001,13723,0xd3e1d,Variant::Down));
    assert(!em.beginFor(0x5000,0x5000,7371,0xd3e1d,Variant::Down));
    Assets hm("package/resources/hoodie/humemale-footman/up.dat","package/resources/hoodie/humemale-footman/down.dat",1,7371);
    assert(hm.beginFor(0x4000,0x4000,7371,0xd3e1d,Variant::Up));
    assert(hm.beginFor(0x4000,0x4000,7371,0xd3e1d,Variant::Down));
    assert(!hm.beginFor(0x4000,0x4000,10547,0xd3e1d,Variant::Down));
    assert(!hm.beginFor(0x4000,0x4001,7371,0xd3e1d,Variant::Down));
    assert(!mithra.beginFor(0x4000,0x4000,7371,0xd3e1d,Variant::Down));
    const auto upSize=std::filesystem::file_size(up),downSize=std::filesystem::file_size(down);
    Assets assets(up,down,1);
    assert(!assets.beginFor(0x2000,0x2000,20075,0xd3e1d,Variant::Down));
    auto a=assets.begin(0x1000,0x1000,10547,0xd3e1d);assert(a&&a->size()==upSize);
    assert(!assets.begin(0x2000,0x1000,10547,0xd3e1d)); // Same armor on another actor.
    assert(!assets.begin(0,0,10547,0xd3e1d));
    assert(!assets.begin(0x1000,0x1000,10548,0xd3e1d));
    assert(!assets.begin(0x1000,0x1000,10547,0xd3b1b));
    assets.select(Variant::Down);auto b=assets.begin(0x1000,0x1000,10547,0xd3e1d);assert(b&&b->size()==downSize);
    assert(a->size()==upSize);assert(!(a->identity==b->identity));
    auto b2=assets.begin(0x1000,0x1000,10547,0xd3e1d);assert(b->identity==b2->identity);
    auto remoteDown=assets.beginFor(0x2000,0x2000,10547,0xd3e1d,Variant::Down);
    auto remoteUp=assets.beginFor(0x3000,0x3000,10547,0xd3e1d,Variant::Up);
    assert(remoteDown&&remoteDown->size()==downSize&&remoteUp&&remoteUp->size()==upSize);
    assert(assets.selection()==Variant::Down); // Peer selection must not change local selection.
    assert(remoteDown->identity.actor!=remoteUp->identity.actor);
    assert(!assets.beginFor(0x2000,0x3000,10547,0xd3e1d,Variant::Down));
    assert(!assets.beginFor(0x2000,0x2000,10548,0xd3e1d,Variant::Down));
    assert(!assets.beginFor(0x2000,0x2000,10547,0,Variant::Down));
    Assets updated(up,down,2);assert(!(a->identity==updated.begin(0x1000,0x1000,10547,0xd3e1d)->identity));
    std::vector<uint8_t> buffer(b->size());assert(b->read(0,buffer.data(),buffer.size())==buffer.size());Blob complete(std::move(buffer));
    assert(b->read(b->size(),nullptr,100)==0);
    bool rejected=false;try{b->read(SIZE_MAX,nullptr,100);}catch(const std::out_of_range&){rejected=true;}assert(rejected);
    rejected=false;try{b->read(0,nullptr,10);}catch(const std::invalid_argument&){rejected=true;}assert(rejected);
    // Reproduce the original mismatch: down contents truncated to the old up size.
    std::vector<uint8_t> truncated(a->size());b->read(0,truncated.data(),truncated.size());
    rejected=false;try{Blob invalid(std::move(truncated));}catch(const std::runtime_error&){rejected=true;}assert(rejected);
    // Mid-request toggles cannot alter a lease's size/content pair.
    std::atomic<bool> stop{false};std::thread toggler([&]{while(!stop){assets.select(Variant::Up);assets.select(Variant::Down);}});
    for(unsigned i=0;i<10000;++i){auto request=assets.begin(0x1000,0x1000,10547,0xd3e1d);auto length=request->size();std::vector<uint8_t> copy(length);assert(request->read(0,copy.data(),length)==length);Blob valid(std::move(copy));assert(request->size()==length);}
    stop=true;toggler.join();
    // A request retains its bytes even after its store is destroyed.
    std::shared_ptr<const Lease> held;
    {Assets temporary(up,down,3);held=temporary.begin(0x1000,0x1000,10547,0xd3e1d);}assert(held->size()==upSize);
    puts("Asset layer passed actor/caller/resource isolation, cache identities, generation, bounds, malformed-buffer regression, lifetime and 10000 concurrent-toggle pinned reads.");
}
