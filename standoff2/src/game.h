#pragma once
#include <cstdint>

#include "il2cpp.h"

namespace game {

constexpr int kMaxBones = 26;
constexpr int kMaxPlayers = 32;

struct BoneView {
    int bone = -1;
    Vec3 world;
    Vec2 screen;
    bool on_screen = false;
};

struct PlayerView {
    void* ent = nullptr;
    Vec3 origin;
    Vec3 head;
    float dist = 0.f;
    int hp = 0, maxhp = 100, armor = 0;
    uint8_t team = 0;
    bool enemy = false;
    bool visible = false;
    bool on_screen = false;
    Vec2 feet, top;       // экранные опорные точки бокса
    float box_w = 0.f;
    int bone_count = 0;
    BoneView bones[kMaxBones];
};

struct Frame {
    int64_t ts = 0;
    int screen_w = 0, screen_h = 0;
    float fov = 60.f;
    Vec3 cam_pos, cam_fwd;
    uint8_t local_team = 0;
    int count = 0;
    PlayerView players[kMaxPlayers];
    bool has_target = false;
    Vec2 target_screen;
    int entities_seen = 0;
};

void tick(void* camera_part);          // хук ios.lxcq, до оригинала
void post_tick(void* camera_part);     // тот же хук, после оригинала: третье лицо
void note_player(void* ent);           // хук ake.opmn, страховка на случай сломанного реестра
void copy_frame(Frame* out);           // снимок для рендера

// цель аима в текущем кадре
struct AimTarget {
    bool ok = false;
    void* ent = nullptr;
    Vec3 point;
    float angle = 0.f;
};
AimTarget current_target();
bool trigger_ready();                  // цель в конусе автострельбы
Vec3 aim_point(void* ent, bool* ok);   // пересчёт точки прицеливания по конфигу

} // namespace game
