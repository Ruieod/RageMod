#include "sdk.h"

#include <cstdio>
#include <cstring>

namespace sdk {

uintptr_t g_client = 0;
uintptr_t g_engine = 0;

bool init() {
    g_client = (uintptr_t)GetModuleHandleA("client.dll");
    g_engine = (uintptr_t)GetModuleHandleA("engine2.dll");
    return g_client && g_engine;
}

uintptr_t entity_list() { return mem::ptr(g_client + off::client_dll::dwEntityList); }

int highest_index() {
    uintptr_t sys = mem::ptr(g_client + off::client_dll::dwGameEntitySystem);
    if (!sys) return 0;
    int n = mem::read<int>(sys + off::client_dll::dwGameEntitySystem_highestEntityIndex);
    return (n > 0 && n < 4096) ? n : 64;
}

uintptr_t Entity::entity_by_index(int index) {
    uintptr_t list = entity_list();
    if (!list || index < 0) return 0;
    uintptr_t chunk = mem::ptr(list + 8 * (((uint32_t)index & 0x7FFF) >> 9) + 16);
    if (!chunk) return 0;
    return mem::ptr(chunk + 120 * ((uint32_t)index & 0x1FF));
}

uintptr_t Entity::entity_by_handle(uint32_t handle) {
    if (handle == 0xFFFFFFFF || handle == 0) return 0;
    return entity_by_index((int)(handle & 0x7FFF));
}

void Controller::name(char* out, int cap) const {
    if (!out || cap <= 0) return;
    out[0] = 0;
    if (!ptr) return;
    const char* src = (const char*)(ptr + off::CBasePlayerController::m_iszPlayerName);
    if (!mem::ok(src, 2)) return;
    int i = 0;
    for (; i < cap - 1; i++) {
        char c = src[i];
        if (c == 0) break;
        out[i] = (c >= 0x20 && c < 0x7f) ? c : '?';
    }
    out[i] = 0;
}

Controller local_controller() { return {mem::ptr(g_client + off::client_dll::dwLocalPlayerController)}; }

Entity local_pawn() { return {mem::ptr(g_client + off::client_dll::dwLocalPlayerPawn)}; }

Vec3 view_angles() { return mem::read<Vec3>(g_client + off::client_dll::dwViewAngles); }

void set_view_angles(const Vec3& a) {
    if (a.bad()) return;
    Vec3 c{norm_angle(a.x > 89.f ? 89.f : (a.x < -89.f ? -89.f : a.x)), norm_angle(a.y), 0.f};
    mem::write(g_client + off::client_dll::dwViewAngles, c);
}

bool world_to_screen(const Vec3& w, Vec2* out, int sw, int sh) {
    const float* m = (const float*)(g_client + off::client_dll::dwViewMatrix);
    if (!mem::ok(m, 64) || !out) return false;
    float cw = m[12] * w.x + m[13] * w.y + m[14] * w.z + m[15];
    if (cw < 0.01f) return false;
    float x = m[0] * w.x + m[1] * w.y + m[2] * w.z + m[3];
    float y = m[4] * w.x + m[5] * w.y + m[6] * w.z + m[7];
    float inv = 1.f / cw;
    out->x = (float)sw * 0.5f + (float)sw * 0.5f * x * inv;
    out->y = (float)sh * 0.5f - (float)sh * 0.5f * y * inv;
    return true;
}

// Состояние кнопки в CS2: 0x5 — зажата, 0x4 — отпущена. Адрес самой кнопки
// отдаёт cs2-dumper (buttons::attack), поэтому ни хуков ввода, ни SendInput.
void press_attack(bool down) {
    mem::write<int>(g_client + off::buttons::attack + 0x8, down ? 0x5 : 0x4);
}

} // namespace sdk
