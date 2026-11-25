// Fotos de várias janelas de uma vez pela captura do Windows (Windows.Graphics.Capture): tudo na placa de
// vídeo, sem passar pela memória do Shell. Cada janela manda quadros (o primeiro em ~1 atualização da
// tela, todas ao mesmo tempo), copiados direto para superfícies do compositor. Sem a borda amarela de
// captura e sem o cursor. A que falhar volta vazia (quem chamou usa a foto por CPU, foto.cpp).
// Abrir uma captura custa ~8 ms (conversa com o DWM): Start abre todas em paralelo e as deixa abertas,
// guardando o último quadro de cada janela; Grab tira as fotos na hora (~10 ms); Stop fecha.
#include "shell.h"
#include <d3d11.h>
#include <dxgi.h>
#include <dcomp.h>
#include <dwmapi.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Graphics.Capture.h>
#include <winrt/Windows.Graphics.DirectX.h>
#include <winrt/Windows.Graphics.DirectX.Direct3D11.h>
#include <windows.graphics.capture.interop.h>
#include <windows.graphics.directx.direct3d11.interop.h>
#include <algorithm>
#include <memory>
#include <mutex>
#include <thread>

#pragma comment(lib, "windowsapp.lib")

namespace wgc = winrt::Windows::Graphics::Capture;
namespace wd3 = winrt::Windows::Graphics::DirectX::Direct3D11;

namespace {
wd3::IDirect3DDevice g_dev{ nullptr };

struct Cap {
    HWND h = nullptr;
    RECT src{};
    wgc::GraphicsCaptureItem item{ nullptr };
    wgc::Direct3D11CaptureFramePool pool{ nullptr };
    wgc::GraphicsCaptureSession session{ nullptr };
    std::mutex m;
    wgc::Direct3D11CaptureFrame frame{ nullptr };  // o último que chegou
    HANDLE ev = nullptr;                            // sinal: chegou o primeiro
    bool failed = false;
};
}  // namespace

struct foto::Live { std::vector<std::shared_ptr<Cap>> caps; };

static foto::Live* StartDevice(const std::vector<HWND>& hs, const std::vector<RECT>& srcs, ID3D11Device* d3d, wd3::IDirect3DDevice& captureDevice) {
    if (!d3d || hs.empty()) return nullptr;
    try {
        if (!wgc::GraphicsCaptureSession::IsSupported()) return nullptr;
        if (!captureDevice) {
            winrt::com_ptr<IDXGIDevice> dx;
            d3d->QueryInterface(IID_PPV_ARGS(dx.put()));
            winrt::com_ptr<::IInspectable> insp;
            winrt::check_hresult(CreateDirect3D11DeviceFromDXGIDevice(dx.get(), insp.put()));
            captureDevice = insp.as<wd3::IDirect3DDevice>();
        }
        auto interop = winrt::get_activation_factory<wgc::GraphicsCaptureItem, IGraphicsCaptureItemInterop>();
        auto* live = new foto::Live;
        size_t n = std::min<size_t>(hs.size(), 60);
        live->caps.resize(n);
        std::vector<std::thread> ths;  // todas ao mesmo tempo, uma thread por janela
        for (size_t i = 0; i < n; i++) {
            live->caps[i] = std::make_shared<Cap>();
            std::shared_ptr<Cap> c = live->caps[i];
            c->h = hs[i]; c->src = srcs[i];
            c->ev = CreateEventW(nullptr, TRUE, FALSE, nullptr);
            ths.emplace_back([c, interop, captureDevice] {
                winrt::init_apartment(winrt::apartment_type::multi_threaded);
                try {
                    winrt::check_hresult(interop->CreateForWindow(c->h, winrt::guid_of<wgc::GraphicsCaptureItem>(), winrt::put_abi(c->item)));
                    c->pool = wgc::Direct3D11CaptureFramePool::CreateFreeThreaded(captureDevice, winrt::Windows::Graphics::DirectX::DirectXPixelFormat::B8G8R8A8UIntNormalized, 2, c->item.Size());
                    c->session = c->pool.CreateCaptureSession(c->item);
                    try { c->session.IsCursorCaptureEnabled(false); } catch (...) {}
                    try { c->session.IsBorderRequired(false); } catch (...) {}
                    std::weak_ptr<Cap> wk = c;  // um quadro atrasado depois do Stop não encontra nada
                    c->pool.FrameArrived([wk](wgc::Direct3D11CaptureFramePool const& p, auto&&) {
                        auto c = wk.lock();
                        if (!c) return;
                        auto f = p.TryGetNextFrame();
                        if (!f) return;
                        wgc::Direct3D11CaptureFrame old{ nullptr };
                        { std::lock_guard<std::mutex> lk(c->m); old = c->frame; c->frame = f; }
                        if (old) old.Close();  // devolve o buffer: o próximo quadro pode chegar
                        SetEvent(c->ev);
                    });
                    c->session.StartCapture();
                } catch (...) {
                    c->failed = true;
                    SetEvent(c->ev);
                }
                winrt::uninit_apartment();
            });
        }
        for (auto& t : ths) t.join();
        return live;
    } catch (...) {
        Log(L"foto (GPU): captura indisponível");
        return nullptr;
    }
}

