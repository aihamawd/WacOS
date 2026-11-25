#pragma once
#include <windows.h>
#include <shellapi.h>
#include <string>
#include <cwctype>

namespace localvolumes {
inline bool IsRoot(const std::wstring& p){
    return (p.size()==2||p.size()==3)&&p[1]==L':'&&
        ((p[0]>=L'A'&&p[0]<=L'Z')||(p[0]>=L'a'&&p[0]<=L'z'))&&
        (p.size()==2||p[2]==L'\\'||p[2]==L'/');
}
inline std::wstring Display(const std::wstring& path){
    if(!IsRoot(path))return {};
    wchar_t letter=(wchar_t)towupper(path[0]);
    std::wstring root=std::wstring(1,letter)+L":\\";
    wchar_t win[MAX_PATH]{};
    if(GetWindowsDirectoryW(win,MAX_PATH)&&towupper(win[0])==letter&&win[1]==L':')return L"Macintosh HD";
    UINT type=GetDriveTypeW(root.c_str());
    wchar_t label[256]{};
    if(type==DRIVE_FIXED&&GetVolumeInformationW(root.c_str(),label,256,nullptr,nullptr,nullptr,nullptr,0)&&label[0])return label;
    SHFILEINFOW sfi{};
    std::wstring name;
    if(SHGetFileInfoW(root.c_str(),0,&sfi,sizeof(sfi),SHGFI_DISPLAYNAME))name=sfi.szDisplayName;
    // Remove only the exact drive-letter suffix, never modify a filesystem path.
    std::wstring suffix=L" ("+std::wstring(1,letter)+L":)";
    if(name.size()>=suffix.size()&&!_wcsicmp(name.c_str()+name.size()-suffix.size(),suffix.c_str()))
        name.resize(name.size()-suffix.size());
    if(!name.empty()&&!IsRoot(name)&&_wcsicmp(name.c_str(),suffix.c_str()+1))return name;
    return type==DRIVE_REMOTE?L"Network Drive":type==DRIVE_REMOVABLE?L"Removable Disk":type==DRIVE_CDROM?L"Optical Disc":L"Untitled";
}
}
