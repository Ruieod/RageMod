#include "hook.h"

#include <elf.h>
#include <sys/mman.h>
#include <unistd.h>

#include <cstring>

#include "log.h"

namespace hook {
namespace {

constexpr uint32_t kLdrX16 = 0x58000050; // ldr x16, #8
constexpr uint32_t kBrX16  = 0xD61F0200; // br x16

bool pc_relative(uint32_t w) {
    if ((w & 0x9F000000) == 0x10000000) return true; // adr
    if ((w & 0x9F000000) == 0x90000000) return true; // adrp
    if ((w & 0xFC000000) == 0x14000000) return true; // b
    if ((w & 0xFC000000) == 0x94000000) return true; // bl
    if ((w & 0xFF000010) == 0x54000000) return true; // b.cond
    if ((w & 0x7E000000) == 0x34000000) return true; // cbz/cbnz
    if ((w & 0x7E000000) == 0x36000000) return true; // tbz/tbnz
    if ((w & 0x3B000000) == 0x18000000) return true; // ldr literal (gp)
    if ((w & 0x3F000000) == 0x1C000000) return true; // ldr literal (simd)
    return false;
}

bool unprotect(void* addr, size_t len) {
    size_t ps = (size_t)getpagesize();
    uintptr_t start = (uintptr_t)addr & ~(uintptr_t)(ps - 1);
    uintptr_t end = ((uintptr_t)addr + len + ps - 1) & ~(uintptr_t)(ps - 1);
    return mprotect((void*)start, end - start, PROT_READ | PROT_WRITE | PROT_EXEC) == 0;
}

void reprotect_exec(void* addr, size_t len) {
    size_t ps = (size_t)getpagesize();
    uintptr_t start = (uintptr_t)addr & ~(uintptr_t)(ps - 1);
    uintptr_t end = ((uintptr_t)addr + len + ps - 1) & ~(uintptr_t)(ps - 1);
    mprotect((void*)start, end - start, PROT_READ | PROT_EXEC);
}

void* alloc_trampoline() {
    void* p = mmap(nullptr, (size_t)getpagesize(), PROT_READ | PROT_WRITE | PROT_EXEC,
                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    return p == MAP_FAILED ? nullptr : p;
}

uintptr_t fix_dyn_ptr(uintptr_t base, uintptr_t v) {
    // DT_* указатели у разных линкеров приходят то абсолютными, то link-time адресами
    return v < base ? base + v : v;
}

} // namespace

bool inlined(void* target, void* detour, void** orig) {
    if (!target || !detour) return false;
    uint32_t* code = (uint32_t*)target;

    for (int i = 0; i < 4; i++) {
        if (pc_relative(code[i])) {
            LOGE("hook: %p instr %d (%08x) is pc-relative, skipping", target, i, code[i]);
            return false;
        }
    }

    uint8_t* tr = (uint8_t*)alloc_trampoline();
    if (!tr) return false;

    std::memcpy(tr, code, 16);
    uint32_t back[2] = {kLdrX16, kBrX16};
    std::memcpy(tr + 16, back, sizeof(back));
    uint64_t resume = (uint64_t)target + 16;
    std::memcpy(tr + 24, &resume, sizeof(resume));
    __builtin___clear_cache((char*)tr, (char*)tr + 32);

    if (!unprotect(target, 16)) {
        LOGE("hook: mprotect failed for %p", target);
        return false;
    }
    uint32_t patch[2] = {kLdrX16, kBrX16};
    uint64_t dst = (uint64_t)detour;
    std::memcpy(code, patch, sizeof(patch));
    std::memcpy(code + 2, &dst, sizeof(dst));
    __builtin___clear_cache((char*)target, (char*)target + 16);
    reprotect_exec(target, 16);

    if (orig) *orig = tr;
    LOGI("hook: %p -> %p (orig %p)", target, detour, tr);
    return true;
}

bool got(uintptr_t module_base, const char* symbol, void* detour, void** orig) {
    if (!module_base || !symbol || !detour) return false;
    auto* ehdr = (Elf64_Ehdr*)module_base;
    if (std::memcmp(ehdr->e_ident, ELFMAG, SELFMAG) != 0) return false;

    auto* phdr = (Elf64_Phdr*)(module_base + ehdr->e_phoff);
    Elf64_Dyn* dyn = nullptr;
    for (int i = 0; i < ehdr->e_phnum; i++) {
        if (phdr[i].p_type == PT_DYNAMIC) {
            dyn = (Elf64_Dyn*)(module_base + phdr[i].p_vaddr);
            break;
        }
    }
    if (!dyn) return false;

    const char* strtab = nullptr;
    Elf64_Sym* symtab = nullptr;
    Elf64_Rela* jmprel = nullptr;
    Elf64_Rela* rela = nullptr;
    size_t jmprel_sz = 0, rela_sz = 0;
    for (Elf64_Dyn* d = dyn; d->d_tag != DT_NULL; d++) {
        switch (d->d_tag) {
            case DT_STRTAB: strtab = (const char*)fix_dyn_ptr(module_base, d->d_un.d_ptr); break;
            case DT_SYMTAB: symtab = (Elf64_Sym*)fix_dyn_ptr(module_base, d->d_un.d_ptr); break;
            case DT_JMPREL: jmprel = (Elf64_Rela*)fix_dyn_ptr(module_base, d->d_un.d_ptr); break;
            case DT_PLTRELSZ: jmprel_sz = d->d_un.d_val; break;
            case DT_RELA: rela = (Elf64_Rela*)fix_dyn_ptr(module_base, d->d_un.d_ptr); break;
            case DT_RELASZ: rela_sz = d->d_un.d_val; break;
            default: break;
        }
    }
    if (!strtab || !symtab) return false;

    auto scan = [&](Elf64_Rela* r, size_t bytes) -> bool {
        if (!r || !bytes) return false;
        for (size_t i = 0; i < bytes / sizeof(Elf64_Rela); i++) {
            uint32_t sym = (uint32_t)ELF64_R_SYM(r[i].r_info);
            if (!sym) continue;
            const char* name = strtab + symtab[sym].st_name;
            if (std::strcmp(name, symbol) != 0) continue;
            void** slot = (void**)(module_base + r[i].r_offset);
            if (!unprotect(slot, sizeof(void*))) return false;
            if (orig) *orig = *slot;
            *slot = detour;
            LOGI("hook: got %s slot %p -> %p", symbol, (void*)slot, detour);
            return true;
        }
        return false;
    };

    return scan(jmprel, jmprel_sz) || scan(rela, rela_sz);
}

} // namespace hook
