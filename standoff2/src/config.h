#pragma once
#include <cstdint>

// одно место на все настройки; читается из рендер-треда и игрового тика без лока
// (поля независимые, рваные чтения безопасны)
struct Config {
    // ESP
    bool esp = true;
    bool esp_box = true;
    bool esp_bones = true;
    bool esp_health = true;
    bool esp_dist = true;
    bool esp_snaplines = false;
    bool esp_team_check = true;      // рисовать только врагов
    bool esp_visible_only = false;
    float esp_max_dist = 120.f;

    // AIM
    bool aim_silent = false;         // сайлент: перехват хит-каста, камера не двигается
    int  aim_silent_mode = 1;        // 0 = поворот WeaponContext.HitTransform, 1 = подмена лучей
    bool aim_visible = false;        // обычный аим: пишем сетевые кватернионы eir
    float aim_smooth = 6.f;          // 1 = снап, больше = плавнее (только visible)
    int  aim_bone = 10;              // HumanBodyBones.Head
    bool aim_fallback_body = true;   // если целевой бон не найден — центр коллайдеров
    float aim_fov = 8.f;             // градусы от центра экрана
    float aim_max_dist = 150.f;
    bool aim_visible_check = true;   // линкаст камера -> бон
    bool aim_team_check = true;
    int  aim_quat = 2;               // 1 = quat A, 2 = quat B, 3 = оба (visible aim)

    // MISC
    bool draw_fov = false;
    bool draw_debug = false;

    // ввод (ориентация тачскрина относительно landscape-кадра)
    bool touch_swap_xy = true;
    bool touch_invert_x = false;
    bool touch_invert_y = true;
};

extern Config g_cfg;

namespace config {
const char* path();
bool save();
bool load();
} // namespace config
