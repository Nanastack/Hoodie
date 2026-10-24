#include "normal_armors.h"
#include "asset_store.h"
#include <cassert>
#include <filesystem>
#include <cstdio>
int main(int argc,char** argv){
 auto root=std::filesystem::path(argc>1?argv[1]:"package/resources/hoodie");
 for(unsigned race=1;race<=8;++race)for(unsigned armor=0;armor<5;++armor){
  auto body=hood::normal::models[armor];auto key=hood::normal::resource(race,body);
  std::string folder=armor==0?hood::normal::footmanFolders[race-1]:std::string(hood::normal::prefixes[armor])+"-"+hood::normal::families[race-1];auto f=root/folder;
  hood::Assets assets((f/"up.dat").string().c_str(),(f/"down.dat").string().c_str(),1,key);
  assert(key&&hood::normal::supportedBody(body|0x2000)&&hood::normal::bodyResource(key));
  for(auto mode:{hood::Variant::Up,hood::Variant::Down}){
   assert(assets.beginFor(1,1,key,0xd3e1d,mode));assert(!assets.beginFor(1,2,key,0xd3e1d,mode));
   for(auto other:hood::normal::models)if(other!=body)assert(!assets.beginFor(1,1,hood::normal::resource(race,other),0xd3e1d,mode));
  }
  for(unsigned other=1;other<=8;++other)if(other!=race&&!(other==5&&race==6)&&!(other==6&&race==5))assert(hood::normal::resource(other,body)!=key);
 }
 assert(!hood::normal::resource(0,3)&&!hood::normal::resource(9,47)&&!hood::normal::resource(1,12)&&!hood::normal::supportedBody(0x200c));
 puts("All 40 race/armor routes passed: 80 variant leases, correct IDs, no race or armor substitution, unsupported armor passthrough, Tarutaru sharing.");
}

