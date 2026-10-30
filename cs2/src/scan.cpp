#include "scan.h"

#include <windows.h>
#include <psapi.h>

#include <cstdio>
#include <cstring>

#include "mem.h"

namespace scan {
namespace {

bool parse_sig(const char* sig, uint8_t* bytes, bool* mask, int cap, int* out_n) {
    int n = 0;
    for (const char* p = sig; *p && n < cap;) {
        if (*p == ' ') { p++; continue; }
        if (*p == '?') {
            mask[n] = false;
            bytes[n] = 0;
            n++;
            p++;
            if (*p == '?') p++;
            continue;
        }
        unsigned v = 0;
        int got = 0;
        if (sscanf(p, "%2x%n", &v, &got) != 1 || got <= 0) return false;
        mask[n] = true;
        bytes[n] = (uint8_t)v;
        n++;
        p += got;
    }
    *out_n = n;
    return n > 0;
}

} // namespace

Section section(uintptr_t module, const char* name) {
    if (!module) return {};
    auto* dos = (IMAGE_DOS_HEADER*)module;
    if (!mem::ok(dos, sizeof(*dos)) || dos->e_magic != IMAGE_DOS_SIGNATURE) return {};
    auto* nt = (IMAGE_NT_HEADERS64*)(module + dos->e_lfanew);
    if (!mem::ok(nt, sizeof(*nt)) || nt->Signature != IMAGE_NT_SIGNATURE) return {};
    auto* sec = IMAGE_FIRST_SECTION(nt);
    for (int i = 0; i < nt->FileHeader.NumberOfSections; i++, sec++) {
        char nm[9] = {0};
        std::memcpy(nm, sec->Name, 8);
        if (std::strcmp(nm, name) == 0) {
            size_t sz = sec->Misc.VirtualSize ? sec->Misc.VirtualSize : sec->SizeOfRawData;
            return {module + sec->VirtualAddress, sz};
        }
    }
    return {};
}

uintptr_t pattern(uintptr_t module, const char* sig, const char* sect) {
    Section s = section(module, sect);
    if (!s) return 0;

    uint8_t bytes[256];
    bool mask[256];
    int n = 0;
    if (!parse_sig(sig, bytes, mask, 256, &n)) return 0;
    if ((size_t)n > s.size) return 0;

    const uint8_t* data = (const uint8_t*)s.base;
    const size_t last = s.size - (size_t)n;
    for (size_t i = 0; i <= last; i++) {
        int k = 0;
        for (; k < n; k++) {
            if (mask[k] && data[i + k] != bytes[k]) break;
        }
        if (k == n) return s.base + i;
    }
    return 0;
}

uintptr_t rel(uintptr_t at, int disp_at, int len) {
    if (!at) return 0;
    int32_t disp = mem::read<int32_t>(at + (uintptr_t)disp_at);
    if (!disp) return 0;
    return at + (uintptr_t)len + (intptr_t)disp;
}

uintptr_t string_ref(uintptr_t module, const char* literal, int nth) {
    Section rdata = section(module, ".rdata");
    Section text = section(module, ".text");
    if (!rdata || !text || !literal) return 0;

    const size_t len = std::strlen(literal);
    if (!len || len > rdata.size) return 0;

    // 1) адрес самой строки
    uintptr_t str = 0;
    const char* rd = (const char*)rdata.base;
    for (size_t i = 0; i + len + 1 <= rdata.size; i++) {
        if (rd[i] != literal[0]) continue;
        if (std::memcmp(rd + i, literal, len + 1) == 0) {   // вместе с нулём
            str = rdata.base + i;
            break;
        }
    }
    if (!str) return 0;

    // 2) lea reg,[rip+disp] на неё. 48 8D xx disp32 (длина 7) и 4C 8D xx (r8-r15)
    const uint8_t* tx = (const uint8_t*)text.base;
    int hits = 0;
    for (size_t i = 0; i + 7 <= text.size; i++) {
        if ((tx[i] != 0x48 && tx[i] != 0x4C) || tx[i + 1] != 0x8D) continue;
        if ((tx[i + 2] & 0xC7) != 0x05) continue;           // mod=00, rm=101 -> rip-relative
        int32_t disp;
        std::memcpy(&disp, tx + i + 3, 4);
        if (text.base + i + 7 + (intptr_t)disp != str) continue;
        if (hits++ == nth) return text.base + i;
    }
    return 0;
}

uintptr_t func_start(uintptr_t inside, size_t max_back) {
    if (!inside) return 0;
    for (size_t back = 0; back < max_back; back++) {
        uintptr_t a = inside - back;
        if (!mem::ok((void*)(a - 2), 8)) break;
        const uint8_t* p = (const uint8_t*)a;
        // паддинг перед функцией: cc cc / 90 90, дальше уже тело
        if (p[-1] == 0xCC && p[-2] == 0xCC) return a;
        if (p[-1] == 0x90 && p[-2] == 0x90) return a;
        // типовые прологи x64
        if (p[0] == 0x48 && p[1] == 0x89 && p[2] == 0x5C && p[3] == 0x24) return a;
        if (p[0] == 0x40 && p[1] == 0x55) return a;
        if (p[0] == 0x48 && p[1] == 0x8B && p[2] == 0xC4) return a;
    }
    return 0;
}

int dump_vtable(const void* instance, int count, const char* file) {
    if (!mem::ok(instance, 8)) return 0;
    void** vt = *(void***)instance;
    if (!mem::ok(vt, 8)) return 0;

    FILE* f = std::fopen(file, "w");
    if (!f) return 0;

    HMODULE mods[64];
    DWORD need = 0;
    char names[64][64];
    int nmods = 0;
    if (EnumProcessModules(GetCurrentProcess(), mods, sizeof(mods), &need)) {
        nmods = (int)(need / sizeof(HMODULE));
        if (nmods > 64) nmods = 64;
        for (int i = 0; i < nmods; i++) {
            char path[MAX_PATH] = {0};
            GetModuleBaseNameA(GetCurrentProcess(), mods[i], path, MAX_PATH);
            std::snprintf(names[i], sizeof(names[i]), "%s", path);
        }
    }

    std::fprintf(f, "vtable @ %p (instance %p)\n", (void*)vt, instance);
    int written = 0;
    for (int i = 0; i < count; i++) {
        if (!mem::ok(&vt[i], 8)) break;
        void* fn = vt[i];
        if (!mem::ok(fn, 1)) break;
        const char* owner = "?";
        uintptr_t rva = 0;
        for (int m = 0; m < nmods; m++) {
            MODULEINFO mi{};
            if (!GetModuleInformation(GetCurrentProcess(), mods[m], &mi, sizeof(mi))) continue;
            uintptr_t b = (uintptr_t)mi.lpBaseOfDll;
            if ((uintptr_t)fn >= b && (uintptr_t)fn < b + mi.SizeOfImage) {
                owner = names[m];
                rva = (uintptr_t)fn - b;
                break;
            }
        }
        const uint8_t* b = (const uint8_t*)fn;
        std::fprintf(f, "[%3d] %p  %s+0x%llx  %02X %02X %02X %02X %02X %02X %02X %02X\n", i, fn, owner,
                     (unsigned long long)rva, b[0], b[1], b[2], b[3], b[4], b[5], b[6], b[7]);
        written++;
    }
    std::fclose(f);
    return written;
}

} // namespace scan
