#include "hood_gesture.h"
#include <cassert>
#include <cstdio>
struct Entity {
 std::array<uint32_t,10>a{0x206c6469,0x20316973,0x206b6c77,0x206e7572,0x20316963,0x206c6469,0x206c6469,0x206c6469,0x206b6c77,0x206e7572};
 uint32_t GetAnimation(unsigned,unsigned i){return a[i];}
 uint16_t GetAnimationTime(unsigned){return 0;}uint16_t GetAnimationStep(unsigned){return 823;}uint8_t GetAnimationPlay(unsigned){return 0;}
};
struct Native {
 bool accept=true,done=false;std::vector<std::pair<uintptr_t,uint32_t>> calls,stops;
 bool play(uintptr_t actor,uint32_t tag){calls.emplace_back(actor,tag);return accept;}
 bool finished(uintptr_t,uint32_t){return done;}
 void stop(uintptr_t actor,uint32_t tag){stops.emplace_back(actor,tag);}
};
int main(int argc,char**argv){
 using namespace hood::gesture;
 Package p;p.race=2;p.duration=500;p.marker=200;p.frames=16;
 Actor a{1,2,3,2,47,1,0,0,0,true};Sequence q;
 CastActivity cast;
 assert(!cast.observe(23.037f,160,100));assert(!cast.observe(23.037f,160,130));assert(!cast.observe(23.037f,160,160));
 assert(cast.observe(22.5f,160,180));assert(cast.observe(22.5f,160,180));assert(cast.observe(22.5f,160,200));
 assert(!cast.observe(22.5f,160,281));assert(!cast.observe(0,160,282));
 assert(cast.observe(160,160,300));assert(cast.observe(159,160,330));assert(!cast.observe(0,160,340));
 assert(!cast.observe(80,80,1000));assert(cast.observe(79,80,1030));assert(!cast.observe(NAN,80,1040));
 cast={};assert(!cast.observe(40,80,100));assert(cast.observe(39,80,130));assert(!cast.observe(39,80,1000));
 // A stale partial bar permits preparation; a progressing bar interrupts it.
 Preparation stalePreparation;stalePreparation.begin(a,100);cast={};cast.observe(23.037f,160,100);
 auto castActor=a;castActor.idle=!cast.observe(23.037f,160,160);assert(stalePreparation.tick(castActor,160)==PrepareEvent::Ready);
 stalePreparation.begin(a,200);cast={};cast.observe(80,80,200);castActor.idle=!cast.observe(79,80,230);assert(stalePreparation.tick(castActor,230)==PrepareEvent::Interrupted);
 MotionWatch watch;watch.observe(a,100);assert(!watch.stationary(a,159));assert(watch.stationary(a,160));
 auto moving=a;moving.x=.02f;assert(!watch.stationary(moving,161));watch.observe(moving,161);assert(!watch.stationary(moving,200));assert(watch.stationary(moving,221));
 auto rebuilt=moving;rebuilt.pointer=7;assert(!watch.stationary(rebuilt,222));watch.observe(rebuilt,222);assert(watch.stationary(rebuilt,282));assert(!watch.stationary(rebuilt,600));
 watch.observe(rebuilt,600);assert(!watch.stationary(rebuilt,601));assert(watch.stationary(rebuilt,660));
 auto invalid=rebuilt;invalid.x=NAN;watch.observe(invalid,661);assert(!watch.stationary(invalid,730));
 assert(!*idleBlock(0,0,0,false,true));assert(*idleBlock(1,0,0,false,true));assert(*idleBlock(0,1,0,false,true));assert(*idleBlock(0,0,4,false,true));assert(*idleBlock(0,0,0,true,true));assert(*idleBlock(0,0,0,false,false));
 Preparation preparation;assert(!preparation.active());preparation.begin(a,100);
 auto jitter=a;jitter.x=.004f;jitter.y=.003f;jitter.z=.002f;assert(!positionMoved(a,jitter));
 assert(preparation.tick(jitter,160)==PrepareEvent::Ready);
 preparation.begin(a,100);
 assert(preparation.tick(a,159)==PrepareEvent::Waiting);assert(preparation.tick(a,160)==PrepareEvent::Ready);assert(!preparation.active());
 preparation.begin(a,100);moving=a;moving.x=.02f;assert(preparation.tick(moving,120)==PrepareEvent::Interrupted);assert(!preparation.active());
 preparation.begin(a,100);auto busy=a;busy.idle=false;assert(preparation.tick(busy,120)==PrepareEvent::Interrupted);
 preparation.begin(a,100);auto changed=a;changed.server++;assert(preparation.tick(changed,120)==PrepareEvent::Changed);
 preparation.begin(a,100);assert(preparation.tick(a,351)==PrepareEvent::Interrupted);preparation.begin(a,100);preparation.cancel();assert(!preparation.active());
 assert(q.begin(a,100,p,false));assert(q.tick(a,299)==Event::None);assert(q.tick(a,300)==Event::Contact);assert(q.tick(a,301)==Event::None);assert(q.tick(a,600)==Event::Finished);
 assert(q.begin(a,100,p,true));assert(q.tick(a,399)==Event::None);assert(q.tick(a,400)==Event::Contact);
 a.idle=false;assert(q.tick(a,401)==Event::Cancelled);a.idle=true;
 assert(q.begin(a,500,p,false));a.pointer=2;assert(q.tick(a,510)==Event::Cancelled);
 // Override is elapsed time in either direction, captured once per gesture.
 for(bool reverse:{false,true}){int delay=123;assert(q.begin(a,100,p,reverse,delay));delay=400;assert(q.tick(a,222)==Event::None);assert(q.tick(a,223)==Event::Contact);assert(q.tick(a,224)==Event::None);q.cancel();}
 assert(q.begin(a,100,p,false,0));assert(q.tick(a,100)==Event::Contact);q.cancel();
 assert(q.begin(a,100,p,true,900));assert(q.tick(a,599)==Event::None);assert(q.tick(a,600)==Event::Contact);assert(q.tick(a,601)==Event::Finished);
 Library library;
 for(unsigned race:{1u,2u,3u,4u,5u,7u,8u}){auto entry=std::make_shared<Package>();entry->race=race;entry->duration=700;entry->marker=351;library.races[race-1]=entry;}
 assert(!library.forRace(0)&&!library.forRace(9));assert(library.forRace(5)==library.forRace(6));
 for(unsigned race=1;race<=8;++race)for(unsigned body:{3u,47u,82u,83u,84u})for(bool reverse:{false,true}){
  auto entry=library.forRace(race);assert(entry&&entry->race==(race==6?5:race));
  Actor actor{1,2,3,entry->race,body,1,0,0,0,true};Sequence change;
  assert(change.begin(actor,100,*entry,reverse,switchDelayMs));assert(change.tick(actor,329)==Event::None);assert(change.tick(actor,330)==Event::Contact);assert(change.tick(actor,331)==Event::None);
 }
 Entity e;auto table=e.a;auto inspection=describeAnimation(&e,2);
 assert(inspection.find("play=0 time=0 step=823")!=std::string::npos);
 assert(inspection.find("[0] 0x206c6469 'idl '")!=std::string::npos);
 Native native;Playback<Native> motion;
 // Both visibility targets use the same authored forward motion.
 for(bool hidden:{false,true}){assert(motion.beginChange(native,1,e.a[0]));assert(native.calls.back().second==upTag);motion.release(native,1,true);}
 // Switch at 230ms without stopping playback or resetting idle. A delayed
 // next render tick on a stationary actor must not cancel the gesture.
 Package switching=p;switching.duration=700;Sequence continuing;Actor stationary=a;stationary.idle=true;
 assert(motion.begin(native,stationary.pointer,upTag,e.a[0]));auto oldStops=native.stops.size();
 assert(continuing.begin(stationary,100,switching,false,switchDelayMs));
 assert(continuing.tick(stationary,330)==Event::Contact);assert(continuing.active()&&motion.active&&native.stops.size()==oldStops);
 assert(continuing.tick(stationary,650)==Event::None);assert(motion.active&&native.stops.size()==oldStops);
 assert(continuing.tick(stationary,800)==Event::Finished);motion.release(native,stationary.pointer,true);assert(native.stops.size()==oldStops+1);
 assert(motion.begin(native,1,upTag,e.a[0]));assert(native.calls.back().second==upTag);assert(e.a==table);
 assert(!motion.begin(native,1,downTag,e.a[0]));auto plays=native.calls.size();motion.release(native,1,true);assert(native.stops.back().second==upTag&&native.calls.size()==plays);assert(e.a==table);
 assert(motion.begin(native,1,downTag,e.a[0]));auto count=native.stops.size();motion.release(native,2,true);assert(native.stops.size()==count);
 assert(motion.begin(native,1,upTag,e.a[0]));count=native.stops.size();motion.release(native,1,false);assert(native.stops.size()==count);
 assert(motion.begin(native,1,upTag,e.a[0]));native.done=true;plays=native.calls.size();motion.release(native,1,true);assert(native.calls.size()==plays&&native.stops.back().second==upTag);
 // Finishing/cancelling the overlay never queues a new idle animation.
 for(auto&call:native.calls)assert(call.second==upTag||call.second==downTag);
 native.accept=false;assert(!motion.begin(native,1,upTag,e.a[0]));assert(!motion.active);assert(e.a==table);
 for(unsigned frames:{2u,16u,22u,61u}){
  auto routines=schedules(frames);assert(routines.size()==2*sizeof(routineTemplate));
  for(unsigned i=0;i<2;++i){auto r=routines.data()+i*sizeof(routineTemplate);uint32_t tag=0,header=0,ref=0;uint16_t duration=0,loops=0;
   memcpy(&tag,r,4);memcpy(&header,r+4,4);memcpy(&ref,r+routineCommandOffset+8,4);memcpy(&duration,r+routineCommandOffset+6,2);memcpy(&loops,r+routineCommandOffset+30,2);
   uint16_t delay=1;uint32_t totalDelay=1;memcpy(&delay,r+routineCommandOffset+4,2);memcpy(&totalDelay,r+0x2c,4);
   uint16_t blendIn=0,blendOut=0;memcpy(&blendIn,r+routineCommandOffset+24,2);memcpy(&blendOut,r+routineCommandOffset+28,2);
   assert(tag==(i?downTag:upTag)&&(header&127)==7);assert(ref==upperClip(tag)&&(ref>>24)=='3');assert(duration==2*(frames-1)&&loops==1);assert(delay==0&&totalDelay==0);assert(blendIn==gestureBlend&&blendOut==gestureBlend);
  }
 }
 if(argc>1){auto file=Package::read(argv[1]);assert(file.frames>=2&&!file.sections.empty());
  std::ifstream input(argv[1],std::ios::binary);std::vector<char> original((std::istreambuf_iterator<char>(input)),{});
  assert(file.sections.size()==original.size()-24+2*sizeof(routineTemplate));
  size_t at=0;for(unsigned i=0;i<2;++i){uint32_t head=0,tag=0;memcpy(&head,original.data()+24+at+4,4);memcpy(&tag,file.sections.data()+at,4);size_t size=((head>>7)&0x7ffffu)*16u;assert(tag==upperClip(i?downTag:upTag));assert(!memcmp(file.sections.data()+at+4,original.data()+24+at+4,size-4));at+=size;}
  if(argc>2){std::ofstream output(argv[2],std::ios::binary);output.write((char*)file.sections.data(),file.sections.size());assert(output.good());}
 }
 puts("PASS native motion dispatch fixture: occupied idle/sit/walk/run table unchanged; both tags; lookup failure; actor/movement/completion restore guards; timing, reverse marker, cancellation and package checks.");
}
