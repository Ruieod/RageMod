#pragma once

namespace touch {

struct Event {
    enum Type { Down, Move, Up, MenuToggle } type;
    float x, y;
};

void start();                       // поднимает поток /dev/input, только если выбран этот источник
void feed(float x, float y, bool down); // ввод из игрового Input (без рута)
void set_screen(int w, int h);      // размер кадра, чтобы нормировать координаты
bool poll(Event* out);              // очередь в рендер-тред
bool available();                   // нашли ли тачскрин

} // namespace touch
