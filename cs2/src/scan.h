// Рантайм-поиск кода в client.dll. Статические RVA из cs2-dumper живут
// ровно до следующего патча, сигнатуры — дольше, а xref по строке переживает
// вообще всё, пока Valve не переименует строку.
#pragma once
#include <cstdint>
#include <cstddef>

namespace scan {

struct Section {
    uintptr_t base = 0;
    size_t size = 0;
    explicit operator bool() const { return base && size; }
};

Section section(uintptr_t module, const char* name);   // ".text", ".rdata"

// IDA-формат: "48 8B 05 ? ? ? ? 48 85 C0", '?' и "??" — любой байт
uintptr_t pattern(uintptr_t module, const char* sig, const char* sect = ".text");

// абсолютный адрес из rip-relative операнда:
//   at      — адрес инструкции
//   disp_at — смещение поля disp32 внутри неё
//   len     — полная длина инструкции
uintptr_t rel(uintptr_t at, int disp_at, int len);

// n-я ссылка на строковый литерал: ищем строку в .rdata, затем lea reg,[rip+x]
// в .text, который на неё указывает. Переживает смену компилятора и порядка байт.
uintptr_t string_ref(uintptr_t module, const char* literal, int nth = 0);

// назад до пролога функции: через int3-паддинг или типовое начало
uintptr_t func_start(uintptr_t inside, size_t max_back = 0x400);

// дамп vtable объекта в текст: индекс, абсолютный адрес, модуль+RVA.
// Нужен, чтобы найти индекс GetUserCmd без дизассемблера на телефоне.
int dump_vtable(const void* instance, int count, const char* file);

} // namespace scan
