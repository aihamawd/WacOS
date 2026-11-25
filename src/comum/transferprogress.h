#pragma once
#include "fileprogress.h"
#include <shlobj.h>
#include <vector>
#include <cwctype>
#include <cstdio>
// Native shell IFileOperation for ordinary filesystem drops. Unsupported data
// objects retain their original IDropTarget behavior. Never retry after execution.
namespace transferprogress {
inline void Trace(const char* reason,DWORD effect=0,size_t count=0){
    wchar_t home[32768]{};
    DWORD n=GetEnvironmentVariableW(L"LOCALAPPDATA",home,32768);
    if(!n||n>=32768)return;
    auto log=std::wstring(home)+L"\\WacOSMetrics\\transfer-route.log";
    // Bounded diagnostic file, no paths or filenames of transferred items.
    HANDLE f=CreateFileW(log.c_str(),FILE_APPEND_DATA,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(f==INVALID_HANDLE_VALUE)return;
    LARGE_INTEGER size{};if(GetFileSizeEx(f,&size)&&size.QuadPart<262144){
        char line[160];int len=sprintf_s(line,"%llu pid=%lu %s effect=%lu count=%zu\r\n",GetTickCount64(),GetCurrentProcessId(),reason,effect,count);
        DWORD written=0;if(len>0)WriteFile(f,line,(DWORD)len,&written,nullptr);
    }
    CloseHandle(f);
}
inline bool Has(IDataObject* d,CLIPFORMAT f){
    FORMATETC fe{f,nullptr,DVASPECT_CONTENT,-1,TYMED_HGLOBAL};
    return d->QueryGetData(&fe)==S_OK;
}
inline CLIPFORMAT Format(const wchar_t* f){return (CLIPFORMAT)RegisterClipboardFormatW(f);}
inline DWORD Effect(IDataObject* d){
    FORMATETC fe{Format(CFSTR_PREFERREDDROPEFFECT),nullptr,DVASPECT_CONTENT,-1,TYMED_HGLOBAL};
    STGMEDIUM m{};DWORD e=DROPEFFECT_COPY;
    if(SUCCEEDED(d->GetData(&fe,&m))){
        if(m.tymed==TYMED_HGLOBAL&&GlobalSize(m.hGlobal)>=sizeof(DWORD)){
            if(auto* p=(DWORD*)GlobalLock(m.hGlobal)){e=*p;GlobalUnlock(m.hGlobal);}
        }
        ReleaseStgMedium(&m);
    }
    return e;
}
inline void Report(IDataObject* d,const wchar_t* fmt,DWORD value){
    HGLOBAL h=GlobalAlloc(GMEM_MOVEABLE,sizeof(DWORD));if(!h)return;
    auto* p=(DWORD*)GlobalLock(h);if(!p){GlobalFree(h);return;}
    *p=value;GlobalUnlock(h);
    FORMATETC fe{Format(fmt),nullptr,DVASPECT_CONTENT,-1,TYMED_HGLOBAL};
    STGMEDIUM m{};m.tymed=TYMED_HGLOBAL;m.hGlobal=h;
    if(FAILED(d->SetData(&fe,&m,TRUE)))GlobalFree(h);
}
inline bool Local(const std::wstring& p,bool directory=false){
    if(p.size()<3||p[1]!=L':'||p[2]!=L'\\'||GetDriveTypeW(p.substr(0,3).c_str())!=DRIVE_FIXED)return false;
    DWORD a=GetFileAttributesW(p.c_str());
    return a!=INVALID_FILE_ATTRIBUTES&&!(a&(FILE_ATTRIBUTE_REPARSE_POINT|FILE_ATTRIBUTE_OFFLINE))&&(!directory||(a&FILE_ATTRIBUTE_DIRECTORY));
}
inline bool Paths(IDataObject* d,std::vector<std::wstring>& out){
    if(!Has(d,CF_HDROP)||!Has(d,Format(CFSTR_SHELLIDLIST))||
       Has(d,Format(CFSTR_FILEDESCRIPTORW))||Has(d,Format(CFSTR_FILEDESCRIPTORA))||
       Has(d,Format(CFSTR_UNTRUSTEDDRAGDROP)))return false;
    FORMATETC fe{CF_HDROP,nullptr,DVASPECT_CONTENT,-1,TYMED_HGLOBAL};STGMEDIUM m{};
    if(FAILED(d->GetData(&fe,&m)))return false;
    bool ok=false;
    if(m.tymed==TYMED_HGLOBAL){
        // DragQueryFile expects the global handle, not a locked pointer.
        HDROP drop=(HDROP)m.hGlobal;
        UINT n=DragQueryFileW(drop,0xFFFFFFFF,nullptr,0);
        ok=n>0;
        for(UINT i=0;i<n&&ok;i++){
            UINT len=DragQueryFileW(drop,i,nullptr,0);
            if(!len||len>32760){ok=false;break;}
            std::wstring p(len+1,L'\0');
            if(DragQueryFileW(drop,i,p.data(),len+1)!=len){ok=false;break;}
            p.resize(len);
            if(!Local(p)){ok=false;break;}out.push_back(std::move(p));
        }
    }
    ReleaseStgMedium(&m);return ok;
}
struct Result {bool handled=false;HRESULT hr=E_FAIL;DWORD effect=DROPEFFECT_NONE;};
inline Result Transfer(HWND owner,IDataObject* d,const std::wstring& folder,DWORD effect,bool paste,bool dark){
    effect&=~DROPEFFECT_SCROLL;
    if(!d||(effect!=DROPEFFECT_COPY&&effect!=DROPEFFECT_MOVE)){Trace("fallback-effect",effect);return {};}
    if(!Local(folder,true)){Trace("fallback-destination",effect);return {};}
    std::vector<std::wstring> paths;if(!Paths(d,paths)){Trace("fallback-source-format-or-path",effect);return {};}
    // Keep same-folder duplicate/move, ancestor recursion and special path semantics native.
    auto key=[](std::wstring s){while(s.size()>3&&s.back()==L'\\')s.pop_back();for(auto& c:s)c=towlower(c);return s;};
    auto dest=key(folder);
    for(auto& p:paths){
        auto src=key(p);auto slash=src.find_last_of(L'\\');
        if(slash==std::wstring::npos||dest==src||dest==key(src.substr(0,slash==2?3:slash))||
           (dest.size()>src.size()&&dest.compare(0,src.size(),src)==0&&dest[src.size()]==L'\\')){Trace("fallback-same-or-descendant",effect);return {};}
    }
    IFileOperation* op=nullptr;IShellItem* dst=nullptr;
    HRESULT hr=CoCreateInstance(CLSID_FileOperation,nullptr,CLSCTX_ALL,IID_PPV_ARGS(&op));
    if(FAILED(hr)){Trace("fallback-engine",effect);return {};}
    hr=op->SetOwnerWindow(owner);
    if(SUCCEEDED(hr))hr=op->SetOperationFlags(FOF_ALLOWUNDO|FOFX_ADDUNDORECORD|FOF_WANTNUKEWARNING);
    if(SUCCEEDED(hr))hr=SHCreateItemFromParsingName(folder.c_str(),nullptr,IID_PPV_ARGS(&dst));
    for(auto& p:paths){
        if(FAILED(hr))break;
        IShellItem* src=nullptr;
        hr=SHCreateItemFromParsingName(p.c_str(),nullptr,IID_PPV_ARGS(&src));
        if(SUCCEEDED(hr))hr=effect==DROPEFFECT_MOVE?op->MoveItem(src,dst,nullptr,nullptr):op->CopyItem(src,dst,nullptr,nullptr);
        if(src)src->Release();
    }
    if(dst)dst->Release();
    if(FAILED(hr)){op->Release();Trace("fallback-queue",effect);return {};}
    // Once started, including partial/cancelled operations, NEVER invoke the native drop again.
    Trace("custom-start",effect,paths.size());
    hr=fileprogress::Perform(op,dark);
    BOOL aborted=FALSE;HRESULT check=op->GetAnyOperationsAborted(&aborted);op->Release();
    bool success=SUCCEEDED(hr)&&SUCCEEDED(check)&&!aborted;
    Trace(success?"custom-complete":"custom-incomplete",effect,paths.size());
    // Optimized move: the target has already moved files. Return NONE to prevent
    // the source deleting originals again, including partial/cancelled moves.
    Report(d,CFSTR_PERFORMEDDROPEFFECT,success&&effect==DROPEFFECT_COPY?DROPEFFECT_COPY:DROPEFFECT_NONE);
    if(success){
        Report(d,CFSTR_LOGICALPERFORMEDDROPEFFECT,effect);
        if(paste&&effect==DROPEFFECT_MOVE)Report(d,CFSTR_PASTESUCCEEDED,DROPEFFECT_MOVE);
    }
    return {true,success?S_OK:FAILED(hr)?hr:HRESULT_FROM_WIN32(ERROR_CANCELLED),
        (DWORD)(success&&effect==DROPEFFECT_COPY?DROPEFFECT_COPY:DROPEFFECT_NONE)};
}
inline Result Paste(HWND owner,const std::wstring& folder,bool dark){
    IDataObject* d=nullptr;if(FAILED(OleGetClipboard(&d)))return {};
    auto e=Effect(d);auto result=Transfer(owner,d,folder,e,true,dark);d->Release();return result;
}
}
