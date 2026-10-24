#pragma once
#include <cstdint>
#include <vector>
#include <memory>
#include <fstream>
#include <stdexcept>
#include <atomic>
#include <algorithm>
#include <cstring>

namespace hood {
enum class Variant:uint8_t {Up,Down};
struct Blob {
    const std::vector<uint8_t> bytes;
    explicit Blob(std::vector<uint8_t> data):bytes(std::move(data)) {
        if(bytes.empty() || bytes.size()>16*1024*1024)throw std::runtime_error("Invalid DAT length");
        size_t at=0;
        while(at<bytes.size()){
            if(bytes.size()-at<16)throw std::runtime_error("Truncated DAT section header");
            uint32_t header=0;std::memcpy(&header,bytes.data()+at+4,4);
            size_t size=((header>>7)&0x7ffffu)*16u;
            if(size<16 || size>bytes.size()-at)throw std::runtime_error("DAT section exceeds pinned buffer");
            at+=size;
        }
    }
    static std::shared_ptr<const Blob> load(const char* path){
        std::ifstream f(path,std::ios::binary|std::ios::ate);if(!f)throw std::runtime_error("Cannot open DAT");
        auto length=f.tellg();if(length<=0 || length>16*1024*1024)throw std::runtime_error("Invalid DAT length");
        std::vector<uint8_t> data(static_cast<size_t>(length));f.seekg(0);
        if(!f.read(reinterpret_cast<char*>(data.data()),data.size()))throw std::runtime_error("Incomplete DAT read");
        return std::make_shared<const Blob>(std::move(data));
    }
};
// Module-owned identity. This is NOT an FFXI model ID or a registered DAT slot.
struct Identity {
    uintptr_t actor;uint32_t resource;Variant variant;uint64_t generation;
    bool operator==(const Identity& rhs)const{return actor==rhs.actor&&resource==rhs.resource&&variant==rhs.variant&&generation==rhs.generation;}
};
struct Lease {
    const Identity identity;
    const std::shared_ptr<const Blob> blob;
    size_t size()const{return blob->bytes.size();}
    size_t read(size_t offset,void* destination,size_t capacity)const{
        if(offset>size())throw std::out_of_range("DAT offset");
        size_t count=std::min(capacity,size()-offset);
        if(count && !destination)throw std::invalid_argument("Null read destination");
        if(count)std::memcpy(destination,blob->bytes.data()+offset,count);
        return count;
    }
};
class Assets {
    std::shared_ptr<const Blob> up,down;
    std::atomic<Variant> selected{Variant::Up};
    const uint64_t generation;
    const uint32_t resourceKey;
public:
    Assets(const char* upPath,const char* downPath,uint64_t generationId,uint32_t key=10547):up(Blob::load(upPath)),down(Blob::load(downPath)),generation(generationId),resourceKey(key){}
    Variant selection()const{return selected.load();}
    void select(Variant v){selected.store(v);}
    std::shared_ptr<const Lease> beginFor(uintptr_t actor,uintptr_t verifiedActor,uint32_t resource,uint32_t callerRva,Variant v)const{
        if(!verifiedActor||actor!=verifiedActor||resource!=resourceKey||callerRva!=0xd3e1d)return {};
        return std::make_shared<const Lease>(Lease{{actor,resource,v,generation},v==Variant::Up?up:down});
    }
    std::shared_ptr<const Lease> begin(uintptr_t actor,uintptr_t local,uint32_t resource,uint32_t callerRva)const{
        // Only the observed tunic request from the observed actor-loading call site.
        if(!local || actor!=local || resource!=resourceKey || callerRva!=0xd3e1d)return {};
        Variant v=selected.load();return std::make_shared<const Lease>(Lease{{actor,resource,v,generation},v==Variant::Up?up:down});
    }
};
}

