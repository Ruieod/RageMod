// RageMod // CS2 — внутренний чит, грузится любым инжектором в cs2.exe.
#include <windows.h>

#include "config.h"
#include "sdk.h"

namespace render {
bool install();
void uninstall();
} // namespace render

namespace {

HMODULE g_self = nullptr;

DWORD WINAPI init(LPVOID) {
    // ждём, пока игра поднимет свои модули
    for (int i = 0; i < 120 && !sdk::init(); i++) Sleep(500);
    if (!sdk::g_client) return 0;

    config::load();
    render::install();

    while (true) {
        if ((GetAsyncKeyState(VK_END) & 1) != 0) break;   // END — выгрузиться
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
