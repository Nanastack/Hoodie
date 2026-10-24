#pragma once
#include <array>
#include <atomic>
#include <cstdint>
#include <mutex>
#include <algorithm>
namespace hood {
constexpr uint16_t bodyRefreshFlag=0x0004;
constexpr uint16_t refreshBody(uint16_t existing){return existing|bodyRefreshFlag;}
struct PendingLooks {
    static constexpr unsigned capacity=0x900,maxAttempts=3;
    static constexpr uint64_t retryMs=100,lifetimeMs=2000;
    struct Batch {std::array<unsigned,16> indices{};unsigned count=0;};
private:
    struct State {uint64_t value=0,created=0,next=0;unsigned attempts=0;};
    mutable std::mutex mutex;
    std::array<State,capacity> values{};
    std::array<unsigned,capacity> indices{},positions{};
    std::atomic<unsigned> count{0};unsigned cursor=0;
    void erase(unsigned index){unsigned size=count.load(),position=positions[index],last=indices[size-1];indices[position]=last;positions[last]=position;values[index]={};count=size-1;if(cursor>=size-1)cursor=0;}
public:
    std::atomic<unsigned> expired{0};
    bool empty()const{return count.load()==0;}
    unsigned size()const{return count.load();}
    void clear(){std::lock_guard<std::mutex> g(mutex);values.fill({});count=0;cursor=0;}
    void queue(unsigned index,uint64_t value,uint64_t now=0){if(!index||index>=capacity||!value)return;std::lock_guard<std::mutex> g(mutex);
        auto& state=values[index];if(state.value==value)return;
        if(!state.value){unsigned position=count.load();indices[position]=index;positions[index]=position;count=position+1;}
        state={value,now,now,0};
    }
    uint64_t get(unsigned index)const{if(!index||index>=capacity)return 0;std::lock_guard<std::mutex> g(mutex);return values[index].value;}
    bool complete(unsigned index,uint64_t value){if(!index||index>=capacity)return false;std::lock_guard<std::mutex> g(mutex);if(!value||values[index].value!=value)return false;erase(index);return true;}
    Batch take(uint64_t now){Batch batch;if(empty())return batch;std::lock_guard<std::mutex> g(mutex);
        // Only pending packet events are visited, at most 16 per frame.
        unsigned visits=std::min<unsigned>(count.load(),batch.indices.size());
        while(visits--&&count.load()){
            if(cursor>=count.load())cursor=0;unsigned index=indices[cursor];auto& state=values[index];
            if(now-state.created>=lifetimeMs||(now>=state.next&&state.attempts>=maxAttempts)){erase(index);++expired;continue;}
            cursor=(cursor+1)%count.load();if(now<state.next)continue;
            ++state.attempts;state.next=now+retryMs;batch.indices[batch.count++]=index;
        }return batch;
    }
};
}
