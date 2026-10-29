#pragma once

namespace mods {
bool install();          // ставит игровые хуки (патчит код игры — по кнопке из меню)
bool installed();        // уже стоят?
int installed_count();

// имена input-экшенов, которые игра реально запрашивала — чтобы подобрать экшен выстрела
int seen_action_count();
const char* seen_action(int i);
} // namespace mods
