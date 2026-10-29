#pragma once
#include <cstdint>

// бинд на опцию: 0 — всегда, иначе VK-код. mode: 0 — держать, 1 — переключать
struct Bind {
    int key = 0;
    int mode = 0;
    bool toggled = false;
};

struct Config {
    // RAGEBOT / aim
    bool aim = false;
    Bind aim_bind{0x02, 0};          // правая кнопка мыши
    float aim_fov = 5.f;
    int aim_bone = 6;                // head
    bool aim_visible_only = true;
    bool aim_silent = true;          // снап только на кадр выстрела, потом угол назад
    float aim_smooth = 1.f;          // 1 = снап (для silent всегда снап)
    bool aim_auto_fire = false;      // стрелять самому, когда цель в конусе
    bool aim_predict = true;         // упреждение по скорости цели
    float aim_predict_scale = 1.f;

    // TRIGGER
    bool trigger = false;
    Bind trigger_bind{0, 0};
    int trigger_delay_ms = 25;
    bool trigger_team_check = true;
    bool trigger_scoped_only = false;

    // MOVEMENT
    bool bhop = false;
    Bind bhop_bind{0x20, 0};         // по умолчанию пока зажат пробел

    // HIT / KILL эффекты
    bool hitmarker = true;
    bool hitmarker_sound = false;
    float hitmarker_size = 9.f;
    bool killeffect = true;

    // VISUALS / esp
    bool esp = true;
    bool esp_box = true;
    bool esp_skeleton = true;
    bool esp_health = true;
    bool esp_name = true;
    bool esp_dist = false;
    bool esp_weapon = false;
    bool esp_team_check = true;
    bool esp_visible_only = false;
    float esp_max_dist = 120.f;      // метры
    bool glow = false;
    int glow_type = 3;               // CGlowProperty::m_iGlowType
    float glow_color[3] = {0.24f, 0.49f, 1.f};
    float glow_color_visible[3] = {1.f, 0.35f, 0.75f};
    bool glow_split_visible = true;  // видимым один цвет, за стеной другой

    // WORLD
    bool night = false;              // затемнение карты
    float night_alpha = 0.55f;
    float night_tint[3] = {0.02f, 0.03f, 0.08f};
    bool bright_players = true;      // в темноте подсвечиваем игроков

    // SKINS (экспериментально)
    bool skins = false;
    int skin_paintkit = 38;          // Asiimov
    int skin_seed = 0;
    float skin_wear = 0.01f;
    int skin_stattrak = -1;

    // MISC
    bool watermark = true;
    bool draw_fov = false;
    bool debug = false;
    float menu_scale = 1.f;
    int menu_key = 0x2D;             // INSERT
};

extern Config g_cfg;

namespace config {
const char* path();
bool save();
bool load();
bool bind_active(Bind& b, bool base);   // учитывает режим бинда и состояние клавиши
const char* key_name(int vk);
} // namespace config
