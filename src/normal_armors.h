#pragma once
#include <cstdint>
namespace hood::normal {
// Native equipped body models; no cross-armor testing remap.
inline constexpr uint16_t footman=3,vermillion=47,cloak=10,silk=11,black=46;
inline constexpr uint32_t footmanKeys[]={7371,10547,13723,16899,20075,20075,23475,26651};
inline constexpr uint32_t vermillionKeys[]={7415,10591,13767,16943,20119,20119,23519,26695};
inline constexpr const char* footmanFolders[]={"humemale-footman","hf-footman","elvaanmale-footman","elvaanfemale-footman","taru-footman","taru-footman","mithra-footman","galka-footman"};
inline constexpr const char* vermillionFolders[]={"vermillion-cloak-humemale","vermillion-cloak-humefemale","vermillion-cloak-elvaanmale","vermillion-cloak-elvaanfemale","vermillion-cloak-tarumale","vermillion-cloak-tarumale","vermillion-cloak-mithra","vermillion-cloak-galka"};
inline constexpr const char* families[]={"humemale","humefemale","elvaanmale","elvaanfemale","tarumale","tarumale","mithra","galka"};
inline constexpr uint16_t models[]={footman,vermillion,cloak,silk,black};
inline constexpr const char* prefixes[]={"royal-footman-tunic","vermillion-cloak","cloak","silk-cloak","black-cloak"};
inline int armorIndex(uint16_t body){for(int i=0;i<5;++i)if((body&0xfff)==models[i])return i;return -1;}
inline bool supportedBody(uint16_t body){auto model=body&0xfff;return armorIndex(model)>=0;}
inline unsigned raceIndex(unsigned race){return race==6?4:race-1;}
inline uint32_t resource(unsigned race,uint16_t body){
    if(race<1||race>8)return 0;
    return supportedBody(body)?footmanKeys[race-1]+(body&0xfff)-footman:0;
}
inline bool bodyResource(uint32_t key){for(unsigned r=1;r<=8;++r)for(auto model:models)if(resource(r,model)==key)return true;return false;}
}