foto::Live* foto::Start(const std::vector<HWND>& hs,const std::vector<RECT>& srcs){
    if(!ui::Comp())return nullptr;
    return StartDevice(hs,srcs,ui::D3D(),g_dev);
}

std::vector<foto::Gpu> foto::Grab(Live* live, int timeoutMs) {
    std::vector<Gpu> out(live ? live->caps.size() : 0);
    if (!live) return out;
    ID3D11Device* d3d = ui::D3D();
    IDCompositionDevice* cd = ui::Comp();
    std::vector<HANDLE> evs;
    for (auto& c : live->caps) evs.push_back(c->ev);
    if (!evs.empty()) WaitForMultipleObjects((DWORD)evs.size(), evs.data(), TRUE, (DWORD)timeoutMs);
    winrt::com_ptr<ID3D11DeviceContext> ctx;
    d3d->GetImmediateContext(ctx.put());
    for (size_t i = 0; i < live->caps.size(); i++) {
        Cap* c = live->caps[i].get();
        if (c->failed) continue;
        try {
            std::lock_guard<std::mutex> lk(c->m);  // o quadro não é trocado enquanto copia
            if (!c->frame) continue;
            auto access = c->frame.Surface().as<::Windows::Graphics::DirectX::Direct3D11::IDirect3DDxgiInterfaceAccess>();
            winrt::com_ptr<ID3D11Texture2D> tex;
            winrt::check_hresult(access->GetInterface(IID_PPV_ARGS(tex.put())));
            D3D11_TEXTURE2D_DESC d{}; tex->GetDesc(&d);
            auto cs = c->frame.ContentSize();
            // o quadro é a janela inteira (com a borda invisível), como a miniatura do DWM: o mesmo recorte
            RECT wr; GetWindowRect(c->h, &wr);
            RECT s = c->src;
            int fw = std::min<int>(cs.Width, (int)d.Width), fh = std::min<int>(cs.Height, (int)d.Height);
            if (fw != wr.right - wr.left || fh != wr.bottom - wr.top) {  // quadro só da parte visível
                RECT efb = wr; DwmGetWindowAttribute(c->h, DWMWA_EXTENDED_FRAME_BOUNDS, &efb, sizeof(efb));
                OffsetRect(&s, -(efb.left - wr.left), -(efb.top - wr.top));
            }
            s.left = std::clamp<LONG>(s.left, 0, fw); s.right = std::clamp<LONG>(s.right, 0, fw);
            s.top = std::clamp<LONG>(s.top, 0, fh); s.bottom = std::clamp<LONG>(s.bottom, 0, fh);
            UINT w = (UINT)(s.right - s.left), h = (UINT)(s.bottom - s.top);
            IDCompositionSurface* surf = nullptr;
            if (w < 4 || h < 4 || FAILED(cd->CreateSurface(w, h, DXGI_FORMAT_B8G8R8A8_UNORM, DXGI_ALPHA_MODE_PREMULTIPLIED, &surf))) continue;
            ID3D11Texture2D* dst = nullptr; POINT off{};
            if (SUCCEEDED(surf->BeginDraw(nullptr, IID_PPV_ARGS(&dst), &off))) {
                D3D11_BOX box{ (UINT)s.left, (UINT)s.top, 0, (UINT)s.right, (UINT)s.bottom, 1 };
                ctx->CopySubresourceRegion(dst, 0, off.x, off.y, 0, tex.get(), 0, &box);
                dst->Release();
                surf->EndDraw();
                out[i] = { surf, w, h };
            } else surf->Release();
        } catch (...) {}
    }
    return out;
}

void foto::Stop(Live* live) {
    if (!live) return;
    for (auto& c : live->caps) {
        try {
            if (c->session) c->session.Close();
            if (c->pool) c->pool.Close();   // fora da trava: um quadro chegando termina antes
            std::lock_guard<std::mutex> lk(c->m);
            if (c->frame) { c->frame.Close(); c->frame = nullptr; }
        } catch (...) {}
        if (c->ev) CloseHandle(c->ev);
    }
    delete live;
}

