// Organizador Visual (Stage Manager do Mac). Ligado (organizador=1): só o app da frente fica no palco; os outros vão
// para uma faixa à esquerda, com a miniatura ao vivo da janela e o ícone do app, agrupados por app.
//
// O motor é um porte para C++ do stage-manager-for-windows de e1roy (https://github.com/e1roy/stage-manager-for-windows,
// fork de renkai9418/stage-bar; licença MIT, Copyright (c) 2026 Renkai: o texto inteiro está em THIRD-PARTY-NOTICES.md),
// pedido do usuário em 23/11/2026 por ser bem mais estável que o nosso de antes:
// - as janelas que saem do palco são ESTACIONADAS logo fora da área de trabalho (só movidas, sem mudar tamanho nem
//   estado e sem redesenhar), não minimizadas. Voltam na hora, do jeito que estavam, e a miniatura da faixa é ao vivo;
// - cada mudança vai para um diário (palco-estacionadas.txt): se o Shell cair, a próxima abertura põe tudo de volta;
// - os diálogos andam junto com a janela dona; uma janela que o app trouxe de volta sozinho é estacionada de novo, e
//   uma que não chega em casa é mandada de novo e, por fim, encaixada na tela mais próxima;
// - o modelo do palco (quem está no palco, os grupos da faixa, de onde o app veio, os recentes) segue o stage.go dele:
//   o app que sai volta para o lugar de onde o novo veio, e fechar o app do palco traz o anterior.
// Simplificações deste porte: só o monitor principal (janelas nos outros ficam como estão); sem as animações de voo,
// o arrastar de cartões e a visão geral (o Mission Control do WacOS faz esse papel).
#include "shell.h"
#include <dwmapi.h>
#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <cstdio>
#include <thread>
#include <mutex>
#include <memory>
#include <atomic>
#include <shlobj.h>
#include "../comum/idioma.h"

namespace {

const UINT_PTR T_FRENTE = 1, T_REFRESH = 2, T_BORDA = 3, T_LOGO = 4, T_CAPTURE=5, T_RESULT=6;
// Reference silhouette (macOS screenshot, normalized to 800px high):
// near edge ~80px, projected width ~90px, icon ~26px, near edge x~14px.
// Keep the underlying app's aspect ratio and apply yaw before fitting.
const float STRIP_W = 190, THUMB_W = 94, THUMB_H = 80, ICON = 26, GAP = 26, LEFT = 14;
constexpr float STAGE_FAR_SCALE=.89f, STAGE_YAW_WIDTH=.59f;
const int MAX_TILES = 6;
const DWORD ASSENTAR_MS = 1000;   // depois de uma troca, o palco não é "podado" nem estaciona nada (stage.SettleTime)
const DWORD NOVA_MS = 1500;       // janela vista há pouco não é estacionada (ela pode estar abrindo para virar a da frente)
const DWORD FECHOU_MS = 1000;     // closeGrace: fechou uma janela do palco, a frente vai para a outra janela dele

HWND g_strip;
ui::GpuSurface g_s;
HWINEVENTHOOK g_hookFrente, g_hookObj;
bool g_on;

DWORD Agora() { return GetTickCount(); }

// ---------------------------------------------------------------- janelas (catalog.go)
struct Jan { HWND h; std::wstring app; DWORD pid; RECT r; bool frente; int z; };

std::wstring Classe(HWND h) { wchar_t c[128] = {}; GetClassNameW(h, c, 128); return c; }

bool Encoberta(HWND h) { DWORD c = 0; DwmGetWindowAttribute(h, DWMWA_CLOAKED, &c, sizeof(c)); return c != 0; }

RECT Moldura(HWND h) {
    RECT r{};
    if (FAILED(DwmGetWindowAttribute(h, DWMWA_EXTENDED_FRAME_BOUNDS, &r, sizeof(r)))) GetWindowRect(h, &r);
    return r;
}

std::map<DWORD, std::wstring> g_appDoPid;  // pid -> app (o caminho do exe em maiúsculas)

std::wstring CaminhoDo(DWORD pid) {
    std::wstring s;
    if (HANDLE p = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid)) {
        wchar_t b[MAX_PATH * 2]; DWORD n = MAX_PATH * 2;
        if (QueryFullProcessImageNameW(p, 0, b, &n)) s.assign(b, n);
        CloseHandle(p);
    }
    for (auto& c : s) c = (wchar_t)towupper(c);
    return s;
}

// o app da janela: o exe do processo; nas da Loja (ApplicationFrameHost), o processo da CoreWindow de dentro
std::wstring AppDe(HWND h, const std::wstring& cls, DWORD pid) {
    if(cls==L"ConsoleWindowClass")return L"CONSOLE:"+std::to_wstring(pid)+L":"+std::to_wstring((UINT_PTR)h);
    if (cls == L"ApplicationFrameWindow") {
        struct C { DWORD pid, achado; } c{ pid, 0 };
        EnumChildWindows(h, [](HWND ch, LPARAM l) -> BOOL {
            auto* c = (C*)l;
            if (Classe(ch) != L"Windows.UI.Core.CoreWindow") return TRUE;
            DWORD p = 0; GetWindowThreadProcessId(ch, &p);
            if (p && p != c->pid) { c->achado = p; return FALSE; }
            return TRUE;
        }, (LPARAM)&c);
        if (!c.achado) { wchar_t t[256] = {}; GetWindowTextW(h, t, 256); std::wstring k = L"FRAME:" + std::wstring(t); for (auto& ch : k) ch = (wchar_t)towupper(ch); return k; }
        pid = c.achado;
    }
    auto it = g_appDoPid.find(pid);
    if (it != g_appDoPid.end()) return it->second;
    std::wstring k = CaminhoDo(pid);
    if (k.empty()) k = L"PID:" + std::to_wstring(pid);
    g_appDoPid[pid] = k;
    return k;
}

// é uma janela de app que o palco organiza? (catalog.inspect)
bool JanelaDeApp(HWND h, HWND fg, Jan* out) {
    if (!IsWindowVisible(h) || GetAncestor(h, GA_ROOT) != h || IsIconic(h)) return false;  // minimizada é do Dock
    DWORD pid = 0; GetWindowThreadProcessId(h, &pid);
    if (!pid || pid == GetCurrentProcessId() || apps::IsShellWindow(h) || shell::Nossa(h)) return false;
    LONG ex = GetWindowLongW(h, GWL_EXSTYLE);
    bool appWin = (ex & WS_EX_APPWINDOW) != 0;
    if ((ex & WS_EX_TOOLWINDOW) && !appWin) return false;
    if (((ex & WS_EX_NOACTIVATE) && !appWin) || (ex & (WS_EX_TOPMOST|WS_EX_TRANSPARENT))) return false;  // sobreposições e "sempre por cima" flutuam
    if (Encoberta(h)) return false;
    std::wstring cls = Classe(h);
    static const std::set<std::wstring> fora = { L"Progman", L"WorkerW", L"Shell_TrayWnd", L"Shell_SecondaryTrayWnd", L"Windows.UI.Core.CoreWindow",
                                                 L"Windows.UI.Composition.DesktopWindowContentBridge", L"XamlExplorerHostIslandWindow" };
    if (fora.count(cls) || cls==L"CEF-OSC-WIDGET" || cls==L"OperationStatusWindow" ||
        cls==L"WacOSFileOperationProgress" || cls==L"ShellAlerta") return false;
    // Ignore non-interactive overlays and auxiliary windows in the same way
    // the Dock and Alt+Tab do; owned dialogues still travel with their root.
    if(!apps::IsAppWindow(h))return false;
    wchar_t t[8] = {};
    if (!GetWindowTextW(h, t, 8)) return false;  // sem título: não é janela de app
    RECT fr = Moldura(h);
    if (fr.right - fr.left < 40 || fr.bottom - fr.top < 30) return false;
    if (out) { out->h = h; out->pid = pid; out->app = AppDe(h, cls, pid); GetWindowRect(h, &out->r); out->frente = h == fg; }
    return true;
}

