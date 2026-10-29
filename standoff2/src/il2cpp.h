#pragma once
#include <cmath>
#include <cstdint>
#include <cstring>

#include "offsets.h"

// ---- математика (layout как у UnityEngine.*) ----
struct Vec2 { float x = 0, y = 0; };
struct Vec3 {
    float x = 0, y = 0, z = 0;
    Vec3 operator-(const Vec3& o) const { return {x - o.x, y - o.y, z - o.z}; }
    Vec3 operator+(const Vec3& o) const { return {x + o.x, y + o.y, z + o.z}; }
    Vec3 operator*(float s) const { return {x * s, y * s, z * s}; }
    float len() const { return std::sqrt(x * x + y * y + z * z); }
    Vec3 norm() const { float l = len(); return l > 1e-6f ? Vec3{x / l, y / l, z / l} : Vec3{}; }
    float dot(const Vec3& o) const { return x * o.x + y * o.y + z * o.z; }
    bool bad() const { return !std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z); }
};
struct Quat { float x = 0, y = 0, z = 0, w = 1; };
struct Ray { Vec3 origin, direction; };
struct RaycastHit { Vec3 point; Vec3 normal; uint32_t faceId; float distance; Vec2 uv; int32_t collider; };

// углы Unity: x = pitch (вниз положительный), y = yaw, z = roll
Vec3 dir_to_euler(const Vec3& dir);
Quat euler_to_quat(const Vec3& e);
float angle_between(const Vec3& a, const Vec3& b);
float norm_angle(float a);

namespace il2cpp {

bool wait_for_module(int timeout_ms);         // ждём маппинг libunity.so
uintptr_t base();                             // load bias libunity.so
inline uintptr_t at(uintptr_t rva) { return base() + rva; }

// безопасное чтение чужой памяти: процесс свой, но указатели managed-объектов живут не вечно
bool readable(const void* p, size_t n);
template <typename T>
inline T read(const void* obj, uintptr_t offset, T def = T{}) {
    if (!obj) return def;
    const void* p = (const char*)obj + offset;
    if (!readable(p, sizeof(T))) return def;
    T v;
    std::memcpy(&v, p, sizeof(T));
    return v;
}
inline void* ptr(const void* obj, uintptr_t offset) { return read<void*>(obj, offset, nullptr); }

const char* class_name(const void* obj);      // obj->klass->name, nullptr если мусор
bool is_class(const void* obj, const char* name);

// ---- managed-вызовы (il2cpp методы — обычные функции, MethodInfo* последний арг и игнорируется) ----
void* camera_main();
Vec3 world_to_screen(void* camera, const Vec3& world);
int camera_pixel_width(void* camera);
int camera_pixel_height(void* camera);
float camera_fov(void* camera);
void* component_transform(void* component);
Vec3 transform_position(void* transform);
Vec3 transform_forward(void* transform);
Vec3 transform_euler(void* transform);
void transform_set_euler(void* transform, const Vec3& euler);
bool linecast(const Vec3& from, const Vec3& to, RaycastHit* hit, int layer_mask);
Vec3 player_position(void* player);
Quat player_rotation(void* player);

} // namespace il2cpp
