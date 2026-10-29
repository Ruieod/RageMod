#pragma once
// Shared state of the native game layer (game.cpp = tracking / ESP / aim, visuals.cpp = chams / world effects).
#include <deque>
#include <memory>
#include <string>
#include <vector>

#include "cfg_gen.h"
#include "common.h"
#include "frame.h"
#include "il2cpp.h"

namespace game {

struct Hist { double t; Vec3 head, foot; };

struct Char {
    il2::Ref bind, tf, root, anim, rend, tool;
    il2::Ref joint[frame::kJoints];
    int id = 0;
    bool biped = false;
    bool local = false;
    bool teammate = false;
    bool visible = false, headVis = false, chestVis = false;
    int team = 0;                       // 0 unknown, 1 T, 2 CT
    Vec3 pos, prevPos, head, foot;
    Vec3 jw[frame::kJoints];
    bool jwv[frame::kJoints] = {};
    bool valid = false;
    float dist = 0;
    double firstSeen = 0, lastSeen = 0, lastVisCheck = 0, toolT = -10;
    std::string toolName;
    int weapon = -1;
    std::deque<Hist> hist;
    // projected data for this frame
    bool onScreen = false;
    Vec2 sHead;                         // y-down camera pixels
    Vec2 sBox1, sBox2;
    int slot = -1;                      // index in the published frame
    // chams
    il2::Ref origMats;
    int chamsState = 0;                 // 0 untouched, 1 visible tint, 2 hidden tint
    double chamsCheck = 0;
    Vec3 lastHead;                      // for kill FX
    Vec2 lastHeadScreen;
    double lastSeenOnScreen = 0;
};

struct World {
    std::vector<std::unique_ptr<Char>> chars;
    void* cam = nullptr;
    Vec3 camPos, camFwd;
    int camW = 0, camH = 0;
    float vfov = 60.f;
    int frameNo = 0;
    double now = 0;
    float dt = 1.f / 60.f;
    float cfg[cfg::kTotal] = {};
    int curWeapon = -1;
    int localTeam = 0;
    Char* local = nullptr;
};

extern World W;

// effective setting: weapon override if the active weapon has one, otherwise global
float E(int slot);
inline bool EB(int slot) { return E(slot) > 0.5f; }
inline float G(int slot) { return W.cfg[slot]; }
inline bool GB(int slot) { return W.cfg[slot] > 0.5f; }

Color colorAt(int slot);

// visuals.cpp
bool visualsInit();
void visualsTick();
std::string visualsDescribe();

}  // namespace game