bool DonoNaLista(HWND h, const std::set<HWND>& lista) {
    HWND o = GetWindow(h, GW_OWNER);
    for (int i = 0; o && i < 16; i++) { if (lista.count(o)) return true; o = GetWindow(o, GW_OWNER); }
    return false;
}

// as janelas de app, da frente para trás; as que têm dono na lista (diálogos) ficam de fora: vão com o dono
std::vector<Jan> Janelas() {
    struct C { HWND fg; std::vector<Jan> v; } c{ GetForegroundWindow(), {} };
    EnumWindows([](HWND h, LPARAM l) -> BOOL {
        auto* c = (C*)l; Jan j;
        if (JanelaDeApp(h, c->fg, &j)) c->v.push_back(j);
        return TRUE;
    }, (LPARAM)&c);
    std::set<HWND> lista; for (auto& j : c.v) lista.insert(j.h);
    std::vector<Jan> out;
    for (auto& j : c.v) if (!DonoNaLista(j.h, lista)) { j.z = (int)out.size(); out.push_back(j); }
    return out;
}

std::vector<HWND> Possuidas(HWND dono) {  // as janelas visíveis cuja corrente de donos chega a `dono`
    struct C { HWND dono; std::vector<HWND> v; } c{ dono, {} };
    EnumWindows([](HWND h, LPARAM l) -> BOOL {
        auto* c = (C*)l;
        if (h == c->dono || !IsWindowVisible(h)) return TRUE;
        HWND o = GetWindow(h, GW_OWNER);
        for (int i = 0; o && i < 16; i++) { if (o == c->dono) { c->v.push_back(h); break; } o = GetWindow(o, GW_OWNER); }
        return TRUE;
    }, (LPARAM)&c);
    return c.v;
}

HWND DonoRaiz(HWND h) {  // sobe pelos donos visíveis até a janela que representa o app
    HWND r = GetAncestor(h, GA_ROOT); if (!r) r = h;
    for (int i = 0; i < 16; i++) { HWND o = GetWindow(r, GW_OWNER); if (!o || !IsWindowVisible(o)) break; r = o; }
    return r;
}

// ---------------------------------------------------------------- geometria dos monitores
bool NaTela(const RECT& r) {  // algum monitor mostra parte dela
    RECT x; HMONITOR m = MonitorFromRect(&r, MONITOR_DEFAULTTONULL);
    if (!m) return false;
    MONITORINFO mi{ sizeof(mi) }; GetMonitorInfoW(m, &mi);
    return IntersectRect(&x, &r, &mi.rcMonitor) != FALSE;
}
bool QuaseFora(const RECT& r) {  // menos da metade dela aparece em algum monitor
    LONG area = (r.right - r.left) * (r.bottom - r.top);
    if (area <= 0) return true;
    struct C { RECT r; LONGLONG vista; } c{ r, 0 };
    EnumDisplayMonitors(nullptr, nullptr, [](HMONITOR m, HDC, LPRECT, LPARAM l) -> BOOL {
        auto* c = (C*)l; MONITORINFO mi{ sizeof(mi) }; GetMonitorInfoW(m, &mi);
        RECT x; if (IntersectRect(&x, &c->r, &mi.rcMonitor)) c->vista += (LONGLONG)(x.right - x.left) * (x.bottom - x.top);
        return TRUE;
    }, (LPARAM)&c);
    return c.vista * 2 < area;
}
RECT Encaixa(RECT r) {  // o mínimo para caber na área de trabalho do monitor mais próximo
    MONITORINFO mi{ sizeof(mi) }; GetMonitorInfoW(MonitorFromRect(&r, MONITOR_DEFAULTTONEAREST), &mi);
    RECT w = mi.rcWork; LONG W = r.right - r.left, H = r.bottom - r.top;
    LONG x = std::clamp(r.left, w.left, std::max(w.left, w.right - W)), y = std::clamp(r.top, w.top, std::max(w.top, w.bottom - H));
    return { x, y, x + W, y + H };
}
RECT NaMesa(RECT casa) {  // para onde a janela pode voltar (parking.onDesktop)
    if (!NaTela(casa)) {  // o monitor dela sumiu: no meio do principal
        MONITORINFO mi{ sizeof(mi) }; GetMonitorInfoW(MonitorFromPoint({ 0, 0 }, MONITOR_DEFAULTTOPRIMARY), &mi);
        LONG W = casa.right - casa.left, H = casa.bottom - casa.top;
        LONG x = mi.rcWork.left + std::max(0L, (mi.rcWork.right - mi.rcWork.left - W) / 2), y = mi.rcWork.top + std::max(0L, (mi.rcWork.bottom - mi.rcWork.top - H) / 2);
        return { x, y, x + W, y + H };
    }
    return QuaseFora(casa) ? Encaixa(casa) : casa;
}

// ---------------------------------------------------------------- estacionar (parking.go)
// SWP_NOREDRAW: o Windows não pinta janela fora da tela; voltando, ela mostra a imagem que já tinha (sem piscar branco).
// Na volta, SWP_FRAMECHANGED: o Chromium recalcula a moldura para onde a janela vai (senão volta sem cantos e sem sombra).
const UINT MOVER = SWP_NOREDRAW | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_ASYNCWINDOWPOS;
const UINT VOLTAR = MOVER | SWP_FRAMECHANGED;
const LONG FOLGA = 160;

