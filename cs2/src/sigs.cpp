#include "sigs.h"

#include <windows.h>
#include <shlobj.h>

#include <cstdio>
#include <cstring>

#include "scan.h"
#include "sdk.h"

namespace sigs {

Table g;

namespace {

char g_path[MAX_PATH] = {0};
char g_status[192] = "sigs: не загружены";

void base_dir(char* out, int cap) {
    char docs[MAX_PATH] = {0};
    if (SUCCEEDED(SHGetFolderPathA(nullptr, CSIDL_PERSONAL, nullptr, 0, docs))) {
        std::snprintf(out, (size_t)cap, "%s\\RageMod", docs);
        CreateDirectoryA(out, nullptr);
    } else {
        std::snprintf(out, (size_t)cap, ".");
    }
}

int ini_int(const char* key, int def) {
    return (int)GetPrivateProfileIntA("usercmd", key, def, ini_path());
}

} // namespace

const char* ini_path() {
    if (!g_path[0]) {
        char dir[MAX_PATH];
        base_dir(dir, sizeof(dir));
        std::snprintf(g_path, sizeof(g_path), "%s\\ragemod_sigs.ini", dir);
    }
    return g_path;
}

bool save_template() {
    FILE* f = std::fopen(ini_path(), "w");
    if (!f) return false;
    std::fprintf(f,
        "; RageMod // CS2 — то, чего нет в cs2-dumper.\n"
        "; Индекс берётся из ragemod_vtable.txt (кнопка Dump input vtable),\n"
        "; смещения — из дизассемблера client.dll. -1 = выключено.\n"
        "[usercmd]\n"
        "getusercmd_index=%d\n"
        "usercmd_pb=%d\n"
        "pb_base=%d\n"
        "base_viewangles=%d\n"
        "qangle_x=%d\n"
        "base_subtick=%d\n",
        g.getusercmd_index, g.usercmd_pb, g.pb_base, g.base_viewangles, g.qangle_x, g.base_subtick);
    std::fclose(f);
    return true;
}

bool load() {
    g.csgo_input = sdk::g_client ? mem::ptr(sdk::g_client + off::client_dll::dwCSGOInput) : 0;

    if (GetFileAttributesA(ini_path()) == INVALID_FILE_ATTRIBUTES) save_template();

    g.getusercmd_index = ini_int("getusercmd_index", -1);
    g.usercmd_pb       = ini_int("usercmd_pb", -1);
    g.pb_base          = ini_int("pb_base", -1);
    g.base_viewangles  = ini_int("base_viewangles", -1);
    g.qangle_x         = ini_int("qangle_x", -1);
    g.base_subtick     = ini_int("base_subtick", -1);

    if (!g.csgo_input) {
        std::snprintf(g_status, sizeof(g_status), "CCSGOInput не найден (dwCSGOInput пуст)");
    } else if (g.usercmd_ready()) {
        std::snprintf(g_status, sizeof(g_status), "usercmd готов: input=%p idx=%d", (void*)g.csgo_input,
                      g.getusercmd_index);
    } else {
        std::snprintf(g_status, sizeof(g_status), "input=%p, usercmd не настроен — silent/AA в real-режиме",
                      (void*)g.csgo_input);
    }
    return g.usercmd_ready();
}

int dump_input_vtable() {
    if (!g.csgo_input) g.csgo_input = sdk::g_client ? mem::ptr(sdk::g_client + off::client_dll::dwCSGOInput) : 0;
    if (!g.csgo_input) return 0;
    char dir[MAX_PATH], file[MAX_PATH];
    base_dir(dir, sizeof(dir));
    std::snprintf(file, sizeof(file), "%s\\ragemod_vtable.txt", dir);
    int n = scan::dump_vtable((const void*)g.csgo_input, 64, file);
    std::snprintf(g_status, sizeof(g_status), "vtable: %d строк -> %s", n, file);
    return n;
}

const char* status() { return g_status; }

} // namespace sigs
