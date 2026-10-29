#pragma once
// Shared small types + logging. Kept free of android headers so the logic files build on the host for tests.
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cmath>
#include <string>

struct Vec2 { float x = 0, y = 0; };
struct Vec3 { float x = 0, y = 0, z = 0; };
struct Quat { float x = 0, y = 0, z = 0, w = 1; };
struct Color { float r = 1, g = 1, b = 1, a = 1; };
struct Bounds { Vec3 center, extents; };

inline Vec3 operator+(Vec3 a, Vec3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
inline Vec3 operator-(Vec3 a, Vec3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
inline Vec3 operator*(Vec3 a, float s) { return {a.x * s, a.y * s, a.z * s}; }
inline float dot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline float len(Vec3 a) { return std::sqrt(dot(a, a)); }
inline Vec3 lerp(Vec3 a, Vec3 b, float t) { return a + (b - a) * t; }
inline float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

inline float qdot(Quat a, Quat b) { return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w; }
inline bool qsame(Quat a, Quat b) {
    return std::fabs(a.x - b.x) < 1e-6f && std::fabs(a.y - b.y) < 1e-6f && std::fabs(a.z - b.z) < 1e-6f && std::fabs(a.w - b.w) < 1e-6f;
}
// normalized lerp along the shorter arc: t = 0 -> a, t = 1 -> b
inline Quat nlerp(Quat a, Quat b, float t) {
    const float s = qdot(a, b) < 0 ? -1.f : 1.f;
    Quat q{a.x + (s * b.x - a.x) * t, a.y + (s * b.y - a.y) * t, a.z + (s * b.z - a.z) * t, a.w + (s * b.w - a.w) * t};
    const float l = std::sqrt(qdot(q, q));
    if (l < 1e-6f) return b;
    return {q.x / l, q.y / l, q.z / l, q.w / l};
}

// log: logcat + in-memory ring (shown in the debug tab)
void rlog(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
std::string logTail(int maxLines);
#define LOGI(...) rlog(__VA_ARGS__)

double nowSec();   // monotonic seconds
void testClock(bool enable, double t);   // host tests drive time by hand
