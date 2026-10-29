#include "mods.h"

#include "config.h"
#include "game.h"
#include "hook.h"
#include "il2cpp.h"
#include "log.h"

using namespace il2cpp;

namespace mods {
namespace {

void (*o_camera_tick)(void*) = nullptr;
void (*o_fire)(void*, void*) = nullptr;
void (*o_rays)(void*, void*, void*, long) = nullptr;
void (*o_player_update)(void*) = nullptr;
int g_installed = 0;

// ios.lxcq() — кадровый тик камеры локального игрока, игровой тред
void hk_camera_tick(void* self) {
    game::tick(self);
    if (o_camera_tick) o_camera_tick(self);
}

// ake.opmn() — апдейт игрока
void hk_player_update(void* self) {
    game::note_player(self);
    if (o_player_update) o_player_update(self);
}

// jxy.vrsg(ref WeaponContext) — выстрел.
// Режим 0: на время каста доворачиваем WeaponContext.HitTransform на цель и возвращаем как было.
// Камера не трогается, поэтому для себя и для других ствол смотрит туда же, куда и был.
void hk_fire(void* self, void* wctx) {
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

    if (o_fire) o_fire(self, wctx);

    if (patched) transform_set_euler(hit_tf, saved);
}

// jxy.rxjx(ref WeaponContext, Span<Ray>) — заполнение лучей хит-каста.
// Режим 1: после оригинала переписываем направления — мимо разброса и мимо отдачи.
void hk_rays(void* self, void* wctx, void* span_ptr, long span_len) {
    if (o_rays) o_rays(self, wctx, span_ptr, span_len);

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

} // namespace mods
