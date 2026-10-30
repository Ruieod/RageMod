// Рендер меню: подменяем Present в vtable свопчейна DXGI. Инлайн-патча кода нет,
// значит нечем светиться в сканах секций и не нужен дизассемблер длин.
#include <d3d11.h>
#include <dxgi.h>
#include <windows.h>

#include "backends/imgui_impl_dx11.h"
#include "backends/imgui_impl_win32.h"
#include "config.h"
#include "game.h"
#include "imgui.h"
#include "mem.h"
#include "sdk.h"
#include "ui.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

namespace render {
namespace {

using PresentFn = HRESULT(__stdcall*)(IDXGISwapChain*, UINT, UINT);
using ResizeFn = HRESULT(__stdcall*)(IDXGISwapChain*, UINT, UINT, UINT, DXGI_FORMAT, UINT);

PresentFn o_present = nullptr;
ResizeFn o_resize = nullptr;
WNDPROC o_wndproc = nullptr;

ID3D11Device* g_device = nullptr;
ID3D11DeviceContext* g_ctx = nullptr;
ID3D11RenderTargetView* g_rtv = nullptr;
HWND g_hwnd = nullptr;
bool g_ready = false;
bool g_style_done = false;
float g_fps = 0.f;
volatile long g_present_calls = 0;   // >0 => хук Present живой (игра на dx11)

struct FindCtx {
    DWORD pid;
    HWND hwnd;
};

BOOL CALLBACK enum_windows(HWND hwnd, LPARAM lp) {
    FindCtx* c = (FindCtx*)lp;
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (pid != c->pid || !IsWindowVisible(hwnd) || GetWindow(hwnd, GW_OWNER)) return TRUE;
    c->hwnd = hwnd;
    return FALSE;
}

HWND game_window() {
    HWND h = FindWindowA("SDL_app", nullptr);
    if (h) return h;
    FindCtx c{GetCurrentProcessId(), nullptr};
    EnumWindows(enum_windows, (LPARAM)&c);
    return c.hwnd;
}

void make_rtv(IDXGISwapChain* sc) {
    ID3D11Texture2D* back = nullptr;
    if (FAILED(sc->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&back)) || !back) return;
    g_device->CreateRenderTargetView(back, nullptr, &g_rtv);
    back->Release();
}

LRESULT CALLBACK hk_wndproc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_KEYDOWN && (int)wp == g_cfg.menu_key) {
        ui::toggle_menu();
        return 0;
    }
    if (g_ready) {
        ImGui_ImplWin32_WndProcHandler(hwnd, msg, wp, lp);
        // пока меню открыто — ввод в игру не пускаем
        if (ui::capturing_input() && msg >= WM_MOUSEFIRST && msg <= WM_MOUSELAST) return 1;
        if (ui::capturing_input() && (msg == WM_KEYDOWN || msg == WM_KEYUP || msg == WM_CHAR)) return 1;
    }
    return CallWindowProcA(o_wndproc, hwnd, msg, wp, lp);
}

void init_imgui(IDXGISwapChain* sc) {
    // привязываемся только к свопчейну с реальным видимым окном игры:
    // cs2 держит и побочные свопчейны, инит на них рисует в никуда
    DXGI_SWAP_CHAIN_DESC desc{};
    if (FAILED(sc->GetDesc(&desc))) return;
    HWND wnd = desc.OutputWindow;
    if (!wnd || !IsWindowVisible(wnd)) {
        HWND gw = game_window();
        if (!gw) return;              // окна ещё нет — ждём следующий кадр
        wnd = gw;
    }

    if (FAILED(sc->GetDevice(__uuidof(ID3D11Device), (void**)&g_device)) || !g_device) return;
    g_device->GetImmediateContext(&g_ctx);
    g_hwnd = wnd;

    make_rtv(sc);

    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.LogFilename = nullptr;
    ImGui_ImplWin32_Init(g_hwnd);
    ImGui_ImplDX11_Init(g_device, g_ctx);

    o_wndproc = (WNDPROC)SetWindowLongPtrA(g_hwnd, GWLP_WNDPROC, (LONG_PTR)hk_wndproc);
    g_ready = true;
}