struct Vaga { RECT casa; POINT lugar; HWND dono; DWORD em; unsigned retries=0; bool confirmed=false; };
std::set<HWND> g_unparkable;
std::map<HWND,ID2D1Bitmap*> g_previews;
unsigned long long g_previewRevision=0;
std::wstring g_lastPaint;
void DropPreview(HWND h){
    auto it=g_previews.find(h);if(it!=g_previews.end()){if(it->second)it->second->Release();g_previews.erase(it);++g_previewRevision;}
}
// Inverse projective resampling into premultiplied BGRA. The left edge
// remains full height; the far right edge recedes. Cache once,
// then draw as an ordinary bitmap, independent of compositor projection.
std::vector<BYTE> StagePerspective(const std::vector<BYTE>& src,UINT w,UINT h){
    std::vector<BYTE> dst((size_t)w*h*4,0);
    const float p=1.f/STAGE_FAR_SCALE-1.f;
    // Native-style inactive card translucency. Preserve detail at the near
    // edge and let the wallpaper show through progressively at the far edge.
    // The same factor is applied to alpha AND premultiplied color channels;
    // no halo, cast shadow or extra draw pass is needed.
    for(UINT x=0;x<w;x++){
        float u=(x+.5f)/w;
        float fade=.94f-.20f*(u*u*(3.f-2.f*u));
        float sx=u/(1.f+p-p*u);
        float scale=1.f/(1.f+p*sx);
        float fx=std::clamp(sx*w-.5f,0.f,(float)w-1);
        UINT ax=(UINT)fx,bx=std::min(ax+1,w-1);float tx=fx-ax;
        float top=h*(1.f-scale)*.5f,bottom=h-top;
        for(UINT y=0;y<h;y++){
            float cover=std::clamp(std::min((y+1.f)-top,bottom-y),0.f,1.f);
            if(cover<=0)continue;
            float edge=std::clamp(std::min(y+.5f-top,bottom-y-.5f)/(std::max(1.f,h*.035f)),0.f,1.f);
            float opacity=cover*fade*(.975f+.025f*edge);
            float fy=std::clamp(((y+.5f-top)/scale)-.5f,0.f,(float)h-1);
            UINT ay=(UINT)fy,by=std::min(ay+1,h-1);float ty=fy-ay;
            BYTE* o=dst.data()+((size_t)y*w+x)*4;
            for(int c=0;c<4;c++){
                float a=src[((size_t)ay*w+ax)*4+c]*(1-tx)+src[((size_t)ay*w+bx)*4+c]*tx;
                float b=src[((size_t)by*w+ax)*4+c]*(1-tx)+src[((size_t)by*w+bx)*4+c]*tx;
                o[c]=(BYTE)std::lround((a*(1-ty)+b*ty)*opacity);
            }
        }
    }
    return dst;
}
struct CaptureJob {
    HWND h{};DWORD pid{};RECT src{};UINT w=0,hh=0;
    std::vector<BYTE> pixels;std::atomic<bool> done{false};
};
std::map<HWND,std::shared_ptr<CaptureJob>> g_jobs;
std::map<HWND,DWORD> g_captureAt,g_pendingPark;
auto& CaptureWorkers(){static auto* count=new std::atomic<int>(0);return *count;}
void CapturePreview(HWND h){
    if(!IsWindow(h)||IsHungAppWindow(h)||g_jobs.count(h)||CaptureWorkers().load()>=2)return;
    DWORD now=Agora();auto last=g_captureAt.find(h);
    if(last!=g_captureAt.end()&&now-last->second<1200)return;
    RECT r{},f{};GetWindowRect(h,&r);f=Moldura(h);
    RECT src{f.left-r.left,f.top-r.top,f.right-r.left,f.bottom-r.top};
    if(src.right-src.left<4||src.bottom-src.top<4||src.right-src.left>4096||src.bottom-src.top>4096)return;
    auto job=std::make_shared<CaptureJob>();job->h=h;job->src=src;
    GetWindowThreadProcessId(h,&job->pid);
    g_captureAt[h]=now;g_jobs[h]=job;++CaptureWorkers();
    try{
        std::thread([job]{
            HRESULT hr=CoInitializeEx(nullptr,COINIT_MULTITHREADED);
            try{
                std::vector<BYTE> px;
                if(foto::StagePixels(job->h,job->src,px,job->w,job->hh)&&job->w&&job->hh&&
                   px.size()==(size_t)job->w*job->hh*4)
                    job->pixels=StagePerspective(px,job->w,job->hh);
            }catch(...){}
            if(SUCCEEDED(hr))CoUninitialize();
            job->done.store(true,std::memory_order_release);--CaptureWorkers();
        }).detach();
    }catch(...){--CaptureWorkers();g_jobs.erase(h);}
}
bool CollectPreviews(){
    bool changed=false;
    for(auto it=g_jobs.begin();it!=g_jobs.end();){
        auto j=it->second;if(!j->done.load(std::memory_order_acquire)){++it;continue;}
        DWORD pid=0;GetWindowThreadProcessId(j->h,&pid);
        if(IsWindow(j->h)&&pid==j->pid&&!j->pixels.empty()){
            if(auto* dc=ui::Gpu())if(auto* b=ui::Bitmap(dc,j->w,j->hh,j->pixels)){
                DropPreview(j->h);g_previews[j->h]=b;++g_previewRevision;changed=true;
                Log(L"organizador: asynchronous preview %p %ux%u",j->h,j->w,j->hh);
            }
        }
        it=g_jobs.erase(it);
    }
    return changed;
}
std::map<HWND, Vaga> g_vagas;                          // estacionadas
struct Volta { POINT para; DWORD em; int tentativas; };
std::map<HWND, Volta> g_voltando;                     // mandadas para casa, até chegarem

std::wstring Diario() {
    wchar_t* p = nullptr; std::wstring d;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &p))) { d = p; CoTaskMemFree(p); }
    return d + L"\\Shell\\palco-estacionadas.txt";
}

void Grava() {  // o diário: cada estacionada com a casa e a vaga (para voltar depois de uma queda)
    std::wstring f = Diario();
    if (g_vagas.empty()) { DeleteFileW(f.c_str()); return; }
    FILE* fp = nullptr;
    if (_wfopen_s(&fp, (f + L".tmp").c_str(), L"w") || !fp) return;
    for (auto& [h, v] : g_vagas) fprintf(fp, "%llu %ld %ld %ld %ld\n", (unsigned long long)(ULONG_PTR)h, v.casa.left, v.casa.top, v.lugar.x, v.lugar.y);
    fclose(fp);
    MoveFileExW((f + L".tmp").c_str(), f.c_str(), MOVEFILE_REPLACE_EXISTING);
}

RECT Onde(HWND h) {  // onde a janela aparece, ou, minimizada, onde ela volta
    RECT r{};
    if (IsIconic(h)) { WINDOWPLACEMENT wp{ sizeof(wp) }; if (GetWindowPlacement(h, &wp)) return wp.rcNormalPosition; }
    GetWindowRect(h, &r);
    return r;
}

void Por(HWND h, POINT p, UINT flags) {
    if (IsIconic(h)) {  // minimizada: muda onde ela volta
        WINDOWPLACEMENT wp{ sizeof(wp) };
        if (GetWindowPlacement(h, &wp)) { OffsetRect(&wp.rcNormalPosition, p.x - wp.rcNormalPosition.left, p.y - wp.rcNormalPosition.top); SetWindowPlacement(h, &wp); }
        return;
    }
    SetWindowPos(h, nullptr, p.x, p.y, 0, 0, flags);
}

// uma vaga fora da área de trabalho cujo monitor mais próximo ainda é o da janela (o DPI não muda enquanto espera)
POINT VagaPara(const RECT& casa) {
    RECT d{ GetSystemMetrics(SM_XVIRTUALSCREEN), GetSystemMetrics(SM_YVIRTUALSCREEN), 0, 0 };
    d.right = d.left + GetSystemMetrics(SM_CXVIRTUALSCREEN); d.bottom = d.top + GetSystemMetrics(SM_CYVIRTUALSCREEN);
    LONG W = casa.right - casa.left, H = casa.bottom - casa.top;
    HMONITOR mon = MonitorFromRect(&casa, MONITOR_DEFAULTTONEAREST);
    POINT cand[4] = { { casa.left, d.bottom + FOLGA }, { casa.left, d.top - H - FOLGA }, { d.right + FOLGA, casa.top }, { d.left - W - FOLGA, casa.top } };
    for (auto& p : cand) { RECT r{ p.x, p.y, p.x + W, p.y + H }; if (MonitorFromRect(&r, MONITOR_DEFAULTTONEAREST) == mon) return p; }
    return cand[0];
}

bool EstacionaUma(HWND h, HWND dono, RECT casa, POINT lugar) {
    // Journal/mark before requesting an asynchronous move; window-limit
    // hooks must recognize the intentional off-screen position immediately.
    g_vagas[h] = { casa, lugar, dono, Agora() };
    Grava();
    if (!SetWindowPos(h, nullptr, lugar.x, lugar.y, 0, 0, MOVER)){
        g_vagas.erase(h);Grava();return false;
    }
    return true;
}

bool Estaciona(HWND h) {
    if (g_vagas.count(h) || g_unparkable.count(h) || !IsWindow(h) || IsIconic(h)) return false;
    RECT r; GetWindowRect(h, &r);
    if (IsRectEmpty(&r) || !NaTela(r)) return false;  // já fora: a vaga viraria a casa e ela nunca voltaria
    RECT casa = NaMesa(r);
    POINT lugar = VagaPara(casa);
    if(!g_previews.count(h)){
        CapturePreview(h);
        if(g_jobs.count(h)){
            auto [it,added]=g_pendingPark.emplace(h,Agora());
            // First-time capture gets a bounded head start while the window
            // is still on the monitor. The UI thread never waits for it.
            if(Agora()-it->second<350)return false;
        }
    }
    g_pendingPark.erase(h);
    if (!EstacionaUma(h, nullptr, casa, lugar)) return false;
    LONG dx = lugar.x - r.left, dy = lugar.y - r.top, sx = casa.left - r.left, sy = casa.top - r.top;
    for (HWND o : Possuidas(h)) {  // os diálogos (e a sombra que alguns apps desenham numa janela) vão junto
        if (g_vagas.count(o)) continue;
        RECT orr; GetWindowRect(o, &orr);
        if (IsRectEmpty(&orr) || !NaTela(orr)) continue;
        RECT em = orr; OffsetRect(&em, dx, dy);
        RECT oc = orr; OffsetRect(&oc, sx, sy);
        if (NaTela(em)) { oc = NaMesa(orr); EstacionaUma(o, h, oc, VagaPara(oc)); }  // longe do dono: espera à parte
        else EstacionaUma(o, h, oc, { em.left, em.top });
    }
    Grava();
    return true;
}

