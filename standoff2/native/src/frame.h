#pragma once
// Layout of the float block that the Unity thread publishes and the Java overlay draws. Mirrored by rage.so2.android.Frame.
namespace frame {

constexpr int kMaxPlayers = 16;
constexpr int kJoints = 16;
constexpr int kHeader = 24;
constexpr int kStride = 52;
constexpr int kTotal = kHeader + kMaxPlayers * kStride;

enum Header {
    H_SEQ = 0,        // frame counter (0 = nothing published yet)
    H_W = 1,          // camera pixel width
    H_H = 2,          // camera pixel height
    H_COUNT = 3,      // players in this frame
    H_FLAGS = 4,      // bit0 camera ok, bit1 tick hook alive, bit2 local char found, bit3 aim engaged, bit4 fire held
    H_FOVPX = 5,      // aim FOV circle radius, px
    H_CX = 6,         // crosshair x, px (Unity space, origin bottom-left)
    H_CY = 7,
    H_TARGET = 8,     // index of the aim target in the player list, -1 if none
    H_KILLSEQ = 9,    // increments on every detected kill
    H_KILLX = 10,
    H_KILLY = 11,
    H_KILLHS = 12,    // 1 if the victim was last seen aimed at the head
    H_LOCALTEAM = 13, // 0 unknown, 1 T, 2 CT
    H_WEAPON = 14,    // active weapon index (-1 unknown)
    H_GAINX = 15,
    H_GAINY = 16,
    H_LAG = 17,
    H_TICKMS = 18,    // native tick cost, ms
    H_CHARS = 19,     // characters tracked
    H_ZONES = 20,     // spawn zones found
};

enum Player {
    P_ID = 0, P_FLAGS = 1, P_DIST = 2, P_WEAPON = 3,
    P_BX1 = 4, P_BY1 = 5, P_BX2 = 6, P_BY2 = 7,
    P_HX = 8, P_HY = 9,
    P_J = 10,                 // 16 joints x (x,y); NaN when not projectable
    P_GX1 = 42, P_GY1 = 43, P_GX2 = 44, P_GY2 = 45,   // backtrack ghost box
    P_AX = 46, P_AY = 47,     // unit direction towards an off-screen player
    P_TEAM = 48,
    P_HP = 49,                // -1 (not readable from the Unity layer)
};

enum PlayerFlags {
    PF_ENEMY = 1, PF_VISIBLE = 2, PF_TARGET = 4, PF_BOX = 8, PF_TEAMKNOWN = 16, PF_GHOST = 32, PF_OFFSCREEN = 64,
};

enum HeaderFlags { HF_CAMERA = 1, HF_TICK = 2, HF_LOCAL = 4, HF_ENGAGED = 8, HF_FIRE = 16 };

}  // namespace frame
