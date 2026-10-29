#pragma once
#include <cstdint>

#include "vmath.h"

namespace game {

constexpr int kMaxPlayers = 64;
constexpr int kMaxBones = 18;

struct BoneView {
    int bone = -1;
    Vec2 screen;
    bool on = false;
};

struct PlayerView {
    uintptr_t pawn = 0;
    bool enemy = false;
    bool visible = false;
    bool on_screen = false;
    int hp = 0, armor = 0;
    float dist = 0.f;
    Vec2 head, feet;
    float box_w = 0.f;
    char name[32] = {0};
    int bone_count = 0;
    BoneView bones[kMaxBones];
};

struct BoneLink {
    int a, b;
};
extern const BoneLink kBoneLinks[];
extern const int kBoneLinkCount;

// событие попадания/убийства для эффектов на экране
struct HitEvent {
    int64_t ts = 0;      // когда случилось
    int damage = 0;
    bool kill = false;
};

struct Frame {
    int count = 0;
    PlayerView players[kMaxPlayers];
    bool in_game = false;
    bool has_target = false;
    Vec2 target_screen;
    int screen_w = 0, screen_h = 0;
    float fps = 0.f;
    HitEvent last_hit;
};

void tick(int screen_w, int screen_h);   // игровая логика, зовётся из Present
const Frame& frame();                    // снимок для отрисовки

} // namespace game