void Manda(HWND h, POINT p) { Por(h, p, VOLTAR); g_voltando[h] = { p, Agora(), 0 }; }

POINT Alcancavel(RECT r) { if (!NaTela(r)) r = Encaixa(r); return { r.left, r.top }; }

bool Desestaciona(HWND h) {
    auto it = g_vagas.find(h);
    if (it == g_vagas.end()) return false;
    Vaga raiz = it->second;
    RECT casa = NaMesa(raiz.casa);
    LONG sx = casa.left - raiz.casa.left, sy = casa.top - raiz.casa.top;
    for (auto i = g_vagas.begin(); i != g_vagas.end();) {
        if (i->first == h || i->second.dono == h) {
            RECT c = i->second.casa; OffsetRect(&c, sx, sy);
            HWND w = i->first; i = g_vagas.erase(i);
            Manda(w, Alcancavel(c));
        } else ++i;
    }
    // um diálogo aberto enquanto o dono esperava fora nasceu perto dele: volta junto, pela mesma distância
    LONG dx = casa.left - raiz.lugar.x, dy = casa.top - raiz.lugar.y;
    for (HWND o : Possuidas(h)) { RECT r; GetWindowRect(o, &r); if (QuaseFora(r)) { OffsetRect(&r, dx, dy); Manda(o, Alcancavel(r)); } }
    return true;
}

// traz janelas para casa, por cima das outras, a primeira na frente (e `frente`, se houver, por cima de todas)
void Traz(const std::vector<HWND>& hs, HWND frente = nullptr) {
    std::vector<HWND> casa;
    for (HWND h : hs) if (Desestaciona(h)) casa.push_back(h);
    if (casa.empty()) return;
    Grava();
    if (frente) casa.insert(casa.begin(), frente);
    for (auto it = casa.rbegin(); it != casa.rend(); ++it) {  // de trás para a frente: a primeira termina por cima
        UINT f = SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOREDRAW | SWP_ASYNCWINDOWPOS;
        SetWindowPos(*it, HWND_TOP, 0, 0, 0, 0, f);
    }
}

bool Chegou(HWND h, POINT p) { RECT r = Onde(h); return (r.left == p.x && r.top == p.y) || !QuaseFora(r); }

void Persegue() {  // as mandadas para casa que não chegaram: de novo (até 3 vezes); depois, encaixadas na tela
    DWORD agora = Agora();
    for (auto it = g_voltando.begin(); it != g_voltando.end();) {
        HWND h = it->first; Volta& v = it->second;
        if (!IsWindow(h) || g_vagas.count(h)) {it=g_voltando.erase(it);continue;}
        if(Chegou(h,v.para)){
            RedrawWindow(h,nullptr,nullptr,RDW_INVALIDATE|RDW_FRAME|RDW_ALLCHILDREN);
            it=g_voltando.erase(it);continue;
        }
        if (agora - v.em < 300) { ++it; continue; }
        if (v.tentativas < 3) { Log(L"organizador: janela %p não chegou em casa; de novo", h); Por(h, v.para, VOLTAR); v.em = agora; v.tentativas++; ++it; continue; }
        RECT r = Encaixa(Onde(h)); Por(h, { r.left, r.top }, VOLTAR);
        Log(L"organizador: janela %p não voltou para casa; encaixada na tela", h);
        it = g_voltando.erase(it);
    }
}

// as estacionadas que voltaram sozinhas (o Windows reencaixa as maximizadas quando a área muda, ou o próprio app)
void Fugidas() {
    bool mudou=false;
    std::vector<HWND> failed;
    for(auto it=g_vagas.begin();it!=g_vagas.end();){
        HWND h=it->first; Vaga& v=it->second;
        if(!IsWindow(h)){DropPreview(h);it=g_vagas.erase(it);mudou=true;continue;}
        if(IsIconic(h)){++it;continue;}
        RECT r=Onde(h);
        if(!NaTela(r)){v.confirmed=true;++it;continue;}
        if(Agora()-v.em<1000){++it;continue;}
        if(!v.dono && DonoRaiz(GetForegroundWindow())==h){
            // Let the foreground handler restore the entire app group.
            ++it;continue;
        }
        RECT d{GetSystemMetrics(SM_XVIRTUALSCREEN),GetSystemMetrics(SM_YVIRTUALSCREEN),0,0};
        d.right=d.left+GetSystemMetrics(SM_CXVIRTUALSCREEN);d.bottom=d.top+GetSystemMetrics(SM_CYVIRTUALSCREEN);
        LONG width=r.right-r.left,height=r.bottom-r.top;
        POINT p[4]={{v.casa.left,d.top-height-FOLGA},{d.left-width-FOLGA,v.casa.top},
                    {d.right+FOLGA,v.casa.top},{v.casa.left,d.bottom+FOLGA}};
        // Retain the original home and retry budget even if the entire
        // window is clamped back on-screen. Never recapture/re-enrol it.
        if(v.retries<4){
            v.lugar=p[v.retries++];v.em=Agora();Por(h,v.lugar,MOVER);
            Log(L"organizador: parking retry %p %u/4",h,v.retries);
            mudou=true;++it;continue;
        }
        failed.push_back(v.dono?v.dono:h);
        ++it;
    }
    std::sort(failed.begin(),failed.end(),std::less<HWND>{});
    failed.erase(std::unique(failed.begin(),failed.end()),failed.end());
    for(HWND h:failed){
        g_unparkable.insert(h);
        if(Desestaciona(h)){Log(L"organizador: parking refused %p; restored group",h);mudou=true;}
    }
    if(mudou)Grava();
}

void TrazTodas() {  // desligar, sair: tudo em casa (e espera até 1 s elas chegarem)
    for (auto& [h, v] : g_vagas) if (IsWindow(h)) { RECT c = NaMesa(v.casa); Manda(h, { c.left, c.top }); RedrawWindow(h, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_FRAME | RDW_ALLCHILDREN); }
    g_vagas.clear();
    Grava();
    DWORD t0 = Agora(); bool reenviou = false;
    while (!g_voltando.empty() && Agora() - t0 < 1000) {
        for (auto it = g_voltando.begin(); it != g_voltando.end();) { if (!IsWindow(it->first) || Chegou(it->first, it->second.para)) it = g_voltando.erase(it); else ++it; }
        if (!reenviou && Agora() - t0 > 500) { reenviou = true; for (auto& [h, v] : g_voltando) Por(h, v.para, VOLTAR); }
        Sleep(20);
    }
    for (auto& [h, v] : g_voltando) { RECT r = Encaixa(Onde(h)); Por(h, { r.left, r.top }, VOLTAR); }
    g_voltando.clear();
}

// ---------------------------------------------------------------- o modelo do palco (stage.go, um monitor)
using Grupo = std::vector<std::wstring>;
std::vector<std::wstring> g_palco;     // os apps no palco, na ordem em que entraram
std::vector<Grupo> g_grupos;           // a faixa, de cima para baixo: os apps de cada cartão
std::wstring g_acima;                  // o app do cartão logo acima de onde o app do palco veio
std::vector<std::wstring> g_recentes;  // os que saíram do palco, o mais recente primeiro
DWORD g_trocouEm;

bool NoPalco(const std::wstring& a) { return std::find(g_palco.begin(), g_palco.end(), a) != g_palco.end(); }
bool Contem(const Grupo& g, const std::wstring& a) { return std::find(g.begin(), g.end(), a) != g.end(); }
int GrupoDe(const std::wstring& a) { for (int i = 0; i < (int)g_grupos.size(); i++) if (Contem(g_grupos[i], a)) return i; return -1; }
bool Assentando() { return Agora() - g_trocouEm < ASSENTAR_MS; }
int VeioDe() { if (g_acima.empty()) return 0; return GrupoDe(g_acima) + 1; }

