// Внутренний чит: мы в адресном пространстве cs2.exe, читаем напрямую,
// но через обёртки с проверкой указателя — схема Valve меняется каждый патч,
// и лучше промахнуться мимо фичи, чем уронить игру.
#pragma once
#include <windows.h>

#include <cstdint>
#include <cstring>

namespace mem {

inline bool ok(const void* p, size_t n = 8) {
    if (!p) return false;
    uintptr_t a = (uintptr_t)p;
    if (a < 0x10000 || a > 0x7FFFFFFFFFFFull) return false;
    MEMORY_BASIC_INFORMATION mbi{};
    if (!VirtualQuery(p, &mbi, sizeof(mbi))) return false;
    if (mbi.State != MEM_COMMIT) return false;
    if (mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD)) return false;
    return a + n <= (uintptr_t)mbi.BaseAddress + mbi.RegionSize;
}

template <typename T>
inline T read(uintptr_t addr, T def = T{}) {
    if (!ok((void*)addr, sizeof(T))) return def;
    T v;
    std::memcpy(&v, (void*)addr, sizeof(T));
    return v;
}

template <typename T>
inline void write(uintptr_t addr, const T& v) {
    if (!ok((void*)addr, sizeof(T))) return;
    std::memcpy((void*)addr, &v, sizeof(T));
}

inline uintptr_t ptr(uintptr_t addr) { return read<uintptr_t>(addr); }

// подмена записи в vtable: никакого инлайн-патча кода, значит нечего
// детектить сканом секций и не нужен дизассемблер длин
inline void* vmt_swap(void** vtable, int index, void* hook) {
    DWORD old = 0;
    if (!VirtualProtect(&vtable[index], sizeof(void*), PAGE_EXECUTE_READWRITE, &old)) return nullptr;
    void* orig = vtable[index];
    vtable[index] = hook;
    VirtualProtect(&vtable[index], sizeof(void*), old, &old);
    return orig;
}

} // namespace mem
