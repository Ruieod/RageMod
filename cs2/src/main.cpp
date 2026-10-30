// RageMod // CS2 — внутренний чит, грузится любым инжектором в cs2.exe.
#include <windows.h>

#include "config.h"
#include "sdk.h"
#include "sigs.h"

namespace render {
bool install();
void uninstall();
} // namespace render

namespace {

HMODULE g_self = nullptr;
bool g_wiped = false;

// Заголовок PE после инициализации нам не нужен (резолв импортов уже сделан),
// а сканеру по памяти он первым подсказывает «тут образ модуля». Нули поверх.
// Побочка: unwind по нашим кадрам при краше без заголовка не работает.
void wipe_headers() {
    DWORD old = 0;
    if (!VirtualProtect(g_self, 0x1000, PAGE_READWRITE, &old)) return;
    SecureZeroMemory(g_self, 0x1000);
    VirtualProtect(g_self, 0x1000, old, &old);
    g_wiped = true;
}

DWORD WINAPI init(LPVOID) {
    // ждём, пока игра поднимет свои модули
    for (int i = 0; i < 120 && !sdk::init(); i++) Sleep(500);
    if (!sdk::g_client) return 0;

    config::load();
    sigs::load();
    render::install();
    wipe_headers();

    while (true) {
        if ((GetAsyncKeyState(VK_END) & 1) != 0) break;   // END — выгрузиться
        Sleep(40 + (GetTickCount() & 31));
    }

    config::save();
    render::uninstall();
    Sleep(300);
    // без заголовка LdrUnloadDll не разберёт образ — хуки сняты, модуль остаётся инертным
    if (!g_wiped) FreeLibraryAndExitThread(g_self, 0);
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