HRESULT __stdcall hk_present(IDXGISwapChain* sc, UINT interval, UINT flags) {
    g_present_calls++;
    if (!g_ready) init_imgui(sc);
    if (!g_ready) return o_present(sc, interval, flags);
    if (!g_rtv) make_rtv(sc);                       // flip-model: добираем бэкбуфер
    if (!g_rtv) return o_present(sc, interval, flags);

    ImGuiIO& io = ImGui::GetIO();
    if (!g_style_done || (int)(g_cfg.menu_scale * 100.f) != (int)(io.FontGlobalScale * 100.f)) {
        ui::init_style(io.DisplaySize.y > 1.f ? io.DisplaySize.y : 1080.f);
        ImGui_ImplDX11_InvalidateDeviceObjects();
        io.FontGlobalScale = 1.f;
        g_style_done = true;
    }

    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    float dt = io.DeltaTime > 0.f ? io.DeltaTime : 1.f / 60.f;
    g_fps = g_fps * 0.9f + (1.f / dt) * 0.1f;

    int w = (int)io.DisplaySize.x, h = (int)io.DisplaySize.y;
    game::tick(w, h);
    ui::frame(w, h, g_fps);

    ImGui::Render();
    g_ctx->OMSetRenderTargets(1, &g_rtv, nullptr);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    return o_present(sc, interval, flags);
}

HRESULT __stdcall hk_resize(IDXGISwapChain* sc, UINT count, UINT w, UINT h, DXGI_FORMAT fmt, UINT flags) {
    if (g_rtv) {
        g_rtv->Release();
        g_rtv = nullptr;
    }
    HRESULT hr = o_resize(sc, count, w, h, fmt, flags);
    if (g_device) make_rtv(sc);
    return hr;
}

// болванка свопчейна, чтобы достать адрес vtable
bool dummy_vtable(void*** out) {
    WNDCLASSEXA wc{sizeof(wc)};
    wc.lpfnWndProc = DefWindowProcA;
    wc.hInstance = GetModuleHandleA(nullptr);
    wc.lpszClassName = "rm_dummy";
    if (!RegisterClassExA(&wc)) return false;
    HWND hwnd = CreateWindowA(wc.lpszClassName, "", WS_OVERLAPPEDWINDOW, 0, 0, 16, 16, nullptr, nullptr,
                              wc.hInstance, nullptr);
    if (!hwnd) {
        UnregisterClassA(wc.lpszClassName, wc.hInstance);
        return false;
    }

    DXGI_SWAP_CHAIN_DESC sd{};
    sd.BufferCount = 1;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hwnd;
    sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    D3D_FEATURE_LEVEL want[] = {D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0};
    D3D_FEATURE_LEVEL got{};
    IDXGISwapChain* sc = nullptr;
    ID3D11Device* dev = nullptr;
    ID3D11DeviceContext* ctx = nullptr;
    HRESULT hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, want, 2,
                                               D3D11_SDK_VERSION, &sd, &sc, &dev, &got, &ctx);
    bool ok = SUCCEEDED(hr) && sc;
    if (ok) *out = *(void***)sc;
    if (sc) sc->Release();
    if (ctx) ctx->Release();
    if (dev) dev->Release();
    DestroyWindow(hwnd);
    UnregisterClassA(wc.lpszClassName, wc.hInstance);
    return ok;
}

} // namespace

long present_calls() { return g_present_calls; }
bool is_ready() { return g_ready; }
bool has_rtv() { return g_rtv != nullptr; }
void* dev_ptr() { return g_device; }
int disp_w() { return g_ready ? (int)ImGui::GetIO().DisplaySize.x : -1; }
int disp_h() { return g_ready ? (int)ImGui::GetIO().DisplaySize.y : -1; }

bool install() {
    void** vt = nullptr;
    if (!dummy_vtable(&vt) || !vt) return false;
    o_present = (PresentFn)mem::vmt_swap(vt, 8, (void*)hk_present);
    o_resize = (ResizeFn)mem::vmt_swap(vt, 13, (void*)hk_resize);
    return o_present != nullptr;
}

void uninstall() {
    void** vt = nullptr;
    if (o_present && dummy_vtable(&vt) && vt) {
        mem::vmt_swap(vt, 8, (void*)o_present);
        if (o_resize) mem::vmt_swap(vt, 13, (void*)o_resize);
    }
    if (g_hwnd && o_wndproc) SetWindowLongPtrA(g_hwnd, GWLP_WNDPROC, (LONG_PTR)o_wndproc);
    if (g_ready) {
        ImGui_ImplDX11_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
        g_ready = false;
    }
    if (g_rtv) {
        g_rtv->Release();
        g_rtv = nullptr;
    }
}

} // namespace render
