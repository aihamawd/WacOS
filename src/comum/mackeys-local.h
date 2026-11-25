#pragma once
#include <windows.h>
#include <string>
#include <cwctype>
namespace localkeys {
inline UINT Message(){static UINT m=RegisterWindowMessageW(L"WacOSLocalCommandShortcut");return m;}
inline int Scope(HWND h){
    wchar_t cls[80]{};GetClassNameW(h,cls,80);
    return !wcscmp(cls,L"ArquivosJanela")?1:!wcscmp(cls,L"ShellSpotlight")?2:!wcscmp(cls,L"SequoiaMesa")?3:0;
}
inline bool Accept(int scope,UINT vk,bool shift,bool alt){
    if(scope==1){
        if(alt)return !shift&&(vk=='P'||vk=='C'||vk=='T');
        if(shift)return vk=='N'||vk=='G'||vk==VK_OEM_PERIOD;
        return (vk>='1'&&vk<='4')||vk=='A'||vk=='C'||vk=='F'||vk=='I'||vk=='J'||vk=='N'||vk=='O'||vk=='T'||vk=='V'||vk=='W'||vk=='X'||vk=='Z'||vk==VK_OEM_COMMA||vk==VK_OEM_4||vk==VK_OEM_6||vk==VK_BACK||vk==VK_UP||vk==VK_DOWN;
    }
    if(scope==2)return !shift&&!alt&&((vk>='1'&&vk<='3')||vk=='A'||vk=='C'||vk=='V'||vk=='Y'||vk==VK_RETURN||vk==VK_BACK);
    if(scope==3)return !alt&&(shift?vk=='N':vk=='A'||vk=='C'||vk=='X'||vk=='V');
    return false;
}
inline std::wstring Caption(std::wstring s,int scope){
    if(s.rfind(L"Ctrl+",0)!=0)return s;
    std::wstring key=s.substr(5);bool sh=false,al=false;
    if(key.rfind(L"Shift+",0)==0){sh=true;key.erase(0,6);}
    if(key.rfind(L"Alt+",0)==0){al=true;key.erase(0,4);}
    UINT vk=key.size()==1?(UINT)towupper(key[0]):key==L"Enter"?VK_RETURN:key==L"Backspace"?VK_BACK:0;
    if(key==L".")vk=VK_OEM_PERIOD;if(key==L",")vk=VK_OEM_COMMA;if(key==L"[")vk=VK_OEM_4;if(key==L"]")vk=VK_OEM_6;
    if(!Accept(scope,vk,sh,al))return s;
    return std::wstring(al?L"\u2325":L"")+(sh?L"\u21e7":L"")+L"\u2318"+key;
}
}
