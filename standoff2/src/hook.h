#pragma once
#include <cstdint>

namespace hook {

// inline-хук arm64: 16-байтный патч LDR X16,#8 / BR X16 / .quad detour
// пролог цели копируется в трамплин как есть, поэтому PC-зависимые инструкции в первых
// 16 байтах не поддерживаются — такие цели отбрасываем вместо тихой порчи кода.
bool inlined(void* target, void* detour, void** orig);

// подмена записи в .rela.plt/.rela.dyn загруженной библиотеки (для импортов вроде eglSwapBuffers)
bool got(uintptr_t module_base, const char* symbol, void* detour, void** orig);

} // namespace hook