namespace {
bool StageContent(const std::vector<BYTE>& px,UINT w,UINT h){
    // Ignore the frame/title edges: a black capture with a non-black border
    // is not a valid image. Dark terminal text and dark UI are still valid.
    if(w<4||h<4||px.size()!=(size_t)w*h*4)return false;
    unsigned lo=255,hi=0;size_t n=0,lit=0;
    for(UINT y=h/6;y<h-h/12;y++)for(UINT x=w/12;x<w-w/12;x++){
        const BYTE* p=px.data()+((size_t)y*w+x)*4;
        unsigned v=std::max({p[0],p[1],p[2]});lo=std::min(lo,v);hi=std::max(hi,v);++n;
        if(v>24)++lit;
    }
    return n && ((hi-lo>=16 && lit>=std::max<size_t>(2,n/500)) || lo>32);
}
bool StageLegacy(HWND hwnd,const RECT& src,std::vector<BYTE>& pixels,UINT& w,UINT& h){
    RECT wr{};if(!GetWindowRect(hwnd,&wr))return false;
    LONG fullW=wr.right-wr.left,fullH=wr.bottom-wr.top;
    if(fullW<4||fullH<4||fullW>4096||fullH>4096)return false;
    RECT s{std::clamp<LONG>(src.left,0,fullW),std::clamp<LONG>(src.top,0,fullH),
           std::clamp<LONG>(src.right,0,fullW),std::clamp<LONG>(src.bottom,0,fullH)};
    LONG sw=s.right-s.left,sh=s.bottom-s.top;if(sw<4||sh<4)return false;
    HDC screen=GetDC(nullptr);if(!screen)return false;
    HDC mem=CreateCompatibleDC(screen);if(!mem){ReleaseDC(nullptr,screen);return false;}
    BITMAPINFO bi{};bi.bmiHeader={sizeof(BITMAPINFOHEADER),fullW,-fullH,1,32,BI_RGB};
    void* bits=nullptr;HBITMAP bm=CreateDIBSection(screen,&bi,DIB_RGB_COLORS,&bits,nullptr,0);
    bool ok=false;
    if(bm && bits){
        HGDIOBJ old=SelectObject(mem,bm);
        UINT dw=std::min<UINT>(sw,384),dh=std::max(1u,(UINT)((uint64_t)sh*dw/sw));
        if(dh>384){dw=std::max(1u,(UINT)((uint64_t)dw*384/dh));dh=384;}
        pixels.resize((size_t)dw*dh*4);
        auto sample=[&](){
            GdiFlush();
            for(UINT y=0;y<dh;y++)for(UINT x=0;x<dw;x++){
                const BYTE* a=(const BYTE*)bits+((size_t)(s.top+y*sh/dh)*fullW+s.left+x*sw/dw)*4;
                BYTE* b=pixels.data()+((size_t)y*dw+x)*4;
                b[0]=a[0];b[1]=a[1];b[2]=a[2];b[3]=255;
            }
            return StageContent(pixels,dw,dh);
        };
        // WM_PRINT is bounded, unlike PrintWindow on an arbitrary process.
        // It asks the application to draw its own window, not its neighbours.
        ZeroMemory(bits,(size_t)fullW*fullH*4);
        DWORD_PTR reply=0;
        if(SendMessageTimeoutW(hwnd,WM_PRINT,(WPARAM)mem,PRF_NONCLIENT|PRF_CLIENT|PRF_ERASEBKGND|PRF_CHILDREN,
                               SMTO_ABORTIFHUNG|SMTO_BLOCK,100,&reply))ok=sample();
        if(!ok){
            // For classic console/GDI windows, read the window DC. Do not
            // sample the desktop or another window's pixels as a fallback.
            wchar_t cls[80]{};GetClassNameW(hwnd,cls,80);
            if(!wcscmp(cls,L"ConsoleWindowClass")){
                HDC wnd=GetWindowDC(hwnd);
                if(wnd){
                    ZeroMemory(bits,(size_t)fullW*fullH*4);
                    if(BitBlt(mem,0,0,fullW,fullH,wnd,0,0,SRCCOPY))ok=sample();
                    ReleaseDC(hwnd,wnd);
                }
            }
        }
        if(ok){w=dw;h=dh;Log(L"organizador: GDI preview %p",hwnd);}
        SelectObject(mem,old);DeleteObject(bm);
    }
    DeleteDC(mem);ReleaseDC(nullptr,screen);
    if(!ok){pixels.clear();w=h=0;}
    return ok;
}
}
bool foto::StagePixels(HWND hwnd, const RECT& src, std::vector<BYTE>& pixels, UINT& w, UINT& hh) {
    w=hh=0; pixels.clear();
    // Dedicated capture device: this function runs on the Stage worker.
    // Never use ui::D3D() or the shell's immediate context from this thread.
    winrt::com_ptr<ID3D11Device> device;
    D3D_FEATURE_LEVEL level{};
    if(FAILED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,D3D11_CREATE_DEVICE_BGRA_SUPPORT,
       nullptr,0,D3D11_SDK_VERSION,device.put(),&level,nullptr)))
        return StageLegacy(hwnd,src,pixels,w,hh);
    wd3::IDirect3DDevice captureDevice{nullptr};
    Live* live=StartDevice({hwnd},{src},device.get(),captureDevice);
    if(!live)return StageLegacy(hwnd,src,pixels,w,hh);
    bool ok=false;
    try {
        auto c=live->caps.front();
        if(WaitForSingleObject(c->ev,180)==WAIT_OBJECT_0 && !c->failed){
            std::lock_guard<std::mutex> lk(c->m);
            if(c->frame){
                auto access=c->frame.Surface().as<::Windows::Graphics::DirectX::Direct3D11::IDirect3DDxgiInterfaceAccess>();
                winrt::com_ptr<ID3D11Texture2D> tex;
                winrt::check_hresult(access->GetInterface(IID_PPV_ARGS(tex.put())));
                D3D11_TEXTURE2D_DESC d{};tex->GetDesc(&d);
                auto cs=c->frame.ContentSize();
                RECT wr{};GetWindowRect(hwnd,&wr);RECT s=src;
                int fw=std::min<int>(cs.Width,(int)d.Width),fh=std::min<int>(cs.Height,(int)d.Height);
                if(fw!=wr.right-wr.left||fh!=wr.bottom-wr.top){
                    RECT efb=wr;DwmGetWindowAttribute(hwnd,DWMWA_EXTENDED_FRAME_BOUNDS,&efb,sizeof(efb));
                    OffsetRect(&s,wr.left-efb.left,wr.top-efb.top);
                }
                s.left=std::clamp<LONG>(s.left,0,fw);s.right=std::clamp<LONG>(s.right,0,fw);
                s.top=std::clamp<LONG>(s.top,0,fh);s.bottom=std::clamp<LONG>(s.bottom,0,fh);
                LONG sw=s.right-s.left,sh=s.bottom-s.top;
                if(sw>=4&&sh>=4&&sw<=4096&&sh<=4096){
                    auto* dev=device.get();
                    winrt::com_ptr<ID3D11DeviceContext> ctx;dev->GetImmediateContext(ctx.put());
                    D3D11_TEXTURE2D_DESC sd{};sd.Width=sw;sd.Height=sh;sd.MipLevels=1;sd.ArraySize=1;
                    sd.Format=d.Format;sd.SampleDesc.Count=1;sd.Usage=D3D11_USAGE_STAGING;sd.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
                    winrt::com_ptr<ID3D11Texture2D> staging;
                    winrt::check_hresult(dev->CreateTexture2D(&sd,nullptr,staging.put()));
                    D3D11_BOX box{(UINT)s.left,(UINT)s.top,0,(UINT)s.right,(UINT)s.bottom,1};
                    ctx->CopySubresourceRegion(staging.get(),0,0,0,0,tex.get(),0,&box);
                    UINT dw=std::min<UINT>(sw,384),dh=std::max(1u,(UINT)((uint64_t)sh*dw/sw));
                    if(dh>384){dw=std::max(1u,(UINT)((uint64_t)dw*384/dh));dh=384;}
                    pixels.resize((size_t)dw*dh*4);
                    D3D11_MAPPED_SUBRESOURCE map{};
                    winrt::check_hresult(ctx->Map(staging.get(),0,D3D11_MAP_READ,0,&map));
                    for(UINT y=0;y<dh;y++)for(UINT x=0;x<dw;x++){
                        const BYTE* a=(const BYTE*)map.pData+(size_t)(y*sh/dh)*map.RowPitch+(x*sw/dw)*4;
                        BYTE* b=pixels.data()+((size_t)y*dw+x)*4;
                        b[0]=a[0];b[1]=a[1];b[2]=a[2];b[3]=255;
                    }
                    ctx->Unmap(staging.get(),0);
                    ok=StageContent(pixels,dw,dh);if(ok){w=dw;hh=dh;Log(L"organizador: WGC preview %p",hwnd);}
                }
            }
        }
    } catch(...) {}
    Stop(live);
    if(!ok){pixels.clear();w=hh=0;return StageLegacy(hwnd,src,pixels,w,hh);}
    return true;
}

std::vector<foto::Gpu> foto::Windows(const std::vector<HWND>& hs, const std::vector<RECT>& srcs, int timeoutMs) {
    double t0 = anim::Now();
    Live* live = Start(hs, srcs);
    if (!live) return std::vector<Gpu>(hs.size());
    double t1 = anim::Now();
    auto out = Grab(live, timeoutMs);
    double t2 = anim::Now();
    Stop(live);
    out.resize(hs.size());
    Log(L"foto (GPU): %zu janelas, abrir capturas %.1f ms, fotos %.1f ms", hs.size(), t1 - t0, t2 - t1);
    return out;
}