template <class F> void Tira(F sai) {
    for (auto& g : g_grupos) g.erase(std::remove_if(g.begin(), g.end(), sai), g.end());
    g_grupos.erase(std::remove_if(g_grupos.begin(), g_grupos.end(), [](const Grupo& g) { return g.empty(); }), g_grupos.end());
}

Grupo GrupoCompleto(const std::wstring& a) {  // SetOf
    if (NoPalco(a)) return g_palco;
    int i = GrupoDe(a);
    return i >= 0 ? g_grupos[i] : Grupo{ a };
}

void FaixaCom(const std::vector<std::wstring>& presentes) {  // Strip: tira os que fecharam; os novos entram no topo
    auto ausente = [&](const std::wstring& a) { return std::find(presentes.begin(), presentes.end(), a) == presentes.end(); };
    Tira(ausente);
    g_recentes.erase(std::remove_if(g_recentes.begin(), g_recentes.end(), ausente), g_recentes.end());
    std::vector<Grupo> novos;
    for (auto& a : presentes) {
        if (GrupoDe(a) >= 0 || NoPalco(a)) continue;
        if (std::none_of(novos.begin(), novos.end(), [&](const Grupo& g) { return g[0] == a; })) novos.push_back({ a });
    }
    g_grupos.insert(g_grupos.begin(), novos.begin(), novos.end());
}

void Troca(const std::vector<std::wstring>& prox) {  // Switch
    auto vem = [&](const std::wstring& a) { return std::find(prox.begin(), prox.end(), a) != prox.end(); };
    std::vector<std::wstring> saindo;
    for (auto& a : g_palco) if (!vem(a)) saindo.push_back(a);
    int vaga = -1;
    for (int i = 0; i < (int)g_grupos.size() && vaga < 0; i++) if (std::any_of(g_grupos[i].begin(), g_grupos[i].end(), vem)) vaga = i;
    std::wstring acima = g_acima;
    if (std::none_of(g_palco.begin(), g_palco.end(), vem)) { acima.clear(); if (vaga > 0) acima = g_grupos[vaga - 1][0]; }
    if (!saindo.empty()) {
        if (vaga < 0) vaga = VeioDe();
        g_grupos.insert(g_grupos.begin() + std::min(vaga, (int)g_grupos.size()), saindo);  // o que sai fica onde o novo estava
        for (auto& a : saindo) g_recentes.erase(std::remove(g_recentes.begin(), g_recentes.end(), a), g_recentes.end());
        g_recentes.insert(g_recentes.begin(), saindo.begin(), saindo.end());
    }
    Tira(vem);
    g_palco = prox;
    g_acima = acima;
    g_trocouEm = Agora();
}

void Sai(const std::wstring& a) {  // Leave: o app fechou todas as janelas do palco
    if (!NoPalco(a)) return;
    g_palco.erase(std::remove(g_palco.begin(), g_palco.end(), a), g_palco.end());
    Tira([&](const std::wstring& x) { return x == a; });
    g_grupos.insert(g_grupos.begin(), Grupo{ a });
    g_recentes.erase(std::remove(g_recentes.begin(), g_recentes.end(), a), g_recentes.end());
}

Grupo Anterior() {  // Previous: o grupo do último que saiu do palco e ainda está na faixa
    for (auto& a : g_recentes) { if (NoPalco(a)) continue; int i = GrupoDe(a); if (i >= 0) return g_grupos[i]; }
    return {};
}

// ---------------------------------------------------------------- o organizador
std::vector<Jan> g_jan;                 // a última lista (só as do monitor principal)
std::map<HWND, DWORD> g_vista;          // quando cada janela apareceu na lista
HWND g_fg, g_fgPendente; int g_fgTentativas;
struct { HWND h; DWORD em; bool on; } g_fechou{};
bool g_ocupado;

HMONITOR Principal() { return MonitorFromPoint({ 0, 0 }, MONITOR_DEFAULTTOPRIMARY); }

bool NoPrincipal(const Jan& j) {  // estacionada: pelo monitor da casa
    auto it = g_vagas.find(j.h);
    RECT r = it != g_vagas.end() ? it->second.casa : j.r;
    return MonitorFromRect(&r, MONITOR_DEFAULTTONEAREST) == Principal();
}

void Lista() {
    auto previous=std::move(g_jan);g_jan.clear();
    for (auto& j : Janelas()) if (NoPrincipal(j)) g_jan.push_back(j);
    // Keep intentionally parked roots even when an application temporarily
    // changes its visibility/style or DWM suppresses the off-screen frame.
    for(auto& j:previous){
        auto v=g_vagas.find(j.h);
        if(v==g_vagas.end()||v->second.dono||!IsWindow(j.h)||IsIconic(j.h))continue;
        DWORD pid=0,cloak=0;GetWindowThreadProcessId(j.h,&pid);
        DwmGetWindowAttribute(j.h,DWMWA_CLOAKED,&cloak,sizeof(cloak));
        if(pid!=j.pid||(cloak&2))continue; // another virtual desktop
        if(std::none_of(g_jan.begin(),g_jan.end(),[&](const Jan& q){return q.h==j.h;})){
            GetWindowRect(j.h,&j.r);j.frente=false;g_jan.push_back(j);
        }
    }
    DWORD agora = Agora();
    std::set<HWND> vivas;
    for (auto& j : g_jan) { vivas.insert(j.h); g_vista.emplace(j.h, agora); }
    for (auto it = g_vista.begin(); it != g_vista.end();) it = vivas.count(it->first) ? std::next(it) : g_vista.erase(it);
    for(auto it=g_unparkable.begin();it!=g_unparkable.end();)it=IsWindow(*it)?std::next(it):g_unparkable.erase(it);
}

const Jan* AchaJan(HWND h) { for (auto& j : g_jan) if (j.h == h) return &j; return nullptr; }
bool Estacionada(HWND h) { return g_vagas.count(h) != 0; }

std::vector<std::wstring> AppsPresentes() {  // na ordem da frente para trás
    std::vector<std::wstring> v;
    for (auto& j : g_jan) if (std::find(v.begin(), v.end(), j.app) == v.end()) v.push_back(j.app);
    return v;
}

void Pinta();

// leva `prox` ao palco: estaciona as do que sai, traz as do que entra, e a `foco` vai para a frente (se ativar)
void TrocaPalco(std::vector<std::wstring> prox, HWND foco, bool ativar) {
    if (g_ocupado) return;
    g_ocupado = true;
    Lista();
    auto vem = [&](const std::wstring& a) { return std::find(prox.begin(), prox.end(), a) != prox.end(); };
    std::vector<HWND> saem, entram;
    for (auto& j : g_jan) {
        if (vem(j.app)) { if (Estacionada(j.h)) entram.push_back(j.h); }
        else if (!Estacionada(j.h)) saem.push_back(j.h);
    }
    if (ativar && !foco) for (auto& j : g_jan) if (vem(j.app)) { foco = j.h; break; }
    // a que vai para a frente vem primeiro (as outras na ordem em que estavam)
    if (foco) { auto it = std::find(entram.begin(), entram.end(), foco); if (it != entram.end()) std::rotate(entram.begin(), it, it + 1); }
    Traz(entram);
    for (HWND h : saem) if (h != foco) Estaciona(h);
    Troca(prox);
    for(auto& j:g_jan)if(NoPalco(j.app))g_pendingPark.erase(j.h);
    FaixaCom(AppsPresentes());
    if (ativar && foco) { shell::LiberaFrente(); SetForegroundWindow(foco); }
    std::wstring nomes; for (auto& a : prox) { App* ap = nullptr; for (auto& j : g_jan) if (j.app == a) { ap = apps::OfWindow(j.h); break; } nomes += (nomes.empty() ? L"" : L", ") + (ap ? ap->name : a); }
    Log(L"organizador: no palco %ls (%zu saem, %zu entram, %zu estacionadas)", nomes.c_str(), saem.size(), entram.size(), g_vagas.size());
    g_ocupado = false;
    Pinta();
}

