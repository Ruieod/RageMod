#include "config.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "log.h"

Config g_cfg;

namespace config {
namespace {

enum class Kind { Bool, Int, Float, Str };
struct Field {
    const char* name;
    Kind kind;
    void* p;
};

#define F_BOOL(f)  {#f, Kind::Bool,  &g_cfg.f}
#define F_INT(f)   {#f, Kind::Int,   &g_cfg.f}
#define F_FLOAT(f) {#f, Kind::Float, &g_cfg.f}
#define F_STR(f)   {#f, Kind::Str,   &g_cfg.f}

const Field kFields[] = {
    F_BOOL(esp),          F_BOOL(esp_box),        F_BOOL(esp_bones),       F_BOOL(esp_health),
    F_BOOL(esp_dist),     F_BOOL(esp_snaplines),  F_BOOL(esp_team_check),  F_BOOL(esp_visible_only),
    F_FLOAT(esp_max_dist),
    F_BOOL(aim_silent),   F_INT(aim_silent_mode), F_BOOL(aim_visible),     F_FLOAT(aim_smooth),
    F_INT(aim_bone),      F_BOOL(aim_fallback_body), F_FLOAT(aim_fov),     F_FLOAT(aim_max_dist),
    F_BOOL(aim_visible_check), F_BOOL(aim_team_check), F_INT(aim_quat),
    F_BOOL(autofire),     F_INT(autofire_mode),   F_FLOAT(autofire_fov),
    F_BOOL(autofire_pulse), F_STR(fire_action),
    F_BOOL(no_recoil),    F_FLOAT(recoil_mult),
    F_INT(chams),         F_INT(chams_queue),     F_BOOL(chams_team_check),
    F_BOOL(third_person), F_FLOAT(tp_dist),       F_FLOAT(tp_height), F_BOOL(tp_show_body),
    F_BOOL(draw_fov),     F_BOOL(draw_debug),
    F_INT(touch_source),  F_BOOL(menu_swallow),
    F_BOOL(touch_swap_xy), F_BOOL(touch_invert_x), F_BOOL(touch_invert_y),
};

#undef F_BOOL
#undef F_INT
#undef F_FLOAT
#undef F_STR

std::string g_path;

} // namespace

const char* path() {
    if (g_path.empty()) {
        // каталог приложения пишется под его uid без доп. прав
        const char* candidates[] = {
            "/data/data/com.axlebolt.standoff2/files",
            "/data/user/0/com.axlebolt.standoff2/files",
            "/data/local/tmp",
        };
        for (const char* dir : candidates) {
            std::string p = std::string(dir) + "/ragemod.json";
            FILE* f = fopen(p.c_str(), "ae");
            if (f) {
                fclose(f);
                g_path = p;
                break;
            }
        }
        if (g_path.empty()) g_path = "/data/local/tmp/ragemod.json";
    }
    return g_path.c_str();
}

bool save() {
    FILE* f = fopen(path(), "we");
    if (!f) {
        LOGE("config: save failed (%s)", path());
        return false;
    }
    fputs("{\n", f);
    for (size_t i = 0; i < sizeof(kFields) / sizeof(kFields[0]); i++) {
        const Field& fd = kFields[i];
        const char* tail = (i + 1 == sizeof(kFields) / sizeof(kFields[0])) ? "\n" : ",\n";
        switch (fd.kind) {
            case Kind::Bool:
                fprintf(f, "  \"%s\": %s%s", fd.name, *(bool*)fd.p ? "true" : "false", tail);
                break;
            case Kind::Int:
                fprintf(f, "  \"%s\": %d%s", fd.name, *(int*)fd.p, tail);
                break;
            case Kind::Float:
                fprintf(f, "  \"%s\": %.3f%s", fd.name, *(float*)fd.p, tail);
                break;
            case Kind::Str:
                fprintf(f, "  \"%s\": \"%s\"%s", fd.name, (const char*)fd.p, tail);
                break;
        }
    }
    fputs("}\n", f);
    fclose(f);
    LOGI("config: saved to %s", path());
    return true;
}

bool load() {
    FILE* f = fopen(path(), "re");
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
            case Kind::Bool:  *(bool*)fd.p = (strncmp(v, "true", 4) == 0 || *v == '1'); break;
            case Kind::Int:   *(int*)fd.p = atoi(v); break;
            case Kind::Float: *(float*)fd.p = (float)atof(v); break;
            case Kind::Str: {
                if (*v != '"') break;
                v++;
                char* dst = (char*)fd.p;
                int i = 0;
                while (*v && *v != '"' && i < 31) dst[i++] = *v++;
                dst[i] = 0;
                break;
            }
        }
    }
    LOGI("config: loaded %s", path());
    return true;
}

} // namespace config
