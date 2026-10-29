#pragma once

#ifdef RM_PREVIEW
#include "game.h"
#endif

namespace ui {
#ifdef RM_PREVIEW
void preview_init(float display_h);
void preview_frame(int tab, bool menu, const game::Frame& f);
#endif
bool install();      // хук eglSwapBuffers через GOT libunity.so
bool menu_open();
bool wants_input();  // палец на меню — игровой ввод надо глушить
} // namespace ui
