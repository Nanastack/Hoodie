#include <Windows.h>
#include "Ashita.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include "routing.h"
int main(){
 assert(displayHeadCommand(" /displayhead   off ")==0);
 assert(displayHeadCommand("/DISPLAYHEAD on")==1);
 assert(displayHeadCommand("/displayhead")==2);
 assert(displayHeadCommand("  /DISPLAYHEAD \t ")==2);
 assert(displayHeadCommand("/displayhead off extra")==-1);
 assert(displayHeadCommand(std::string("/displayhead ")+"\xEF\x27"+"off"+"\xEF\x28")==0);
 assert(footmanPath("Z:\\Phoenix XI\\SquareEnix\\FINAL FANTASY XI\\ROM\\32\\114.DAT"));
 assert(footmanPath("ROM/32/114.DAT"));
 assert(footmanPath("Z:\\a\\ROM\\\\32\\114.DAT"));
 assert(!footmanPath("ROM10/250/0.DAT"));
 assert(!footmanPath("BROM/32/114.DAT"));
 assert(!footmanPath("ROM/32/114.DAT.bak"));
 assert(!footmanPath("ROM/32/115.DAT"));
 assert(!footmanPath("resources/hoodie/hf-footman/down.dat"));
 auto h=LoadLibraryA("package/plugins/hoodie.dll");assert(h);
 auto ver=(double(__stdcall*)())GetProcAddress(h,"expGetInterfaceVersion");assert(ver&&ver()==4.30);
 auto create=(void*(__stdcall*)(const char*))GetProcAddress(h,"expCreatePlugin");
 auto destroy=(void(__stdcall*)(void*))GetProcAddress(h,"expDestroyPlugin");assert(create&&destroy);
 auto p=create("");assert(p);assert(std::strcmp(static_cast<IPlugin*>(p)->GetName(),"Hoodie")==0);assert(std::strcmp(static_cast<IPlugin*>(p)->GetAuthor(),"Zarianna")==0);assert(static_cast<IPlugin*>(p)->Direct3DInitialize(nullptr));assert(static_cast<IPlugin*>(p)->GetVersion()==1.0);assert((static_cast<IPlugin*>(p)->GetFlags() & (uint32_t)Ashita::PluginFlags::UseDirect3D)!=0);assert(!static_cast<IPlugin*>(p)->HandleCommand(1,"/displayhead off",false));assert(static_cast<IPlugin*>(p)->HandleCommand(1,"/hoodie",true));assert(static_cast<IPlugin*>(p)->HandleCommand(1,"/hoodie",false));assert(!static_cast<IPlugin*>(p)->HandleCommand(1,"/displayhead off",true));destroy(p);FreeLibrary(h);
 puts("Passed 8 path-routing cases and x86 DLL load/export/interface/instance checks.");
}





