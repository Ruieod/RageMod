#include "config.h"

#include <windows.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

Config g_cfg;

namespace config {
namespace {

enum class Kind { Bool, Int, Float, Bind3 };
struct Field {
    const char* name;
    Kind kind;
    void* p;
};

#define F_B(f) {#f, Kind::Bool,  &g_cfg.f}
#define F_I(f) {#f, Kind::Int,   &g_cfg.f}
#define F_F(f) {#f, Kind::Float, &g_cfg.f}
#define F_K(f) {#f, Kind::Bind3, &g_cfg.f}

const Field kFields[] = {
    F_B(aim), F_K(aim_bind), F_F(aim_fov), F_I(aim_bone), F_B(aim_visible_only), F_B(aim_silent),
    F_F(aim_smooth), F_B(aim_auto_fire), F_B(aim_predict), F_F(aim_predict_scale),
    F_B(trigger), F_K(trigger_bind), F_I(trigger_delay_ms), F_B(trigger_team_check), F_B(trigger_scoped_only),
    F_B(bhop), F_K(bhop_bind),
    F_B(hitmarker), F_B(hitmarker_sound), F_F(hitmarker_size), F_B(killeffect),
    F_I(glow_type), F_B(glow_split_visible),
    F_B(esp), F_B(esp_box), F_B(esp_skeleton), F_B(esp_health), F_B(esp_name), F_B(esp_dist),
    F_B(esp_weapon), F_B(esp_team_check), F_B(esp_visible_only), F_F(esp_max_dist), F_B(glow),
    F_B(night), F_F(night_alpha), F_B(bright_players),
    F_B(skins), F_I(skin_paintkit), F_I(skin_seed), F_F(skin_wear), F_I(skin_stattrak),
    F_B(watermark), F_B(draw_fov), F_B(debug), F_F(menu_scale), F_I(menu_key),
};

#undef F_B
#undef F_I
#undef F_F
#undef F_K

std::string g_path;

} // namespace

const char* path() {
    if (g_path.empty()) {
        char buf[MAX_PATH]{};
        DWORD n = GetEnvironmentVariableA("APPDATA", buf, sizeof(buf));
        g_path = (n > 0 && n < sizeof(buf)) ? std::string(buf) + "\\ragemod_cs2.json" : "ragemod_cs2.json";
    }
    return g_path.c_str();
}

bool save() {
    FILE* f = fopen(path(), "w");
    if (!f) return false;
    fputs("{\n", f);
    const int n = (int)(sizeof(kFields) / sizeof(kFields[0]));
    for (int i = 0; i < n; i++) {
        const Field& fd = kFields[i];
        const char* tail = (i + 1 == n) ? "\n" : ",\n";
        switch (fd.kind) {
            case Kind::Bool: fprintf(f, "  \"%s\": %s%s", fd.name, *(bool*)fd.p ? "true" : "false", tail); break;
            case Kind::Int: fprintf(f, "  \"%s\": %d%s", fd.name, *(int*)fd.p, tail); break;
            case Kind::Float: fprintf(f, "  \"%s\": %.4f%s", fd.name, *(float*)fd.p, tail); break;
            case Kind::Bind3: {
                Bind* b = (Bind*)fd.p;
                fprintf(f, "  \"%s\": [%d, %d]%s", fd.name, b->key, b->mode, tail);
                break;
            }
        }
    }
    fputs("}\n", f);
    fclose(f);
    return true;
}

bool load() {
    FILE* f = fopen(path(), "r");
    if (!f) return false;
    std::string text;
    char buf[1024];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0) text.append(buf, n);
    fclose(f);

    for (const Field& fd : kFields) {
        std::string key = std::string("\"") + fd.name + "\"";
        size_t at = text.find(key);
        if (at == std::string::npos) continue;
        at = text.find(':', at + key.size());
        if (at == std::string::npos) continue;
        const char* v = text.c_str() + at + 1;
        while (*v == ' ' || *v == '\t') v++;
        switch (fd.kind) {
            case Kind::Bool: *(bool*)fd.p = (strncmp(v, "true", 4) == 0); break;
            case Kind::Int: *(int*)fd.p = atoi(v); break;
            case Kind::Float: *(float*)fd.p = (float)atof(v); break;
            case Kind::Bind3: {
                if (*v != '[') break;
                Bind* b = (Bind*)fd.p;
                b->key = atoi(v + 1);
                const char* c = strchr(v, ',');
                b->mode = c ? atoi(c + 1) : 0;
                break;
            }
        }
    }
    return true;
}

bool bind_active(Bind& b, bool base) {
    if (!base) return false;
    if (b.key == 0) return true;
    bool down = (GetAsyncKeyState(b.key) & 0x8000) != 0;
    if (b.mode == 0) return down;
    static bool prev[256]{};
    int k = b.key & 0xFF;
    if (down && !prev[k]) b.toggled = !b.toggled;
    prev[k] = down;
    return b.toggled;
}

const char* key_name(int vk) {
    static char buf[32];
    if (vk == 0) return "always";
    switch (vk) {
        case 0x01: return "mouse1";
        case 0x02: return "mouse2";
        case 0x04: return "mouse3";
        case 0x05: return "mouse4";
        case 0x06: return "mouse5";
        case VK_SHIFT: return "shift";
        case VK_CONTROL: return "ctrl";
        case VK_MENU: return "alt";
        case VK_SPACE: return "space";
        case VK_TAB: return "tab";
        case VK_INSERT: return "insert";
        default: break;
    }
    UINT sc = MapVirtualKeyA((UINT)vk, MAPVK_VK_TO_VSC);
    if (GetKeyNameTextA((LONG)(sc << 16), buf, sizeof(buf)) > 0) return buf;
    snprintf(buf, sizeof(buf), "vk%d", vk);
    return buf;
}

} // namespace config
