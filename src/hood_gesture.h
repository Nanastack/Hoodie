#pragma once
// Experimental local-only upper-arm/hand layer. Disabled until explicitly enabled.
#include "asset_store.h"
#include <array>
#include <filesystem>
#include <cmath>
#include <sstream>
#include <iomanip>
#include "gesture_routine_template.h"
namespace hood::gesture {
constexpr uint32_t fourcc(char a,char b,char c,char d){return uint8_t(a)|(uint32_t(uint8_t(b))<<8)|(uint32_t(uint8_t(c))<<16)|(uint32_t(uint8_t(d))<<24);}
inline constexpr uint32_t upTag=fourcc('h','g','u','0'),downTag=fourcc('h','g','d','0');
inline constexpr unsigned switchDelayMs=230;
// Native controller channels 0..2 carry normal lower/upper/waist idle motion.
// A separate sparse channel keeps those controllers and their clocks intact.
inline constexpr uint32_t upperClip(uint32_t action){return (action&0x00ffffffu)|(uint32_t('3')<<24);}
inline constexpr uint16_t gestureBlend=6;
inline std::vector<uint8_t> schedules(unsigned frames){
 std::vector<uint8_t> result;
 for(auto tag:{upTag,downTag}){
  std::vector<uint8_t> routine(std::begin(routineTemplate),std::end(routineTemplate));
  memcpy(routine.data(),&tag,4);
  uint16_t delay=0;uint32_t totalDelay=0;
  memcpy(routine.data()+routineCommandOffset+4,&delay,2);memcpy(routine.data()+0x2c,&totalDelay,4);
  // Native op05 scales to duration/(2*rate) frames. One pass at 30 fps.
  uint16_t duration=uint16_t(2*(frames-1));memcpy(routine.data()+routineCommandOffset+6,&duration,2);
  // Exact lookup stays in the scheduler's equipment scope. Wildcard lookup
  // switches to the actor's normal motion list, which omits these private clips.
  uint32_t clip=upperClip(tag);memcpy(routine.data()+routineCommandOffset+8,&clip,4);
  memcpy(routine.data()+routineCommandOffset+24,&gestureBlend,2);memcpy(routine.data()+routineCommandOffset+28,&gestureBlend,2);
  result.insert(result.end(),routine.begin(),routine.end());
 }
 return result;
}
struct Package {
 unsigned race=0,joints=0,duration=0,marker=0,frames=0;std::vector<uint8_t> sections;
 static Package read(const std::filesystem::path& path){
  std::ifstream f(path,std::ios::binary|std::ios::ate);if(!f)throw std::runtime_error("No gesture export for this race");auto size=f.tellg();if(size<24||size>2*1024*1024)throw std::runtime_error("Invalid gesture length");std::vector<uint8_t> raw(size_t(size),0);f.seekg(0);if(!f.read((char*)raw.data(),raw.size()))throw std::runtime_error("Incomplete gesture");
  auto u16=[&](size_t at){uint16_t v;memcpy(&v,raw.data()+at,2);return v;};auto u32=[&](size_t at){uint32_t v;memcpy(&v,raw.data()+at,4);return v;};
  if(memcmp(raw.data(),"HGA1",4)||u32(20)!=1||u32(16)!=raw.size()-24)throw std::runtime_error("Unknown gesture format");Package p;p.race=u16(4);p.joints=u16(6);p.duration=u32(8);p.marker=u32(12);if(p.race<1||p.race>8||p.joints<1||p.joints>255||p.duration<100||p.duration>2000||p.marker>p.duration)throw std::runtime_error("Invalid gesture timing or race");
  p.sections.assign(raw.begin()+24,raw.end());size_t at=0;unsigned count=0;
  while(at<p.sections.size()){
   auto data=p.sections.data()+at;if(p.sections.size()-at<32)throw std::runtime_error("Truncated gesture section");uint32_t tag=0,head=0;memcpy(&tag,data,4);memcpy(&head,data+4,4);size_t bytes=((head>>7)&0x7ffffu)*16u;uint16_t tracks=0,frames=0;memcpy(&tracks,data+18,2);memcpy(&frames,data+20,2);float rate=0;memcpy(&rate,data+22,4);
   if(count>1||tag!=(count?downTag:upTag)||(head&127)!=0x2b||bytes<32||bytes>p.sections.size()-at||tracks<1||tracks>p.joints||frames<2||frames>61||rate!=1.f||26+size_t(tracks)*84>bytes)throw std::runtime_error("Invalid native gesture layer");
   if(count&&frames!=p.frames)throw std::runtime_error("Gesture directions differ");p.frames=frames;
   for(unsigned i=0;i<tracks;++i){auto entry=data+26+i*84;uint32_t joint=0;memcpy(&joint,entry,4);if(joint>=p.joints)throw std::runtime_error("Gesture joint outside skeleton");
    for(auto group:{std::pair<unsigned,unsigned>{4,4},{36,3},{60,3}})for(unsigned k=0;k<group.second;++k){uint32_t offset=0;float value=0;memcpy(&offset,entry+group.first+k*4,4);memcpy(&value,entry+group.first+group.second*4+k*4,4);if(!std::isfinite(value)||std::abs(value)>1000)throw std::runtime_error("Invalid gesture constant");if(offset){if(offset<tracks*21||26+(uint64_t(offset)+frames)*4>bytes)throw std::runtime_error("Gesture channel outside section");for(unsigned frame=0;frame<frames;++frame){memcpy(&value,data+26+size_t(offset+frame)*4,4);if(!std::isfinite(value)||std::abs(value)>1000)throw std::runtime_error("Invalid gesture channel");}}}
   }
   // Keep external HGA1 exports compatible. Only the private section name is
   // changed to a separate sparse channel; all authored channel bytes stay exact.
   auto privateClip=upperClip(tag);memcpy(p.sections.data()+at,&privateClip,4);
   ++count;at+=bytes;
  }
  if(count!=2||std::abs(int(p.duration)-int((p.frames-1)*1000/30))>2)throw std::runtime_error("Gesture duration does not match native clip");
  // Native Play searches type07 scheduler resources, not type2B keyframes.
  // HGA files remain unchanged: wrap both clips only in the private lease.
  auto routines=schedules(p.frames);p.sections.insert(p.sections.end(),routines.begin(),routines.end());return p;
 }
 std::shared_ptr<const Lease> decorate(const std::shared_ptr<const Lease>& lease)const{
  auto bytes=lease->blob->bytes;size_t at=0;
  while(at<bytes.size()){uint32_t head=0;memcpy(&head,bytes.data()+at+4,4);if((head&127)==0x2a||!memcmp(bytes.data()+at,"end",3))break;at+=((head>>7)&0x7ffffu)*16u;}
  if(at>=bytes.size())throw std::runtime_error("No native insertion point");bytes.insert(bytes.begin()+at,sections.begin(),sections.end());return std::make_shared<const Lease>(Lease{lease->identity,std::make_shared<const Blob>(std::move(bytes))});
 }
};
struct Actor {uintptr_t pointer=0;unsigned index=0,server=0,race=0,body=0,zone=0;float x=0,y=0,z=0;bool idle=false;};
struct Library {
 std::array<std::shared_ptr<const Package>,8> races{};
 std::shared_ptr<const Package> forRace(unsigned race)const{return race>=1&&race<=8?races[race==6?4:race-1]:nullptr;}
};
inline bool same(const Actor&a,const Actor&b){return a.pointer&&a.pointer==b.pointer&&a.index==b.index&&a.server==b.server&&a.race==b.race&&a.body==b.body&&a.zone==b.zone;}
inline bool positionMoved(const Actor&a,const Actor&b){
 float x=a.x-b.x,y=a.y-b.y,z=a.z-b.z,distance=x*x+y*y+z*z;
 return !std::isfinite(distance)||distance>.01f*.01f;
}
class CastActivity {
 float previous=0;uint64_t sampled=0,progressed=0;bool tracking=false,active=false;
public:
 bool observe(float count,float maximum,uint64_t now){
  bool valid=std::isfinite(count)&&std::isfinite(maximum)&&maximum>0&&count>0&&count<=maximum+.0001f;
  if(!tracking||now-sampled>250){tracking=true;active=false;}
  else if(valid&&std::abs(count-previous)>.0001f){active=true;progressed=now;}
  previous=count;sampled=now;
  if(!valid)active=false;
  return active&&now-progressed<=100;
 }
};
class MotionWatch {
 Actor last{};uint64_t sampled=0,stableSince=0;
 static bool positionsClose(const Actor&a,const Actor&b){return !positionMoved(a,b);}
public:
 void observe(Actor actor,uint64_t now){
  if(!same(last,actor)||!positionsClose(last,actor)||now-sampled>250)stableSince=now;
  last=actor;sampled=now;
 }
 bool stationary(Actor actor,uint64_t now)const{return same(last,actor)&&positionsClose(last,actor)&&now-sampled<=250&&now-stableSince>=60;}
};
inline const char* idleBlock(unsigned status,unsigned server,unsigned event,bool casting,bool stationary){
 if(status!=0)return "character is not standing idle";
 if(server!=0)return "server status is not idle";
 if(event!=0)return "character is in an event";
 if(casting)return "casting or using an item";
 if(!stationary)return "moving or settling after movement";
 return "";
}
enum class PrepareEvent{Waiting,Ready,Interrupted,Changed};
class Preparation {
 Actor start{};uint64_t began=0;bool running=false;
public:
 void begin(Actor actor,uint64_t now){start=actor;began=now;running=true;}
 bool active()const{return running;}
 Actor identity()const{return start;}
 void cancel(){running=false;}
 PrepareEvent tick(Actor actor,uint64_t now){
  if(!same(start,actor)){running=false;return PrepareEvent::Changed;}
  if(!actor.idle||positionMoved(start,actor)||now-began>250){running=false;return PrepareEvent::Interrupted;}
  if(now-began>=60){running=false;return PrepareEvent::Ready;}
  return PrepareEvent::Waiting;
 }
};
enum class Event{None,Contact,Finished,Cancelled};
class Sequence {
 Actor start{};uint64_t began=0;unsigned duration=0,contact=0;bool running=false,fired=false;
public:
 bool begin(Actor actor,uint64_t now,const Package& p,bool reverse,int delayMs=-1){if(!actor.pointer||!actor.idle||running||actor.race!=p.race)return false;start=actor;began=now;duration=p.duration;contact=delayMs<0?(reverse?p.duration-p.marker:p.marker):std::min(unsigned(delayMs),p.duration);running=true;fired=false;return true;}
 Event tick(Actor actor,uint64_t now){if(!running)return Event::None;if(!same(start,actor)||!actor.idle){running=false;return Event::Cancelled;}auto elapsed=now-began;if(!fired&&elapsed>=contact){fired=true;return Event::Contact;}if(elapsed>=duration){running=false;return Event::Finished;}return Event::None;}
 bool active()const{return running;}void cancel(){running=false;}Actor identity()const{return start;}
};
template<class Native> struct Playback {
 uintptr_t actor=0;uint32_t tag=0,idle=0;bool active=false;
 bool beginChange(Native&native,uintptr_t identity,uint32_t idleMotion){return begin(native,identity,upTag,idleMotion);}
 bool begin(Native&native,uintptr_t identity,uint32_t motion,uint32_t idleMotion){
  if(active||!identity||!idleMotion||!native.play(identity,motion))return false;
  actor=identity;tag=motion;idle=idleMotion;active=true;return true;
 }
 void release(Native&native,uintptr_t current,bool identityValid){
  // Stop this named scheduler only. Starting idle here resets its phase and
  // causes a visible snap; movement/casting retain their own native animation.
  if(active&&actor==current&&identityValid)native.stop(actor,tag);
  active=false;actor=0;
 }
};
// These SDK entries are not proven independent mixer layers. Capture their
// actual client values before expanding eligibility beyond a zero entry.
template<class E> std::string describeAnimation(E*e,unsigned index){
 std::ostringstream out;out<<"play="<<unsigned(e->GetAnimationPlay(index))<<" time="<<e->GetAnimationTime(index)<<" step="<<e->GetAnimationStep(index);
 for(unsigned i=0;i<10;++i){uint32_t value=e->GetAnimation(index,i);out<<"\n["<<i<<"] 0x"<<std::hex<<std::setw(8)<<std::setfill('0')<<value<<std::dec<<" '";for(unsigned k=0;k<4;++k){unsigned c=(value>>(k*8))&255;out<<char(c>=32&&c<=126?c:'.');}out<<"'";}
 return out.str();
}
}
