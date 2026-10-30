// RageMod // CS2 — внутренний чит, грузится любым инжектором в cs2.exe.
#include <windows.h>

#include <cstdarg>
#include <cstdio>

#include "config.h"
#include "mem.h"
#include "sdk.h"
#include "sigs.h"

namespace render {
bool install();
void uninstall();
long present_calls();
bool is_ready();
bool has_rtv();
void* dev_ptr();
int disp_w();
int disp_h();
} // namespace render

namespace {

HMODULE g_self = nullptr;

// лог рядом с профилем: C:\Users\<user>\ragemod_debug.txt — не зависит от меню
void logf(const char* fmt, ...) {
    char path[MAX_PATH];
    DWORD n = GetEnvironmentVariableA("USERPROFILE", path, sizeof(path));
    if (!n || n >= sizeof(path)) return;
    std::snprintf(path + n, sizeof(path) - n, "\\ragemod_debug.txt");
    FILE* f = std::fopen(path, "a");
    if (!f) return;
    va_list ap;
    va_start(ap, fmt);
    std::vfprintf(f, fmt, ap);
    va_end(ap);
    std::fputc('\n', f);
    std::fclose(f);
}

void snapshot(const char* tag) {
    uintptr_t csgo = sdk::g_client ? mem::ptr(sdk::g_client + off::client_dll::dwCSGOInput) : 0;
    sdk::Controller ctrl = sdk::local_controller();
    sdk::Entity pawn = sdk::local_pawn();
    logf("[%s] present=%ld ready=%d rtv=%d dev=%p disp=%dx%d | localpawn=%p highest=%d csgoinput=%p",
         tag, render::present_calls(), render::is_ready(), render::has_rtv(), render::dev_ptr(),
         render::disp_w(), render::disp_h(),
         (void*)(pawn ? pawn.ptr : 0), sdk::highest_index(), (void*)csgo);
}

DWORD WINAPI init(LPVOID) {
    logf("---- inject ----");
    int tries = 0;
    for (; tries < 120 && !sdk::init(); tries++) Sleep(500);
    logf("sdk::init after %d tries: client=%p engine=%p", tries, (void*)sdk::g_client, (void*)sdk::g_engine);
    if (!sdk::g_client) { logf("client.dll не найден — выход"); return 0; }

    config::load();
    sigs::load();
    snapshot("load");

    bool rendered = render::install();
    logf("render::install -> %s", rendered ? "ok" : "FAILED (вулкан? хук не встал)");

    int n = sigs::dump_input_vtable();
    logf("auto dump_input_vtable -> %d строк (нужна игра с ботами, не меню)", n);

    Sleep(3000);
    snapshot("after3s");   // present_calls>0 => хук Present живой (dx11); 0 => вулкан

    // F8 — переснять дамп (зайди в игру, потом жми); F7 — снапшот; END — выгрузка
    while (true) {
        if ((GetAsyncKeyState(VK_END) & 1) != 0) break;
        if ((GetAsyncKeyState(VK_F8) & 1) != 0) {
            int m = sigs::dump_input_vtable();
            logf("F8 dump_input_vtable -> %d строк", m);
            snapshot("F8");
        }
        if ((GetAsyncKeyState(VK_F7) & 1) != 0) snapshot("F7");
        Sleep(50);
    }

    config::save();
    render::uninstall();
    Sleep(300);
    FreeLibraryAndExitThread(g_self, 0);
    return 0;
}

} // namespace

BOOL APIENTRY DllMain(HMODULE mod, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        g_self = mod;
        DisableThreadLibraryCalls(mod);
        CreateThread(nullptr, 0, init, nullptr, 0, nullptr);
    }
    return TRUE;
}
