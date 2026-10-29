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

// log: logcat + in-memory ring (shown in the debug tab)
void rlog(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
std::string logTail(int maxLines);
#define LOGI(...) rlog(__VA_ARGS__)

double nowSec();   // monotonic seconds
void testClock(bool enable, double t);   // host tests drive time by hand
