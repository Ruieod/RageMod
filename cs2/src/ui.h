#pragma once

struct Bind;   // из config.h; объявляем до namespace, иначе родится ui::Bind

namespace ui {
void init_style(float display_h);       // стиль + шрифт под текущее разрешение
void frame(int w, int h, float fps);    // рисует меню, ватермарку и esp
bool menu_open();
void toggle_menu();
bool capturing_input();                 // меню забирает мышь — игре её не отдаём
#ifdef RM_PREVIEW
void preview_set_tab(int tab);          // только для хостового превью вёрстки
void preview_wait_bind(::Bind* b); // показать строку в режиме «ждём клавишу»
#endif
} // namespace ui