void Atualiza() {  // refresh
    if (!g_on || g_ocupado || GetCapture()==g_strip || revelar::DesktopShown()) return;
    Persegue();
    Lista();
    Fugidas();
    DWORD agora = Agora();
    std::set<std::wstring> visiveis;
    for (auto& j : g_jan) if (!Estacionada(j.h)) visiveis.insert(j.app);
    // o app do palco fechou (ou foi minimizado): sai, e o anterior volta (refill)
    if (!Assentando()) {
        std::vector<std::wstring> foram;
        for (auto& a : g_palco) if (!visiveis.count(a)) foram.push_back(a);
        for (auto& a : foram) Sai(a);
        if (!foram.empty() && g_palco.empty()) {
            HWND fg = DonoRaiz(GetForegroundWindow());
            const Jan* jf = AchaJan(fg);
            if (!jf || Estacionada(jf->h)) {  // a frente não ficou com outra janela deste monitor
                Grupo ant = Anterior();
                if (!ant.empty()) { FaixaCom(AppsPresentes()); TrocaPalco(ant, nullptr, true); return; }
            }
        }
    }
    // as janelas de apps fora do palco que ainda aparecem (abertas por trás, por exemplo) vão para a faixa
    if (!Assentando()) {
        HWND fg = DonoRaiz(GetForegroundWindow());
        for (auto& j : g_jan)
            if (!NoPalco(j.app) && !Estacionada(j.h) && j.h != fg && agora - g_vista[j.h] > NOVA_MS) Estaciona(j.h);
    }
    FaixaCom(AppsPresentes());
    Pinta();
}

void AssentaFrente() {  // settleForeground
    HWND h = g_fgPendente;
    if (HWND fg = GetForegroundWindow(); h && fg && fg != h) h = g_fgPendente = fg;
    if (!h || g_ocupado || !g_on || revelar::DesktopShown()) return;
    HWND raiz = DonoRaiz(h);
    Lista();
    const Jan* j = AchaJan(raiz);
    if (!j) {  // ainda não é janela de app (abrindo): tenta mais algumas vezes
        if (++g_fgTentativas < 6 && IsWindow(h)) SetTimer(g_strip, T_FRENTE, 70, nullptr);
        else g_fgPendente = nullptr;
        return;
    }
    g_fgPendente = nullptr;
    SetTimer(g_strip,T_CAPTURE,300,nullptr);
    if (NoPalco(j->app)) {
        g_fechou.on = false;
        if (Estacionada(j->h)) { Traz({ j->h }); Pinta(); }
        return;
    }
    // fechou uma janela do palco e o Windows passou a frente para outro app: o palco fica com a outra janela dele
    if (g_fechou.on && Agora() - g_fechou.em < FECHOU_MS) {
        for (auto& o : g_jan)
            if (o.h != g_fechou.h && !Estacionada(o.h) && NoPalco(o.app)) { g_fechou.on = false; shell::LiberaFrente(); SetForegroundWindow(o.h); return; }
    }
    g_fechou.on = false;
    TrocaPalco(GrupoCompleto(j->app), j->h, false);
}

void CALLBACK NaFrente(HWINEVENTHOOK, DWORD, HWND h, LONG obj, LONG, DWORD, DWORD) {
    if (!g_on || !g_strip || obj != OBJID_WINDOW || !h) return;
    if (mesa::IsDeskWindow(h) || apps::IsShellWindow(h)) return;  // a mesa e as janelas do WacOS não mudam o palco
    g_fgPendente = h; g_fgTentativas = 0;
    SetTimer(g_strip, T_FRENTE, 70, nullptr);
}

void CALLBACK NoObjeto(HWINEVENTHOOK, DWORD ev, HWND h, LONG obj, LONG, DWORD, DWORD) {
    if (!g_on || !g_strip || obj != OBJID_WINDOW || !h) return;
    if (ev == EVENT_OBJECT_DESTROY) {  // noteClosed
        if (const Jan* j = AchaJan(h)) if (!Estacionada(h) && NoPalco(j->app)) g_fechou = { h, Agora(), true };
    } else if (GetAncestor(h, GA_ROOT) != h || shell::Nossa(h)) return;
    SetTimer(g_strip, T_LOGO, 120, nullptr);  // refreshSoon
}

// ---------------------------------------------------------------- a faixa (o desenho de antes)
std::map<HWND, HTHUMBNAIL> g_thumbs;
struct Tile { int grupo; HWND win; std::vector<App*> apps; D2D1_RECT_F thumb, icon; };
std::vector<Tile> g_tiles;
int g_hover = -1;
int g_pressed=-1; POINT g_pressPoint{}; bool g_dragGroup=false;
bool g_raised = false;  // trazida para a frente pela borda esquerda
std::map<std::wstring, ID2D1Bitmap*> g_icons;

float Sc() { return ui::Scale(g_strip); }

RECT AreaPrincipal() { MONITORINFO mi{ sizeof(mi) }; GetMonitorInfoW(Principal(), &mi); return mi.rcWork; }

void ClearThumbs() { for (auto& [h, t] : g_thumbs) DwmUnregisterThumbnail(t); g_thumbs.clear(); }

void PlaceStrip() {  // logo acima da mesa: abaixo de todas as janelas dos apps
    RECT wa = AreaPrincipal();
    int w = (int)std::lround(STRIP_W * Sc());
    HWND desk = nullptr;
    // The first visible desktop in front-to-back order is the one that
    // can obscure the strip. Progman may remain behind SequoiaMesa.
    for(HWND x=GetTopWindow(nullptr);x;x=GetWindow(x,GW_HWNDNEXT)){
        if(!IsWindowVisible(x)||!mesa::IsDeskWindow(x))continue;
        RECT r{},overlap{};
        if(GetWindowRect(x,&r)&&IntersectRect(&overlap,&r,&wa)){desk=x;break;}
    }
    HWND after=desk?GetWindow(desk,GW_HWNDPREV):HWND_BOTTOM;
    if(desk&&(!after||(GetWindowLongPtrW(after,GWL_EXSTYLE)&WS_EX_TOPMOST)))after=HWND_TOP;
    bool already=after==g_strip;
    RECT current{};GetWindowRect(g_strip,&current);
    bool same=current.left==wa.left&&current.top==wa.top&&current.right-current.left==w&&current.bottom==wa.bottom;
    if(already&&same&&IsWindowVisible(g_strip))return;
    SetWindowPos(g_strip, already?nullptr:after?after:HWND_BOTTOM,wa.left,wa.top,w,wa.bottom-wa.top,
        SWP_NOACTIVATE|SWP_SHOWWINDOW|(already?SWP_NOZORDER:0));
}

