#include "antiaim.h"

#include <windows.h>

#include <cmath>

#include "config.h"
#include "sdk.h"
#include "sigs.h"

namespace antiaim {
namespace {

Vec3 g_real{};          // настоящий угол игрока, его возвращаем камере
Vec3 g_last_fake{};
bool g_have_real = false;
float g_yaw_to_target = 0.f;
bool g_target = false;

float now_s() {
    static LARGE_INTEGER freq{};
    if (!freq.QuadPart) QueryPerformanceFrequency(&freq);
    LARGE_INTEGER c{};
    QueryPerformanceCounter(&c);
    return (float)((double)c.QuadPart / (double)freq.QuadPart);
}

float pitch_for_mode() {
    float p;
    switch (g_cfg.aa_pitch_mode) {
        case 1: p = -89.f; break;             // вверх
        case 2: p = 89.f; break;              // вниз
        case 3: p = 0.f; break;               // ровно
        case 4: {                             // прыгающий
            static int f = 0;
            p = (++f & 1) ? 89.f : -89.f;
            break;
        }
        default: p = g_cfg.aa_pitch; break;   // своё значение
    }
    if (g_cfg.match_safe) {                   // ±89 в демке читается глазами — держим коридор
        const float c = g_cfg.aa_pitch_cap;
        if (p > c) p = c; else if (p < -c) p = -c;
    }
    return p;
}

float yaw_for_mode(float base) {
    const float t = now_s();
    int mode = g_cfg.aa_mode;
    if (g_cfg.match_safe && mode == 3) mode = 4;   // спин — инста-палево в мм, вместо него sway
    switch (mode) {
        case 1:                                // static
            return base + g_cfg.aa_yaw;
        case 2: {                              // jitter
            static int side = 0;
            side ^= 1;
            return base + g_cfg.aa_yaw + (side ? g_cfg.aa_jitter : -g_cfg.aa_jitter);
        }
        case 3:                                // spin
            return base + std::fmod(t * g_cfg.aa_spin_speed, 360.f);
        case 4: {                              // sway — медленная синусоида, менее палевно в демке
            return base + g_cfg.aa_yaw + std::sin(t * 2.2f) * g_cfg.aa_jitter;
        }
        default:
            return base;
    }
}

} // namespace

bool via_usercmd() { return sigs::g.usercmd_ready(); }

bool enabled() {
    if (g_cfg.aa_mode == 0) return false;
    return config::bind_active(g_cfg.aa_bind, g_cfg.aa_on);
}

Vec3 fake_angles(const Vec3& real, float yaw_to_target) {
    // за базу берём либо свой взгляд, либо направление на врага: так спина
    // всегда повёрнута к тому, кто стреляет
    float base = (g_cfg.aa_at_target && g_target) ? yaw_to_target : real.y;
    Vec3 out{pitch_for_mode(), norm_angle(yaw_for_mode(base + 180.f)), 0.f};
    if (g_cfg.match_safe) {                    // режем угловую скорость до человеческой
        const float cap = g_cfg.aa_max_delta;
        const float dy = norm_angle(out.y - g_last_fake.y);
        if (dy > cap) out.y = norm_angle(g_last_fake.y + cap);
        else if (dy < -cap) out.y = norm_angle(g_last_fake.y - cap);
    }
    g_last_fake = out;
    return out;
}

void run(const Vec3& eye, bool has_target, const Vec3& target_point) {
    g_target = has_target;
    if (has_target) g_yaw_to_target = calc_angle(eye, target_point).y;

    if (!enabled()) {
        // выключили на ходу — вернуть камеру туда, где она была до спина
        if (g_have_real && !via_usercmd()) {
            sdk::set_view_angles(g_real);
            g_have_real = false;
        }
        return;
    }

    if (via_usercmd()) return;                 // угол подменит хук, камеру не трогаем

    // real-режим: запоминаем настоящий угол один раз, дальше крутим от него
    Vec3 cur = sdk::view_angles();
    if (!g_have_real) {
        g_real = cur;
        g_have_real = true;
    }
    if (g_cfg.aa_follow_mouse) {
        // игрок довернул мышкой — принимаем это как новый настоящий угол
        float d = std::fabs(norm_angle(cur.y - g_last_fake.y));
        if (d > 1.f && d < 179.f) g_real = cur;
    }
    sdk::set_view_angles(fake_angles(g_real, g_yaw_to_target));
}

void on_usercmd(void* usercmd) {
    if (!usercmd || !enabled() || !via_usercmd()) return;
    const sigs::Table& s = sigs::g;

    uintptr_t pb = mem::ptr((uintptr_t)usercmd + (uintptr_t)s.usercmd_pb);
    if (!pb) pb = (uintptr_t)usercmd + (uintptr_t)s.usercmd_pb;   // inline, не указатель
    uintptr_t base = mem::ptr(pb + (uintptr_t)s.pb_base);
    if (!base) base = pb + (uintptr_t)s.pb_base;
    uintptr_t qa = mem::ptr(base + (uintptr_t)s.base_viewangles);
    if (!qa) return;

    Vec3 real = sdk::view_angles();
    Vec3 fake = fake_angles(real, g_yaw_to_target);
    mem::write<float>(qa + (uintptr_t)s.qangle_x + 0, fake.x);
    mem::write<float>(qa + (uintptr_t)s.qangle_x + 4, fake.y);
    mem::write<float>(qa + (uintptr_t)s.qangle_x + 8, 0.f);
}

} // namespace antiaim
