#pragma once

namespace touch {

struct Event {
    enum Type { Down, Move, Up, MenuToggle } type;
    float x, y;
};

void start();                       // поток чтения /dev/input (нужен root)
void set_screen(int w, int h);      // размер кадра, чтобы нормировать координаты
bool poll(Event* out);              // очередь в рендер-тред
bool available();                   // нашли ли тачскрин

} // namespace touch