void Pinta() {
    if (!g_strip || !g_on) return;
    float sc = Sc();
    if(!g_raised)PlaceStrip();
    RECT wr; GetWindowRect(g_strip, &wr);
    int wpx = wr.right - wr.left, hpx = wr.bottom - wr.top;
    g_tiles.clear();
    for (int gi = 0; gi < (int)g_grupos.size() && (int)g_tiles.size() < MAX_TILES; gi++) {
        Tile t{ gi, nullptr, {} };
        for (auto& a : g_grupos[gi])
            for (auto& j : g_jan) if (j.app == a) {  // a janela mais à frente de cada app do cartão
                if (!t.win || (!g_previews.count(t.win)&&g_previews.count(j.h))) t.win = j.h;
                if (App* ap = apps::OfWindow(j.h)) if (std::find(t.apps.begin(), t.apps.end(), ap) == t.apps.end()) t.apps.push_back(ap);
                break;
            }
        if (t.win) g_tiles.push_back(t);
    }
    float H=hpx/sc,lo=18.f,hi=std::max(lo,H-66.f);
    std::vector<std::pair<float,float>> free{{lo,hi}};
    for(auto q:widgets::Areas()){
        if(q.right<=wr.left||q.left>=wr.right)continue;
        float a=(q.top-wr.top)/sc-12,b=(q.bottom-wr.top)/sc+12;
        std::vector<std::pair<float,float>> next;
        for(auto [l,r]:free){if(b<=l||a>=r)next.push_back({l,r});else{if(a>l)next.push_back({l,a});if(b<r)next.push_back({b,r});}}
        free.swap(next);
    }
    if(!free.empty()){
        auto best=*std::max_element(free.begin(),free.end(),[](auto a,auto b){return a.second-a.first<b.second-b.first;});
        lo=best.first;hi=best.second;
    }
    float n=(float)g_tiles.size(),gap=18.f;
    float th=n?std::clamp((hi-lo-gap*std::max(0.f,n-1)-8)/n,36.f,THUMB_H):THUMB_H;
    float tw=THUMB_W*th/THUMB_H,ic=ICON*std::min(1.f,th/THUMB_H+.12f);
    float total=n*th+std::max(0.f,n-1)*gap;
    float y=lo+std::max(0.f,(hi-lo-total)/2);
    for(auto& t:g_tiles){
        auto it=g_previews.find(t.win);
        auto sz=it!=g_previews.end()?it->second->GetSize():D2D1::SizeF(1.8f,1.f);
        // Apply horizontal foreshortening BEFORE fitting. Previously the
        // unprojected width limited scale, making very wide, short cards.
        float projected=sz.width*STAGE_YAW_WIDTH;
        float k=std::min(tw/projected,th/sz.height);
        float width=projected*k,height=sz.height*k;
        float top=y+(th-height)*.5f;
        t.thumb=D2D1::RectF(LEFT,top,LEFT+width,top+height);
        // The icon overlaps the lower near corner, like the reference.
        t.icon=D2D1::RectF(LEFT-ic*.30f,top+height-ic*.82f,LEFT+ic*.70f,top+height+ic*.18f);
        y+=th+gap;
    }
    // Recompose only when content or geometry changes. A refresh timer must
    // not repeatedly clear/present the same transparent swap chain.
    for(auto it=g_previews.begin();it!=g_previews.end();){if(!IsWindow(it->first)){it->second->Release();it=g_previews.erase(it);++g_previewRevision;}else ++it;}
    std::wstring key=std::to_wstring(wpx)+L":"+std::to_wstring(hpx)+L":"+std::to_wstring(sc)+L":"+std::to_wstring(g_previewRevision)+L":"+std::to_wstring(g_hover);
    for(auto& t:g_tiles){
        key+=L"|"+std::to_wstring((UINT_PTR)t.win)+L":"+std::to_wstring(t.thumb.left)+L":"+std::to_wstring(t.thumb.top)+L":"+std::to_wstring(t.thumb.right)+L":"+std::to_wstring(t.thumb.bottom);
        for(auto* a:t.apps)key+=L":"+a->id;
    }
    if(g_s.swap&&key==g_lastPaint)return;
    ID2D1DeviceContext* dc = ui::GpuBegin(g_s, wpx, hpx, sc);
    if (!dc) return;
    for(auto& t:g_tiles){
        auto it=g_previews.find(t.win);
        auto* b=it==g_previews.end()?nullptr:it->second;
        float width=t.thumb.right-t.thumb.left,height=t.thumb.bottom-t.thumb.top;
        float x=t.thumb.left,cy=(t.thumb.top+t.thumb.bottom)/2;
        // The strip uses a shared vanishing point near its vertical centre:
        // upper cards recede downwards, lower cards upwards. The fade is
        // already baked into the projected premultiplied bitmap.
        D2D1_RECT_F dst{x,cy-height/2,x+width,cy+height/2};
        if(b){
            // Far edge shares the same vanishing point as its vertical
            // scale: the top card tips down, bottom card tips up.
            float shift=std::clamp((H*.5f-cy)*(1.f-STAGE_FAR_SCALE),-height*.22f,height*.22f);
            dc->SetTransform(D2D1::Matrix3x2F(1,shift/width,0,1,0,-x*shift/width));
            dc->DrawBitmap(b,dst,1.f,D2D1_INTERPOLATION_MODE_HIGH_QUALITY_CUBIC);
            dc->SetTransform(D2D1::Matrix3x2F::Identity());
        }
        // If an application blocks capture, its icon remains selectable. Do not
        // masquerade a blank rectangle as a working application preview.
    }
    for (auto& t : g_tiles) {  // os ícones dos apps do cartão, um ao lado do outro no canto de baixo
        float dx = 0;
        for (App* a : t.apps) {
            ID2D1Bitmap*& b = g_icons[a->id];
            if (!b) { apps::EnsureIcon(*a, (int)std::lround(ICON * 2 * sc)); b = ui::Bitmap(dc, a->iw, a->ih, a->icon); }
            if (b) dc->DrawBitmap(b, D2D1::RectF(t.icon.left + dx, t.icon.top, t.icon.right + dx, t.icon.bottom), 1, D2D1_INTERPOLATION_MODE_HIGH_QUALITY_CUBIC);
            dx += ICON * 0.7f;
        }
    }
    ui::GpuEnd(g_s);
    size_t visiblePreviews=0;
    for(const auto& t:g_tiles)if(g_previews.count(t.win))++visiblePreviews;
    Log(L"organizador: strip draw %zu cards, %zu previews",g_tiles.size(),visiblePreviews);
    g_lastPaint=std::move(key);
}

int HitTile(LPARAM l) {
    float sc = Sc(), x = (short)LOWORD(l) / sc, y = (short)HIWORD(l) / sc;
    for (int i = 0; i < (int)g_tiles.size(); i++) {
        auto& t = g_tiles[i];
        float width=t.thumb.right-t.thumb.left,height=t.thumb.bottom-t.thumb.top;
        RECT wr{};GetWindowRect(g_strip,&wr);
        float cy=(t.thumb.top+t.thumb.bottom)*.5f,H=(wr.bottom-wr.top)/sc;
        float shift=std::clamp((H*.5f-cy)*(1.f-STAGE_FAR_SCALE),-height*.22f,height*.22f);
        float u=width>0?std::clamp((x-t.thumb.left)/width,0.f,1.f):0.f;
        float h=height*(1.f-(1.f-STAGE_FAR_SCALE)*u)*.5f;
        if ((x >= t.thumb.left && x < t.thumb.right && y >= cy+u*shift-h && y < cy+u*shift+h) ||
            (x >= t.icon.left && x < t.icon.right + ICON && y >= t.icon.top && y < t.icon.bottom)) return i;
    }
    return -1;
}

