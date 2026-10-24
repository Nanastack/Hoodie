#pragma once
#include <Windows.h>
#include <filesystem>
namespace hood {
struct Preferences {
 std::filesystem::path file;
 bool animateChange()const{return !file.empty()&&GetPrivateProfileIntW(L"Hoodie",L"AnimateChange",0,file.c_str())!=0;}
 bool saveAnimateChange(bool enabled)const{
  if(file.empty())return true;
  std::error_code error;std::filesystem::create_directories(file.parent_path(),error);
  return !error&&WritePrivateProfileStringW(L"Hoodie",L"AnimateChange",enabled?L"1":L"0",file.c_str())!=FALSE;
 }
};
}
