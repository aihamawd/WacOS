#pragma once
// WacOS menu-bar data extensions v1: untrusted text, never load or execute plugin code.
#include <filesystem>
#include <fstream>
#include <sstream>
#include <chrono>
namespace barplugins {
namespace fs = std::filesystem;
struct Row { std::wstring label, value, action; bool checked = false; std::wstring group; int kind = 0; };
struct Item { std::wstring id, top, bottom; fs::path source; std::vector<Row> rows; bool dual = false; float minWidth = 30; bool afterStats = false, urgent = false; };
struct Snapshot { std::vector<Item> items; std::wstring signature; };
inline std::mutex actionMx;
inline std::vector<std::pair<fs::path,std::wstring>> actions;
inline fs::path Root() {
    wchar_t p[32768]; DWORD n=GetEnvironmentVariableW(L"APPDATA",p,32768);
    return n && n<32768 ? fs::path(p)/L"Shell"/L"plugins" : fs::path();
}
inline bool Token(const std::wstring& s) {
    if(s.empty() || s.size()>64) return false;
    for(auto c:s) if(!((c>=L'a'&&c<=L'z')||(c>=L'A'&&c<=L'Z')||(c>=L'0'&&c<=L'9')||c==L'_'||c==L'-')) return false;
    return true;
}
inline std::wstring Decode(const std::string& s) {
    int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s.data(),(int)s.size(),nullptr,0);
    if(n<=0) return {};
    std::wstring w(n,L'\0'); MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s.data(),(int)s.size(),w.data(),n); return w;
}
inline std::string Encode(const std::wstring& w) {
    int n=WideCharToMultiByte(CP_UTF8,0,w.data(),(int)w.size(),nullptr,0,nullptr,nullptr);
    if(n<=0) return {}; std::string s(n,'\0'); WideCharToMultiByte(CP_UTF8,0,w.data(),(int)w.size(),s.data(),n,nullptr,nullptr); return s;
}
inline void Select(const Item& item,const std::wstring& action) {
    if(!Token(action)) return; std::lock_guard<std::mutex> l(actionMx);
    if(actions.size()<32) actions.emplace_back(item.source,action);
}
inline Snapshot Read() {
    Snapshot out;
    try {
        auto root=Root(); if(root.empty()) return out;
        std::error_code ec; if(!fs::is_directory(root,ec)) return out;
        std::vector<std::pair<fs::path,std::wstring>> pending;
        { std::lock_guard<std::mutex> l(actionMx); pending.swap(actions); }
        for(auto& a:pending) {
            auto p=a.first; p.replace_extension(L".action");
            auto tmp=p; tmp+=L".tmp";
            { std::ofstream f(tmp,std::ios::binary|std::ios::trunc); f<<Encode(a.second)<<"\n"; }
            MoveFileExW(tmp.c_str(),p.c_str(),MOVEFILE_REPLACE_EXISTING);
        }
        std::vector<fs::path> paths;
        for(auto& e:fs::directory_iterator(root,ec)) {
            if(paths.size()>=32) break;
            if(e.path().extension()==L".wmp" && e.is_regular_file(ec) && !e.is_symlink(ec)) paths.push_back(e.path());
        }
        std::sort(paths.begin(),paths.end());
        for(auto& p:paths) {
            auto age=fs::file_time_type::clock::now()-fs::last_write_time(p,ec);
            if(ec || age>std::chrono::seconds(180) || fs::file_size(p,ec)>65536 || ec) continue;
            std::ifstream f(p,std::ios::binary); std::string line;
            if(!std::getline(f,line)) continue;
            if(!line.empty()&&line.back()=='\r') line.pop_back();
            if(line!="WMP1") continue;
            std::vector<Item> local;
            for(int count=0;count<256&&std::getline(f,line);++count) {
                if(!line.empty()&&line.back()=='\r') line.pop_back();
                if(line.size()>2048) continue;
                std::vector<std::wstring> v; std::stringstream ss(line); std::string a;
                while(std::getline(ss,a,'\t')) v.push_back(Decode(a));
                if(v.size()<4 || !Token(v[1])) continue;
                if(v[0]==L"item"&&local.size()<12&&v[2].size()<=48&&v[3].size()<=48) {
                    if(std::none_of(local.begin(),local.end(),[&](auto& i){return i.id==v[1];}))
                        { float minW=30; if(v.size()>5) { try { minW=std::clamp(std::stof(v[5]),30.f,130.f); } catch(...) {} } local.push_back({v[1],v[2],v[3],p,{},v.size()>4&&v[4]==L"dual",minW,v.size()>6&&v[6]==L"after-stats",v.size()>7&&v[7]==L"urgent"}); }
                } else if(v[0]==L"row"||v[0]==L"action"||v[0]==L"heading"||v[0]==L"separator") {
                    auto i=std::find_if(local.begin(),local.end(),[&](auto& i){return i.id==v[1];});
                    if(i==local.end()||i->rows.size()>=96||v[2].size()>120||v[3].size()>120) continue;
                    bool action=v[0]==L"action"; if(action&&!Token(v[3])) continue;
                    i->rows.push_back({v[2],action?L"":v[3],action?v[3]:L"",v.size()>4&&v[4]==L"1",v.size()>5&&v[5].size()<=48?v[5]:L"",v[0]==L"heading"?1:v[0]==L"separator"?2:0});
                }
            }
            for(auto& i:local) {
                if(out.items.size()>=24) break;
                out.signature+=p.wstring()+L"|"+i.id+L"|"+i.top+L"|"+i.bottom+std::to_wstring(i.dual)+std::to_wstring(i.minWidth)+std::to_wstring(i.afterStats)+std::to_wstring(i.urgent);
                for(auto& r:i.rows) out.signature+=L"|"+r.label+L"|"+r.value+L"|"+r.action+(r.checked?L"1":L"0")+L"|"+r.group+std::to_wstring(r.kind);
                out.items.push_back(std::move(i));
            }
        }
    } catch(...) {} // an extension cannot interrupt the shell
    return out;
}
}
