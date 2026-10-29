#include "il2cpp.h"

#include <time.h>
#include <dlfcn.h>
#include <pthread.h>
#include <unistd.h>

#include <algorithm>
#include <cstdio>
#include <mutex>
#include <vector>

#include "log.h"

// ---- математика ----
float norm_angle(float a) {
    while (a > 180.f) a -= 360.f;
    while (a < -180.f) a += 360.f;
    return a;
}

Vec3 dir_to_euler(const Vec3& d) {
    Vec3 n = d.norm();
    float yaw = std::atan2(n.x, n.z) * 57.2957795f;
    float pitch = -std::asin(n.y) * 57.2957795f;
    return {pitch, yaw, 0.f};
}

Quat euler_to_quat(const Vec3& e) {
    const float k = 0.0087266462f; // pi/180/2
    float cp = std::cos(e.x * k), sp = std::sin(e.x * k);
    float cy = std::cos(e.y * k), sy = std::sin(e.y * k);
    float cr = std::cos(e.z * k), sr = std::sin(e.z * k);
    Quat q;
    q.x = sp * cy * cr + cp * sy * sr;
    q.y = cp * sy * cr - sp * cy * sr;
    q.z = cp * cy * sr - sp * sy * cr;
    q.w = cp * cy * cr + sp * sy * sr;
    return q;
}

float angle_between(const Vec3& a, const Vec3& b) {
    float d = a.norm().dot(b.norm());
    d = d > 1.f ? 1.f : (d < -1.f ? -1.f : d);
    return std::acos(d) * 57.2957795f;
}

