#pragma once
#include <cmath>

struct Vec2 { float x = 0, y = 0; };

struct Vec3 {
    float x = 0, y = 0, z = 0;
    Vec3 operator-(const Vec3& o) const { return {x - o.x, y - o.y, z - o.z}; }
    Vec3 operator+(const Vec3& o) const { return {x + o.x, y + o.y, z + o.z}; }
    Vec3 operator*(float s) const { return {x * s, y * s, z * s}; }
    float len() const { return std::sqrt(x * x + y * y + z * z); }
    float len2d() const { return std::sqrt(x * x + y * y); }
    bool bad() const { return !std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z); }
};

inline float norm_angle(float a) {
    while (a > 180.f) a -= 360.f;
    while (a < -180.f) a += 360.f;
    return a;
}

// углы Source: x = pitch (вниз положительный), y = yaw
inline Vec3 calc_angle(const Vec3& from, const Vec3& to) {
    Vec3 d = to - from;
    float yaw = std::atan2(d.y, d.x) * 57.2957795f;
    float pitch = -std::atan2(d.z, d.len2d()) * 57.2957795f;
    return {norm_angle(pitch), norm_angle(yaw), 0.f};
}

inline float angle_diff(const Vec3& a, const Vec3& b) {
    float dp = norm_angle(a.x - b.x);
    float dy = norm_angle(a.y - b.y);
    return std::sqrt(dp * dp + dy * dy);
}