LRESULT CALLBACK Proc(HWND h, UINT m, WPARAM w, LPARAM l) {
    switch (m) {
    case WM_TIMER:
        if (w == T_FRENTE) { KillTimer(h, T_FRENTE); AssentaFrente(); }
        else if (w == T_LOGO) { KillTimer(h, T_LOGO); if (!g_raised) Atualiza(); }
        else if (w == T_RESULT) {
            bool changed=CollectPreviews();
            if(!revelar::DesktopShown()&&!g_ocupado){
                std::vector<HWND> ready;
                for(auto it=g_pendingPark.begin();it!=g_pendingPark.end();){
                    const Jan* j=AchaJan(it->first);
                    if(!j||!IsWindow(it->first)||NoPalco(j->app)||it->first==DonoRaiz(GetForegroundWindow())){
                        it=g_pendingPark.erase(it);continue;
                    }
                    if(!g_jobs.count(it->first)||Agora()-it->second>=350)ready.push_back(it->first);
                    ++it;
                }
                for(HWND w:ready){Estaciona(w);g_pendingPark.erase(w);changed=true;}
            }
            if(changed)Pinta();
        }
        else if (w == T_CAPTURE) {
            KillTimer(h,T_CAPTURE);
            HWND fg=DonoRaiz(GetForegroundWindow());Jan j{};
            if(!g_ocupado && !Estacionada(fg) && JanelaDeApp(fg,fg,&j) && NoPalco(j.app))
                CapturePreview(fg);
        }
        else if (w == T_REFRESH) { if (!g_raised) Atualiza(); else Persegue(); }
        else if (w == T_BORDA) {  // janela grande por cima da faixa: o mouse na borda esquerda traz a faixa (como no Mac)
            POINT c; GetCursorPos(&c);
            RECT wa = AreaPrincipal(), sr; GetWindowRect(h, &sr);
            bool edge = c.x <= wa.left + 1 && c.y >= wa.top && c.y < wa.bottom && GetAsyncKeyState(VK_LBUTTON) >= 0;
            if (!g_raised && edge && !g_tiles.empty()) { g_raised = true; SetWindowPos(h, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE); }
            else if (g_raised && !PtInRect(&sr, c)) { g_raised = false; SetWindowPos(h, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE); PlaceStrip(); }
        }
        return 0;
    case WM_NCHITTEST: {
        POINT p{ (short)LOWORD(l), (short)HIWORD(l) }; ScreenToClient(h, &p);
        return HitTile(MAKELPARAM(p.x, p.y)) >= 0 ? HTCLIENT : HTTRANSPARENT;
    }
    case WM_MOUSEACTIVATE: return MA_NOACTIVATE;
    case WM_LBUTTONDOWN:{
        g_pressed=HitTile(l);g_dragGroup=false;
        if(g_pressed>=0){GetCursorPos(&g_pressPoint);SetCapture(h);}
        return 0;
    }
    case WM_CAPTURECHANGED:g_pressed=-1;g_dragGroup=false;return 0;
    case WM_MOUSEMOVE: {
        if(g_pressed>=0){POINT p;GetCursorPos(&p);g_dragGroup=g_dragGroup||std::abs(p.x-g_pressPoint.x)>8||std::abs(p.y-g_pressPoint.y)>8;}
        int i = HitTile(l);
        if (i != g_hover) { g_hover = i; Pinta(); }
        TRACKMOUSEEVENT t{ sizeof(t), TME_LEAVE, h, 0 }; TrackMouseEvent(&t);
        return 0;
    }
    case WM_MOUSELEAVE: if (g_hover >= 0) { g_hover = -1; Pinta(); } return 0;
    case WM_LBUTTONUP: {  // o cartão vai para o palco; com Shift, entra junto com o que já está (dois apps lado a lado)
        int i=HitTile(l);
        bool add=false;
        if(g_dragGroup&&g_pressed>=0){POINT p;GetCursorPos(&p);RECT sr{};GetWindowRect(h,&sr);add=p.x>sr.right&&([&]{RECT wa=AreaPrincipal();return PtInRect(&wa,p);})();i=add?g_pressed:-1;}
        g_pressed=-1;g_dragGroup=false;if(GetCapture()==h)ReleaseCapture();
        if (i < 0 || i >= (int)g_tiles.size() || g_tiles[i].grupo >= (int)g_grupos.size()) return 0;
        Grupo g = g_grupos[g_tiles[i].grupo];
        std::vector<std::wstring> prox = g;
        if (add || (w & MK_SHIFT)) { prox = g_palco; for (auto& a : g) if (!NoPalco(a)) prox.push_back(a); }
        if(g_raised){g_raised=false;SetWindowPos(h,HWND_NOTOPMOST,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE);}
        TrocaPalco(prox, g_tiles[i].win, true);
        return 0;
    }
    case WM_DPICHANGED: case WM_DISPLAYCHANGE: Atualiza(); return 0;
    }
    return DefWindowProcW(h, m, w, l);
}

void TurnOn() {
    if (g_on) return;
    HINSTANCE hi = GetModuleHandleW(nullptr);
    WNDCLASSW wc{}; wc.lpfnWndProc = Proc; wc.hInstance = hi; wc.lpszClassName = L"ShellOrganizador"; wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    RegisterClassW(&wc);
    g_strip = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_NOREDIRECTIONBITMAP, wc.lpszClassName, L"", WS_POPUP, 0, 0, 10, 10, nullptr, nullptr, hi, nullptr);
    if (!g_strip) return;
    g_s.hwnd = g_strip;g_lastPaint.clear();
    apps::ours.push_back(g_strip);
    g_on = true;
    g_palco.clear(); g_grupos.clear(); g_recentes.clear(); g_acima.clear(); g_vista.clear(); g_unparkable.clear(); g_raised = false;
    // o palco começa com o app da frente (Init); os outros vão para a faixa no primeiro refresh
    Lista();
    if (const Jan* j = AchaJan(DonoRaiz(GetForegroundWindow()))) g_palco = { j->app };
    else if(!g_jan.empty()) g_palco={g_jan.front().app};
    g_trocouEm = 0;
    for (auto& [h, t] : g_vista) t = 0;  // as que já estavam abertas podem ir para a faixa já
    g_hookFrente = SetWinEventHook(EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_FOREGROUND, nullptr, NaFrente, 0, 0, WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);
    g_hookObj = SetWinEventHook(EVENT_OBJECT_DESTROY, EVENT_OBJECT_HIDE, nullptr, NoObjeto, 0, 0, WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);
    SetTimer(g_strip, T_REFRESH, 1500, nullptr);
    SetTimer(g_strip,T_RESULT,60,nullptr);
    SetTimer(g_strip, T_BORDA, 100, nullptr);
    Log(L"organizador: ligado (motor do stage-manager-for-windows: janelas estacionadas fora da tela)");
    Atualiza();
    SetTimer(g_strip,T_CAPTURE,300,nullptr);
}

void TurnOff() {
    if (!g_on) return;
    g_on = false;
    if (g_hookFrente) { UnhookWinEvent(g_hookFrente); g_hookFrente = nullptr; }
    if (g_hookObj) { UnhookWinEvent(g_hookObj); g_hookObj = nullptr; }
    ClearThumbs();g_jobs.clear();g_captureAt.clear();g_pendingPark.clear();
    for(auto& [h,b]:g_previews)if(b)b->Release();g_previews.clear();
    for (auto& [id, b] : g_icons) if (b) b->Release();
    g_icons.clear();
    ui::GpuFree(g_s);
    if (g_strip) { apps::ours.erase(std::remove(apps::ours.begin(), apps::ours.end(), g_strip), apps::ours.end()); DestroyWindow(g_strip); g_strip = nullptr; }
    size_t n = g_vagas.size();
    TrazTodas();  // todas as estacionadas em casa, atrás da da frente
    g_palco.clear(); g_grupos.clear(); g_recentes.clear(); g_tiles.clear(); g_jan.clear();g_lastPaint.clear();
    Log(L"organizador: desligado (%zu janelas de volta)", n);
}

}  // namespace

bool palco::On() { return g_on; }
void palco::Sync() {
    if (shell::Setting(L"organizador", L"0") == L"1") TurnOn(); else TurnOff();
    mesa::StageVisibilityChanged();widgets::StageVisibilityChanged();
}
void palco::Toggle() { shell::SetSetting(L"organizador", g_on ? L"0" : L"1"); Sync(); }
void palco::Stop() { TurnOff(); }
bool palco::Estacionada(HWND h) { return g_vagas.count(h) != 0; }

// O Shell abrindo: as janelas que uma execução anterior deixou estacionadas (o Shell caiu ou foi encerrado à força)
// voltam para casa, pelo diário. Só as que ainda estão na vaga ou fora da tela (o app pode tê-las trazido sozinho).
void palco::Recupera() {
    std::wstring f = Diario();
    FILE* fp = nullptr;
    if (_wfopen_s(&fp, f.c_str(), L"r") || !fp) return;
    unsigned long long hv; long hx, hy, sx, sy; int n = 0;
    while (fscanf_s(fp, "%llu %ld %ld %ld %ld", &hv, &hx, &hy, &sx, &sy) == 5) {
        HWND h = (HWND)(ULONG_PTR)hv;
        if (!IsWindow(h)) continue;
        RECT r = Onde(h);
        if ((r.left != sx || r.top != sy) && !QuaseFora(r)) continue;
        RECT casa{ hx, hy, hx + (r.right - r.left), hy + (r.bottom - r.top) };
        casa = NaMesa(casa);
        Por(h, { casa.left, casa.top }, VOLTAR);
        RedrawWindow(h, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_FRAME | RDW_ALLCHILDREN);
        n++;
    }
    fclose(fp);
    DeleteFileW(f.c_str());
    if (n) Log(L"organizador: %d janelas estacionadas por uma execução anterior voltaram para a tela", n);
}
