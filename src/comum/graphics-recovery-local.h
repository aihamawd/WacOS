#pragma once
#include <windows.h>
#include <cstdint>
namespace localgraphics {
inline bool Lost(HRESULT hr){
    auto v=static_cast<uint32_t>(hr);
    return v==0x8899000Cu || v==0x887A0005u || v==0x887A0006u || v==0x887A0007u || v==0x887A0026u;
}
struct FaultWindow {
    uint64_t first=0,last=0;
    unsigned count=0;
    void Note(HRESULT hr,uint64_t now){
        if(!Lost(hr))return;
        if(!count||now<last||now-last>4000){first=now;count=0;}
        last=now;if(count<1000)++count;
    }
    bool Due(uint64_t now)const{return count>=3&&now>=last&&now-last<=4000&&now-first>=500;}
};
inline bool Cooldown(uint64_t previous,uint64_t now){return previous&&now>=previous&&now-previous<120000;}
void Report(HRESULT hr);
}