namespace il2cpp {
namespace {

uintptr_t g_base = 0;

// карта читаемых диапазонов: managed-указатели живут не вечно, перед разыменованием сверяемся
struct Range { uintptr_t lo, hi; };
std::vector<Range> g_ranges;
std::mutex g_ranges_mu;
int64_t g_ranges_ts = 0;

int64_t now_ms() {
    struct timespec ts{};
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

void refresh_ranges_locked() {
    FILE* f = fopen("/proc/self/maps", "re");
    if (!f) return;
    std::vector<Range> out;
    out.reserve(1024);
    char line[512];
    while (fgets(line, sizeof(line), f)) {
        uintptr_t lo = 0, hi = 0;
        char perms[8] = {0};
        if (sscanf(line, "%lx-%lx %7s", &lo, &hi, perms) != 3) continue;
        if (perms[0] != 'r') continue;
        if (!out.empty() && out.back().hi == lo) {
            out.back().hi = hi; // склеиваем соседние
        } else {
            out.push_back({lo, hi});
        }
    }
    fclose(f);
    std::sort(out.begin(), out.end(), [](const Range& a, const Range& b) { return a.lo < b.lo; });
    g_ranges.swap(out);
    g_ranges_ts = now_ms();
}

bool in_ranges(uintptr_t lo, uintptr_t hi) {
    std::lock_guard<std::mutex> lk(g_ranges_mu);
    if (g_ranges.empty() || now_ms() - g_ranges_ts > 2000) refresh_ranges_locked();
    auto it = std::upper_bound(g_ranges.begin(), g_ranges.end(), lo,
                               [](uintptr_t v, const Range& r) { return v < r.lo; });
    if (it == g_ranges.begin()) return false;
    --it;
    return lo >= it->lo && hi <= it->hi;
}

// адрес модуля по имени, строка берётся из pathname в maps
uintptr_t module_base(const char* name) {
    FILE* f = fopen("/proc/self/maps", "re");
    if (!f) return 0;
    char line[512];
    uintptr_t best = 0;
    while (fgets(line, sizeof(line), f)) {
        if (!strstr(line, name)) continue;
        uintptr_t lo = 0, hi = 0;
        unsigned long long fileoff = 0;
        char perms[8] = {0};
        if (sscanf(line, "%lx-%lx %7s %llx", &lo, &hi, perms, &fileoff) != 4) continue;
        if (fileoff != 0) continue;
        if (!best || lo < best) best = lo;
    }
    fclose(f);
    return best;
}

// POD-типы для ABI: HFA возврат/передача без сюрпризов
struct V3 { float x, y, z; };
struct V4 { float x, y, z, w; };
inline Vec3 to_vec(const V3& v) { return {v.x, v.y, v.z}; }
inline V3 to_v3(const Vec3& v) { return {v.x, v.y, v.z}; }

template <typename Fn>
inline Fn fn(uintptr_t rva) { return reinterpret_cast<Fn>(at(rva)); }

} // namespace

uintptr_t base() { return g_base; }

bool wait_for_module(int timeout_ms) {
    for (int waited = 0; waited <= timeout_ms; waited += 200) {
        g_base = module_base("libunity.so");
        if (g_base) {
            LOGI("libunity.so base = %p", (void*)g_base);
            return true;
        }
        usleep(200 * 1000);
    }
    return false;
}

bool readable(const void* p, size_t n) {
    uintptr_t a = (uintptr_t)p;
    if (a < 0x1000 || a > 0x7fffffffffull) return false;
    return in_ranges(a, a + n - 1);
}

const char* class_name(const void* obj) {
    void* klass = ptr(obj, off::il2::Object_klass);
    if (!readable(klass, 0x20)) return nullptr;
    const char* name = (const char*)ptr(klass, off::il2::Class_name);
    if (!readable(name, 2)) return nullptr;
    for (int i = 0; i < 64; i++) {
        if (!readable(name + i, 1)) return nullptr;
        char c = name[i];
        if (c == 0) return i ? name : nullptr;
        if ((unsigned char)c < 0x20 || (unsigned char)c > 0x7e) return nullptr;
    }
    return nullptr;
}

bool is_class(const void* obj, const char* want) {
    const char* n = class_name(obj);
    return n && std::strcmp(n, want) == 0;
}

void* camera_main() {
    return fn<void* (*)()>(off::rva::Camera_get_main)();
}

Vec3 world_to_screen(void* camera, const Vec3& world) {
    if (!camera) return {};
    V3 r = fn<V3 (*)(void*, V3)>(off::rva::Camera_WorldToScreenPoint)(camera, to_v3(world));
    return to_vec(r);
}

int camera_pixel_width(void* camera) {
    return camera ? fn<int (*)(void*)>(off::rva::Camera_get_pixelWidth)(camera) : 0;
}

int camera_pixel_height(void* camera) {
    return camera ? fn<int (*)(void*)>(off::rva::Camera_get_pixelHeight)(camera) : 0;
}

float camera_fov(void* camera) {
    return camera ? fn<float (*)(void*)>(off::rva::Camera_get_fieldOfView)(camera) : 60.f;
}

void* component_transform(void* component) {
    if (!readable(component, 0x40)) return nullptr;
    return fn<void* (*)(void*)>(off::rva::Component_get_transform)(component);
}

Vec3 transform_position(void* transform) {
    if (!readable(transform, 0x20)) return {};
    return to_vec(fn<V3 (*)(void*)>(off::rva::Transform_get_position)(transform));
}

Vec3 transform_forward(void* transform) {
    if (!readable(transform, 0x20)) return {};
    return to_vec(fn<V3 (*)(void*)>(off::rva::Transform_get_forward)(transform));
}

Vec3 transform_euler(void* transform) {
    if (!readable(transform, 0x20)) return {};
    return to_vec(fn<V3 (*)(void*)>(off::rva::Transform_get_eulerAngles)(transform));
}

void transform_set_euler(void* transform, const Vec3& euler) {
    if (!readable(transform, 0x20)) return;
    fn<void (*)(void*, V3)>(off::rva::Transform_set_eulerAngles)(transform, to_v3(euler));
}

bool linecast(const Vec3& from, const Vec3& to, RaycastHit* hit, int layer_mask) {
    RaycastHit tmp{};
    bool r = fn<bool (*)(V3, V3, RaycastHit*, int)>(off::rva::Physics_Linecast)(
        to_v3(from), to_v3(to), &tmp, layer_mask);
    if (hit) *hit = tmp;
    return r;
}

Vec3 player_position(void* player) {
    if (!readable(player, 0x98)) return {};
    return to_vec(fn<V3 (*)(void*)>(off::rva::Player_get_position)(player));
}

void transform_set_position(void* transform, const Vec3& pos) {
    if (!readable(transform, 0x20)) return;
    fn<void (*)(void*, V3)>(off::rva::Transform_set_position)(transform, to_v3(pos));
}

void* renderer_shared_material(void* renderer) {
    if (!readable(renderer, 0x30)) return nullptr;
    return fn<void* (*)(void*)>(off::rva::Renderer_get_sharedMaterial)(renderer);
}

void renderer_set_material(void* renderer, void* material) {
    if (!readable(renderer, 0x30) || !readable(material, 0x20)) return;
    fn<void (*)(void*, void*)>(off::rva::Renderer_set_material)(renderer, material);
}

void renderer_set_enabled(void* renderer, bool on) {
    if (!readable(renderer, 0x30)) return;
    fn<void (*)(void*, bool)>(off::rva::Renderer_set_enabled)(renderer, on);
}

int material_render_queue(void* material) {
    if (!readable(material, 0x20)) return -1;
    return fn<int (*)(void*)>(off::rva::Material_get_renderQueue)(material);
}

void material_set_render_queue(void* material, int queue) {
    if (!readable(material, 0x20)) return;
    fn<void (*)(void*, int)>(off::rva::Material_set_renderQueue)(material, queue);
}

bool unity_touch(Vec2* pos, int* phase) {
    int count = fn<int (*)()>(off::rva::Input_get_touchCount)();
    if (count <= 0) return false;
    uint8_t buf[off::touch_fld::size];
    std::memset(buf, 0, sizeof(buf));
    fn<void (*)(int, void*)>(off::rva::Input_GetTouch_Injected)(0, buf);
    V3 p{};
    std::memcpy(&p, buf + off::touch_fld::position, sizeof(float) * 2);
    if (pos) *pos = {p.x, p.y};
    if (phase) std::memcpy(phase, buf + off::touch_fld::phase, sizeof(int));
    return true;
}

bool unity_mouse(Vec2* pos, bool* down) {
    V3 p{};
    fn<void (*)(void*)>(off::rva::Input_mousePos_Injected)(&p);
    bool held = fn<bool (*)(int)>(off::rva::Input_GetMouseButton)(0);
    if (pos) *pos = {p.x, p.y};
    if (down) *down = held;
    return true;
}

int string_to_ascii(const void* str, char* out, int cap) {
    if (!out || cap <= 0) return 0;
    out[0] = 0;
    if (!readable(str, off::il2::String_chars + 2)) return 0;
    int len = read<int>(str, off::il2::String_length);
    if (len <= 0 || len > 4096) return 0;
    if (len > cap - 1) len = cap - 1;
    const uint16_t* src = (const uint16_t*)((const char*)str + off::il2::String_chars);
    if (!readable(src, sizeof(uint16_t) * (size_t)len)) return 0;
    for (int i = 0; i < len; i++) out[i] = (char)(src[i] & 0x7f);
    out[len] = 0;
    return len;
}

Quat player_rotation(void* player) {
    if (!readable(player, 0x98)) return {};
    V4 q = fn<V4 (*)(void*)>(off::rva::Player_get_rotation)(player);
    return {q.x, q.y, q.z, q.w};
}

} // namespace il2cpp
