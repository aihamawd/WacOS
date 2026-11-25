#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shobjidl.h>
#include <shellapi.h>
#include <dwmapi.h>
#include <atomic>
#include <thread>
#include <mutex>
#include <string>
#include <algorithm>
#include <cmath>
#include <new>

// Shared file-operation progress. IFileOperation remains the only file-operation engine.
// No hooks, no interception of other processes, no changes to operation flags.
namespace fileprogress {
class Dialog final: public IOperationsProgressDialog {
    std::atomic<ULONG> refs{1};
    std::atomic<bool> cancel{false}, stop{false};
    std::atomic<ULONGLONG> started{0},pausedAt{0},pausedTotal{0};
    std::atomic<ULONGLONG> current{0},total{0},bytes{0},byteTotal{0},items{0},itemTotal{0};
    std::atomic<SPACTION> action{SPACTION_NONE};
    std::atomic<PDMODE> mode{PDM_DEFAULT};
    std::atomic<unsigned> callbacks{0},queries{0},starts{0};
    std::atomic<HWND> hwnd{nullptr};
    std::mutex lock;
    std::wstring destination,source;
    HICON sourceIcon=nullptr;
    std::thread ui;
    HWND owner=nullptr;
    bool dark=true;
    static constexpr const wchar_t* cls=L"WacOSFileOperationProgress";
    static void Text(HDC dc,const std::wstring& s,RECT r,COLORREF c,int size,int weight){
        HFONT f=CreateFontW(-size,0,0,0,weight,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");
        auto old=SelectObject(dc,f);SetBkMode(dc,TRANSPARENT);SetTextColor(dc,c);
        DrawTextW(dc,s.c_str(),-1,&r,DT_SINGLELINE|DT_VCENTER|DT_END_ELLIPSIS|DT_NOPREFIX);
        SelectObject(dc,old);DeleteObject(f);
    }
    static void Round(HDC dc,RECT r,COLORREF c,int d){
        HBRUSH b=CreateSolidBrush(c);auto old=SelectObject(dc,b);auto pen=SelectObject(dc,GetStockObject(NULL_PEN));
        RoundRect(dc,r.left,r.top,r.right,r.bottom,d,d);SelectObject(dc,pen);SelectObject(dc,old);DeleteObject(b);
    }
    std::wstring Verb() const{
        switch(action.load()){
            case SPACTION_COPYING:return L"Copying";
            case SPACTION_MOVING:return L"Moving";
            case SPACTION_DELETING:return L"Deleting";
            case SPACTION_RECYCLING:return L"Moving to Trash";
            case SPACTION_RENAMING:return L"Renaming";
            default:return L"Preparing";
        }
    }
    static std::wstring Size(ULONGLONG n){
        wchar_t s[64];double z=(double)n;
        if(n>=1000000000ULL)swprintf_s(s,L"%.2f GB",z/1e9);
        else if(n>=1000000)swprintf_s(s,L"%.1f MB",z/1e6);
        else if(n>=1000)swprintf_s(s,L"%.1f KB",z/1e3);
        else swprintf_s(s,L"%llu bytes",n);
        return s;
    }
    ULONGLONG Elapsed() const {
        auto begin=started.load(),end=pausedAt.load(),paused=pausedTotal.load();
        if(!end)end=GetTickCount64();
        return begin&&end>begin&&end-begin>paused?end-begin-paused:0;
    }
    double Fraction() const {
        auto t=total.load(),c=current.load();if(t)return std::clamp((double)c/t,0.,1.);
        t=byteTotal.load();c=bytes.load();if(t)return std::clamp((double)c/t,0.,1.);
        t=itemTotal.load();c=items.load();return t?std::clamp((double)c/t,0.,1.):0.;
    }
    void Paint(HWND h){
        PAINTSTRUCT ps{};HDC dc=BeginPaint(h,&ps);RECT box{};GetClientRect(h,&box);
        const int W=box.right,H=box.bottom;const float s=W/500.f;
        auto X=[&](float v){return (LONG)std::lround(v*s);};
        auto R=[&](float l,float t,float r,float b){return RECT{X(l),X(t),X(r),X(b)};};
        COLORREF bg=dark?RGB(30,30,32):RGB(244,244,247),fg=dark?RGB(242,242,244):RGB(30,30,33),muted=dark?RGB(174,174,180):RGB(104,104,111);
        HDC mem=CreateCompatibleDC(dc);HBITMAP bmp=CreateCompatibleBitmap(dc,W,H);auto ob=SelectObject(mem,bmp);
        HBRUSH brush=CreateSolidBrush(bg);FillRect(mem,&box,brush);DeleteObject(brush);
        HPEN edge=CreatePen(PS_SOLID,1,dark?RGB(86,86,90):RGB(180,180,185));
        auto edgeOld=SelectObject(mem,edge);auto empty=SelectObject(mem,GetStockObject(HOLLOW_BRUSH));
        RoundRect(mem,0,0,W,H,X(36),X(36));SelectObject(mem,empty);SelectObject(mem,edgeOld);DeleteObject(edge);
        Round(mem,R(11,11,28,28),RGB(255,95,87),X(17));
        Round(mem,R(40,11,57,28),RGB(254,188,46),X(17));
        Round(mem,R(69,11,86,28),RGB(73,73,75),X(17));
        {std::lock_guard<std::mutex> g(lock);
        if(sourceIcon)DrawIconEx(mem,X(25),X(50),sourceIcon,X(34),X(34),0,nullptr,DI_NORMAL);
        else{
            Round(mem,R(28,51,54,85),RGB(245,245,245),X(3));
            Round(mem,R(33,62,49,74),RGB(182,182,187),X(3));
        }}
        std::wstring title=Verb();auto n=itemTotal.load();
        if(n>1)title+=L" "+std::to_wstring(n)+L" items";
        {std::lock_guard<std::mutex> g(lock);if(n<=1&&!source.empty())title+=L" \u201c"+source+L"\u201d";if(!destination.empty()&&(action==SPACTION_COPYING||action==SPACTION_MOVING))title+=L" to \u201c"+destination+L"\u201d";}
        if(cancel)title=L"Cancelling\u2026";
        Text(mem,title,R(76,36,460,60),fg,X(14),FW_SEMIBOLD);
        Round(mem,R(76,64,460,74),dark?RGB(78,78,84):RGB(212,212,217),X(7));
        double f=Fraction();
        if(total||byteTotal||itemTotal){if(f>0)Round(mem,R(76,64,76+384*(float)f,74),RGB(0,122,255),X(7));}
        else {float off=(GetTickCount64()%1500)/1500.f;Round(mem,R(76+320*off,64,76+320*off+64,74),RGB(0,122,255),X(7));}
        std::wstring detail;
        if(byteTotal)detail=Size(bytes)+L" of "+Size(byteTotal);
        else if(itemTotal)detail=std::to_wstring(items)+L" of "+std::to_wstring(itemTotal)+L" items";
        else detail=L"Preparing\u2026";
        if(f>.01&&f<1&&started&&!pausedAt){
            auto sec=(ULONGLONG)(Elapsed()/1000.0*(1-f)/f);
            if(sec<86400)detail+=L" \u2014 About "+std::to_wstring(sec>=60?(sec+59)/60:sec)+(sec>=60?L" min":L" sec");
        }
        if(pausedAt)detail+=L" \u2014 Waiting\u2026";
        Text(mem,detail,R(76,77,460,101),muted,X(12),FW_NORMAL);
        Round(mem,R(471,62,485,76),dark?RGB(110,110,115):RGB(218,218,223),X(14));
        HPEN pen=CreatePen(PS_SOLID,std::max(1,(int)X(1.5f)),fg);auto op=SelectObject(mem,pen);
        MoveToEx(mem,X(475),X(66),nullptr);LineTo(mem,X(481),X(72));MoveToEx(mem,X(481),X(66),nullptr);LineTo(mem,X(475),X(72));
        SelectObject(mem,op);DeleteObject(pen);
        BitBlt(dc,0,0,W,H,mem,0,0,SRCCOPY);SelectObject(mem,ob);DeleteObject(bmp);DeleteDC(mem);EndPaint(h,&ps);
    }
    static LRESULT CALLBACK Proc(HWND h,UINT m,WPARAM w,LPARAM l){
        auto self=reinterpret_cast<Dialog*>(GetWindowLongPtrW(h,GWLP_USERDATA));
        if(m==WM_NCCREATE){self=static_cast<Dialog*>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams);SetWindowLongPtrW(h,GWLP_USERDATA,(LONG_PTR)self);}
        if(!self)return DefWindowProcW(h,m,w,l);
        if(m==WM_PAINT){self->Paint(h);return 0;}
        if(m==WM_ERASEBKGND)return 1;
        if(m==WM_CLOSE){self->cancel=true;InvalidateRect(h,nullptr,FALSE);return 0;}
        if(m==WM_LBUTTONUP){
            RECT r{};GetClientRect(h,&r);float sc=r.right/500.f;float x=(short)LOWORD(l)/sc,y=(short)HIWORD(l)/sc;
            if((x>=466&&y>=55&&y<=83)||(x>=7&&x<=32&&y>=6&&y<=32)){self->cancel=true;InvalidateRect(h,nullptr,FALSE);}else if(x>=36&&x<=61&&y>=6&&y<=32){ShowWindow(h,SW_MINIMIZE);}return 0;
        }
        if(m==WM_KEYDOWN&&w==VK_ESCAPE){self->cancel=true;return 0;}
        if(m==WM_TIMER){
            if(self->stop){DestroyWindow(h);return 0;}
            if(!IsWindowVisible(h)&&!IsIconic(h)&&GetTickCount64()-self->started.load()>350){
                DWORD foregroundPid=0;GetWindowThreadProcessId(GetForegroundWindow(),&foregroundPid);
                bool ownForeground=foregroundPid==GetCurrentProcessId();
                SetWindowPos(h,HWND_TOP,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_SHOWWINDOW|(ownForeground?0:SWP_NOACTIVATE));
                if(ownForeground)SetForegroundWindow(h);
                UpdateWindow(h);
            }
            InvalidateRect(h,nullptr,FALSE);return 0;
        }
        if(m==WM_DESTROY){PostQuitMessage(0);return 0;}
        return DefWindowProcW(h,m,w,l);
    }
    void Ui(){
        WNDCLASSW wc{};wc.lpfnWndProc=Proc;wc.hInstance=GetModuleHandleW(nullptr);wc.lpszClassName=cls;wc.hCursor=LoadCursor(nullptr,IDC_ARROW);
        if(!RegisterClassW(&wc)&&GetLastError()!=ERROR_CLASS_ALREADY_EXISTS)return;
        HMONITOR m=MonitorFromWindow(owner,MONITOR_DEFAULTTONEAREST);MONITORINFO mi{sizeof(mi)};GetMonitorInfoW(m,&mi);
        float scale=owner?GetDpiForWindow(owner)/96.f:GetDpiForSystem()/96.f;if(scale<=0)scale=1;
        int W=(int)std::lround(500*scale),H=(int)std::lround(109*scale);
        HWND h=CreateWindowExW(WS_EX_APPWINDOW,cls,L"File Operations",WS_POPUP|WS_MINIMIZEBOX,(mi.rcWork.left+mi.rcWork.right-W)/2,(mi.rcWork.top+mi.rcWork.bottom-H)/2,W,H,nullptr,nullptr,wc.hInstance,this);
        if(!h)return;
        HRGN clip=CreateRoundRectRgn(0,0,W+1,H+1,(int)(36*scale),(int)(36*scale));
        if(clip&&!SetWindowRgn(h,clip,FALSE))DeleteObject(clip);
        BOOL dk=dark;DwmSetWindowAttribute(h,20,&dk,sizeof(dk));
        hwnd=h;if(!SetTimer(h,1,50,nullptr)){DestroyWindow(h);hwnd=nullptr;return;}
        if(stop)DestroyWindow(h);
        MSG msg{};while(GetMessageW(&msg,nullptr,0,0)>0){TranslateMessage(&msg);DispatchMessageW(&msg);}
        hwnd=nullptr;
    }
public:
    explicit Dialog(bool d=true):dark(d){}
    ~Dialog(){StopProgressDialog();if(sourceIcon)DestroyIcon(sourceIcon);}
    unsigned Updates()const{return callbacks;}
    unsigned Queries()const{return queries;}
    unsigned Starts()const{return starts;}
    HWND Window()const{return hwnd;}
    void RequestCancel(){cancel=true;}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid,void** out) override{
        if(!out)return E_POINTER;*out=nullptr;
        if(riid==IID_IUnknown||riid==__uuidof(IOperationsProgressDialog)){*out=static_cast<IOperationsProgressDialog*>(this);AddRef();return S_OK;}return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef()override{return ++refs;}
    ULONG STDMETHODCALLTYPE Release()override{auto n=--refs;if(!n)delete this;return n;}
    HRESULT STDMETHODCALLTYPE StartProgressDialog(HWND h,OPPROGDLGF)override{
        if(ui.joinable())return S_OK;owner=h;started=GetTickCount64();stop=false;starts++;
        try{ui=std::thread([this]{Ui();});}catch(...){return E_FAIL;}return S_OK;
    }
    HRESULT STDMETHODCALLTYPE StopProgressDialog()override{stop=true;if(ui.joinable())ui.join();return S_OK;}
    HRESULT STDMETHODCALLTYPE SetOperation(SPACTION a)override{action=a;return S_OK;}
    HRESULT STDMETHODCALLTYPE SetMode(PDMODE m)override{mode=m;return S_OK;}
    HRESULT STDMETHODCALLTYPE UpdateProgress(ULONGLONG c,ULONGLONG t,ULONGLONG b,ULONGLONG bt,ULONGLONG i,ULONGLONG it)override{
        current=c;total=t;bytes=b;byteTotal=bt;items=i;itemTotal=it;callbacks++;return S_OK;
    }
    HRESULT STDMETHODCALLTYPE UpdateLocations(IShellItem* src,IShellItem* target,IShellItem*)override{
        if(src){PWSTR name=nullptr;if(SUCCEEDED(src->GetDisplayName(SIGDN_NORMALDISPLAY,&name))&&name){std::lock_guard<std::mutex> g(lock);source=name;CoTaskMemFree(name);
            if(!sourceIcon){PWSTR path=nullptr;if(SUCCEEDED(src->GetDisplayName(SIGDN_FILESYSPATH,&path))&&path){
                SHFILEINFOW fi{};if(SHGetFileInfoW(path,0,&fi,sizeof(fi),SHGFI_ICON|SHGFI_LARGEICON))sourceIcon=fi.hIcon;
                CoTaskMemFree(path);
            }}}}
        if(target){PWSTR p=nullptr;if(SUCCEEDED(target->GetDisplayName(SIGDN_NORMALDISPLAY,&p))&&p){std::lock_guard<std::mutex> g(lock);destination=p;CoTaskMemFree(p);}}return S_OK;
    }
    HRESULT STDMETHODCALLTYPE ResetTimer()override{started=GetTickCount64();pausedAt=0;pausedTotal=0;return S_OK;}
    HRESULT STDMETHODCALLTYPE PauseTimer()override{ULONGLONG expected=0;pausedAt.compare_exchange_strong(expected,GetTickCount64());return S_OK;}
    HRESULT STDMETHODCALLTYPE ResumeTimer()override{auto t=pausedAt.exchange(0);if(t)pausedTotal+=GetTickCount64()-t;return S_OK;}
    HRESULT STDMETHODCALLTYPE GetMilliseconds(ULONGLONG* e,ULONGLONG* r)override{
        if(!e||!r)return E_POINTER;*e=Elapsed();double f=Fraction();*r=f>.01&&f<1?(ULONGLONG)(*e*(1-f)/f):0;return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetOperationStatus(PDOPSTATUS* p)override{if(!p)return E_POINTER;queries++;*p=cancel?PDOPS_CANCELLED:stop?PDOPS_STOPPED:PDOPS_RUNNING;return S_OK;}
};
// Preserve existing operation flags and owner. Fallback to native progress if registration fails.
inline HRESULT Perform(IFileOperation* op,bool dark){
    auto* d=new(std::nothrow) Dialog(dark);
    if(!d)return op->PerformOperations();
    HRESULT attached=op->SetProgressDialog(d);
    HRESULT hr=op->PerformOperations();
    if(SUCCEEDED(attached))d->StopProgressDialog();
    d->Release();return hr;
}
}
