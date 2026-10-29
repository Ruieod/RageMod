// Тонкая обёртка над схемой CS2. Все смещения — из src/offsets_gen.h,
// он генерится tools/update_offsets.py по cs2-dumper, вручную ничего не правим.
#pragma once
#include <cstdint>

#include "vmath.h"
#include "mem.h"
#include "offsets_gen.h"

namespace sdk {

extern uintptr_t g_client;  // client.dll
extern uintptr_t g_engine;  // engine2.dll

bool init();

// кости CS2 (индексы в массиве модели игрока)
enum Bone : int {
    BonePelvis = 0,
    BoneSpine1 = 4,
    BoneSpine2 = 5,
    BoneNeck = 5,
    BoneHead = 6,
    BoneLeftShoulder = 8,
    BoneLeftElbow = 9,
    BoneLeftHand = 10,
    BoneRightShoulder = 13,
    BoneRightElbow = 14,
    BoneRightHand = 15,
    BoneLeftHip = 22,
    BoneLeftKnee = 23,
    BoneLeftFoot = 24,
    BoneRightHip = 25,
    BoneRightKnee = 26,
    BoneRightFoot = 27,
};

struct Entity {
    uintptr_t ptr = 0;
    explicit operator bool() const { return ptr != 0; }

    uintptr_t scene_node() const { return mem::ptr(ptr + off::C_BaseEntity::m_pGameSceneNode); }
    bool dormant() const { return mem::read<bool>(scene_node() + off::CGameSceneNode::m_bDormant, true); }
    Vec3 origin() const { return mem::read<Vec3>(scene_node() + off::CGameSceneNode::m_vecAbsOrigin); }
    int health() const { return mem::read<int>(ptr + off::C_BaseEntity::m_iHealth); }
    uint8_t team() const { return mem::read<uint8_t>(ptr + off::C_BaseEntity::m_iTeamNum); }
    uint8_t life_state() const { return mem::read<uint8_t>(ptr + off::C_BaseEntity::m_lifeState, 1); }
    bool alive() const { return life_state() == 0 && health() > 0 && health() <= 100; }

    Vec3 eye() const {
        Vec3 o = origin();
        Vec3 v = mem::read<Vec3>(ptr + off::C_BaseModelEntity::m_vecViewOffset);
        return o + v;
    }

    uintptr_t bone_array() const {
        uintptr_t node = scene_node();
        if (!node) return 0;
        return mem::ptr(node + off::CSkeletonInstance::m_modelState + 0x80);
    }

    Vec3 bone(int index) const {
        uintptr_t arr = bone_array();
        if (!arr) return {};
        return mem::read<Vec3>(arr + (uintptr_t)index * 32);
    }

    uintptr_t active_weapon() const {
        uintptr_t ws = mem::ptr(ptr + off::C_BasePlayerPawn::m_pWeaponServices);
        if (!ws) return 0;
        uint32_t h = mem::read<uint32_t>(ws + off::CPlayer_WeaponServices::m_hActiveWeapon);
        return entity_by_handle(h);
    }

    static uintptr_t entity_by_index(int index);
    static uintptr_t entity_by_handle(uint32_t handle);
};

struct Controller {
    uintptr_t ptr = 0;
    explicit operator bool() const { return ptr != 0; }
    uint32_t pawn_handle() const { return mem::read<uint32_t>(ptr + off::CCSPlayerController::m_hPlayerPawn); }
    bool pawn_alive() const { return mem::read<bool>(ptr + off::CCSPlayerController::m_bPawnIsAlive); }
    uint8_t team() const { return mem::read<uint8_t>(ptr + off::C_BaseEntity::m_iTeamNum); }
    void name(char* out, int cap) const;
};

uintptr_t entity_list();
int highest_index();
Controller local_controller();
Entity local_pawn();
Vec3 view_angles();
void set_view_angles(const Vec3& a);
bool world_to_screen(const Vec3& world, Vec2* out, int screen_w, int screen_h);
void press_attack(bool down);

} // namespace sdk
