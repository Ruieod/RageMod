#include "mods.h"

#include <mutex>
#include <cstring>

#include "config.h"
#include "game.h"
#include "hook.h"
#include "il2cpp.h"
#include "log.h"

using namespace il2cpp;

namespace mods {
namespace {

void (*o_camera_tick)(void*, void*) = nullptr;
void (*o_fire)(void*, void*, void*) = nullptr;
void (*o_rays)(void*, void*, void*, long, void*) = nullptr;
void (*o_player_update)(void*, void*) = nullptr;
void (*o_recoil)(void*, bool, void*) = nullptr;
int g_installed = 0;

// Axlebolt.Standoff.Inputs.ButtonInput: 3 байта, возвращается в x0
struct ButtonIn {
    uint8_t pressed;
    uint8_t released;
    uint8_t down;
};
ButtonIn (*o_get_button)(void*, void*, void*) = nullptr;

constexpr int kSeenActions = 32;
char g_actions[kSeenActions][32];
int g_actions_n = 0;
std::mutex g_actions_mu;

void note_action(const char* name) {
    std::lock_guard<std::mutex> lk(g_actions_mu);
    for (int i = 0; i < g_actions_n; i++) {
        if (std::strcmp(g_actions[i], name) == 0) return;
    }
    if (g_actions_n >= kSeenActions) return;
    std::snprintf(g_actions[g_actions_n], sizeof(g_actions[0]), "%s", name);
    g_actions_n++;
}

bool icontains(const char* hay, const char* needle) {
    if (!needle[0]) return false;
    size_t nl = std::strlen(needle);
    for (const char* p = hay; *p; p++) {
        size_t i = 0;
        while (i < nl && p[i] && (p[i] | 32) == (needle[i] | 32)) i++;
        if (i == nl) return true;
    }
    return false;
}

// ios.lxcq() — кадровый тик камеры локального игрока, игровой тред
void hk_camera_tick(void* self, void* method) {
    game::tick(self);
    if (o_camera_tick) o_camera_tick(self, method);
    game::post_tick(self); // третье лицо пишем после оригинала, иначе он перезатрёт
}

// znn.rpcp(string) -> ButtonInput — игра спрашивает состояние кнопки по имени экшена.
// Автострельба: подмешиваем нажатие в ответ на запрос экшена выстрела.
ButtonIn hk_get_button(void* self, void* name, void* method) {
    ButtonIn in = o_get_button ? o_get_button(self, name, method) : ButtonIn{0, 0, 0};

    char buf[32];
    if (il2cpp::string_to_ascii(name, buf, sizeof(buf)) <= 0) return in;
    note_action(buf);
    if (!g_cfg.autofire || !icontains(buf, g_cfg.fire_action)) return in;

    bool want = g_cfg.autofire_mode == 0 || game::trigger_ready();
    if (!want) return in;

    if (g_cfg.autofire_pulse) {
        static bool phase = false;
        phase = !phase;
        in.down = phase ? 1 : 0;
        in.pressed = phase ? 1 : 0;
        in.released = phase ? 0 : 1;
    } else {
        in.down = 1;
        in.pressed = 1;
        in.released = 0;
    }
    return in;
}

// vez.lhgb(ref WeaponContext, bool) — пересчёт отдачи. Гасим состояние после оригинала,
// LastShotTime не трогаем: по нему считается темп стрельбы.
void hk_recoil(void* wctx, bool flag, void* method) {
    if (o_recoil) o_recoil(wctx, flag, method);
    if (!g_cfg.no_recoil || !readable(wctx, 0x98)) return;

    char* c = (char*)wctx;
    *(float*)(c + off::fld::wctx_recoil_ctx) = g_cfg.recoil_mult;

    char* r = c + off::fld::wctx_recoil;
    *(float*)(r + off::fld::rst_progress) = 0.f;
    *(float*)(r + off::fld::rst_prev_progress) = 0.f;
    std::memset(r + off::fld::rst_prev_point, 0, 8);   // Vector2
    std::memset(r + off::fld::rst_point, 0, 8);        // Vector2
    *(float*)(r + off::fld::rst_approach_delta) = 0.f;
}

// ake.opmn() — апдейт игрока
void hk_player_update(void* self, void* method) {
    game::note_player(self);
    if (o_player_update) o_player_update(self, method);
}

// jxy.vrsg(ref WeaponContext) — выстрел.
// Режим 0: на время каста доворачиваем WeaponContext.HitTransform на цель и возвращаем как было.
// Камера не трогается, поэтому для себя и для других ствол смотрит туда же, куда и был.
void hk_fire(void* self, void* wctx, void* method) {
    bool patched = false;
    void* hit_tf = nullptr;
    Vec3 saved{};

    if (g_cfg.aim_silent && g_cfg.aim_silent_mode == 0 && readable(wctx, 0x98)) {
        game::AimTarget t = game::current_target();
        if (t.ok) {
            bool ok = false;
            Vec3 point = game::aim_point(t.ent, &ok);
            hit_tf = ptr(wctx, off::fld::wctx_hit_tf);
            if (ok && readable(hit_tf, 0x20)) {
                Vec3 from = transform_position(hit_tf);
                if (!from.bad()) {
                    saved = transform_euler(hit_tf);
                    transform_set_euler(hit_tf, dir_to_euler(point - from));
                    patched = true;
                }
            }
        }
    }

    if (o_fire) o_fire(self, wctx, method);

    if (patched) transform_set_euler(hit_tf, saved);
}

// jxy.rxjx(ref WeaponContext, Span<Ray>) — заполнение лучей хит-каста.
// Режим 1: после оригинала переписываем направления — мимо разброса и мимо отдачи.
void hk_rays(void* self, void* wctx, void* span_ptr, long span_len, void* method) {
    if (o_rays) o_rays(self, wctx, span_ptr, span_len, method);

    if (!g_cfg.aim_silent || g_cfg.aim_silent_mode != 1) return;
    int n = (int)(span_len & 0xffffffff);
    if (n <= 0 || n > 64) return;
    Ray* rays = (Ray*)span_ptr;
    if (!readable(rays, sizeof(Ray) * (size_t)n)) return;

    game::AimTarget t = game::current_target();
    if (!t.ok) return;
    bool ok = false;
    Vec3 point = game::aim_point(t.ent, &ok);
    if (!ok) return;

    for (int i = 0; i < n; i++) {
        Vec3 dir = (point - rays[i].origin).norm();
        if (dir.bad()) continue;
        rays[i].direction = dir;
    }
}

} // namespace

bool install() {
    struct Entry {
        const char* name;
        uintptr_t rva;
        void* detour;
        void** orig;
    };
    const Entry entries[] = {
        {"ios.lxcq (camera tick)", off::hook_rva::CameraPart_tick, (void*)hk_camera_tick, (void**)&o_camera_tick},
        {"jxy.vrsg (fire)",        off::hook_rva::HitCast_fire,    (void*)hk_fire,        (void**)&o_fire},
        {"jxy.rxjx (rays)",        off::hook_rva::HitCast_rays,    (void*)hk_rays,        (void**)&o_rays},
        {"ake.opmn (player)",      off::hook_rva::Player_update,   (void*)hk_player_update, (void**)&o_player_update},
        {"znn.rpcp (input)",       off::hook_rva::Input_get_button, (void*)hk_get_button,  (void**)&o_get_button},
        {"vez.lhgb (recoil)",      off::hook_rva::Recoil_apply,    (void*)hk_recoil,      (void**)&o_recoil},
    };

    g_installed = 0;
    for (const Entry& e : entries) {
        if (hook::inlined((void*)at(e.rva), e.detour, e.orig)) {
            g_installed++;
        } else {
            LOGE("hook failed: %s", e.name);
        }
    }
    LOGI("features: %d/%d hooks", g_installed, (int)(sizeof(entries) / sizeof(entries[0])));
    return g_installed > 0;
}

int installed_count() { return g_installed; }

int seen_action_count() {
    std::lock_guard<std::mutex> lk(g_actions_mu);
    return g_actions_n;
}

const char* seen_action(int i) {
    std::lock_guard<std::mutex> lk(g_actions_mu);
    return (i >= 0 && i < g_actions_n) ? g_actions[i] : "";
}

} // namespace mods
