#pragma once

namespace ui {
bool install();      // хук eglSwapBuffers через GOT libunity.so
bool menu_open();
bool wants_input();  // палец на меню — игровой ввод надо глушить
} // namespace ui
