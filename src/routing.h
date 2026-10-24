#pragma once
#include <string>
#include <algorithm>
#include <cctype>
inline std::string normalize(std::string p) {
    std::transform(p.begin(),p.end(),p.begin(),[](unsigned char c){return c=='\\'?'/':static_cast<char>(std::tolower(c));});
    std::string r; for(char c:p) if(c!='/' || r.empty() || r.back()!='/') r+=c;
    return r;
}
inline bool footmanPath(const std::string& p) {
    auto n=normalize(p); const std::string suffix="rom/32/114.dat";
    if(n==suffix) return true;
    return n.size()>suffix.size() && n.compare(n.size()-suffix.size(),suffix.size(),suffix)==0 && n[n.size()-suffix.size()-1]=='/';
}
// Returns -1 for unrelated commands, 0 for off, 1 for on, 2 for toggle.
inline int displayHeadCommand(std::string command) {
    command=normalize(command);
    for(auto tag:{std::string("\xEF\x27"),std::string("\xEF\x28")}) {
        size_t at;while((at=command.find(tag))!=std::string::npos)command.erase(at,tag.size());
    }
    std::string compact;bool space=false;
    for(unsigned char c:command){if(std::isspace(c)){space=!compact.empty();continue;}if(space){compact+=' ';space=false;}compact+=char(c);}
    if(compact=="/displayhead off")return 0;
    if(compact=="/displayhead on")return 1;
    if(compact=="/displayhead")return 2;
    return -1;
}
